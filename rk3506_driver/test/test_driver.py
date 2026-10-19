"""Public ROS/UDP integration checks; no physical robot is contacted."""
import os
import secrets
import socket
import struct
import subprocess
import threading
import time

import pytest
import rclpy
from geometry_msgs.msg import Twist, TwistStamped
from nav_msgs.msg import Odometry
from std_srvs.srv import Trigger
from sensor_msgs.msg import BatteryState
from std_msgs.msg import UInt8
from diagnostic_msgs.msg import DiagnosticArray, DiagnosticStatus
from tf2_msgs.msg import TFMessage


class Controller:
    """An independent peer speaking the existing controller wire protocol."""

    def __init__(self):
        self.sockets = []
        for _ in range(3):
            sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            sock.bind(("127.0.0.1", 0))
            sock.setblocking(False)
            self.sockets.append(sock)
        self.ports = [sock.getsockname()[1] for sock in self.sockets]
        self.clients = [set(), set(), set()]
        self.subscriptions = [0, 0, 0]
        self.owner = 0
        self.owner_deadline = 0
        self.retired_sessions = set()
        self.sequence = 0
        self.flags = 4 | 16 | 32 | 256 | 8192
        self.commands = []
        self.online = True
        self.stop_result = 0
        self.sensor_packets = [None, struct.pack("!BHhHHHHBBBB3H", 0, 2673, -120, 1890, 3500,
                                                3, 0, 54, 3, 7, 3, 2981, 2982, 2983), b"\x00\x03"]
        self.running = True
        self.started = time.monotonic()
        self.thread = threading.Thread(target=self.run, daemon=True)
        self.thread.start()

    def run(self):
        next_sample = 0
        while self.running:
            now = time.monotonic()
            if self.owner and self.owner != 0xB0000001 and now >= self.owner_deadline:
                self.retired_sessions.add(self.owner)
                self.owner = 0
            for index, sock in enumerate(self.sockets):
                for _ in range(32):
                    try:
                        data, peer = sock.recvfrom(256)
                    except BlockingIOError:
                        break
                    if not self.online:
                        continue
                    if data in (b"\x01\x01", b"\x01\x03"):
                        self.clients[index].add(peer)
                        self.subscriptions[index] += 1
                    elif index == 0 and len(data) >= 6 and data[0] == 1:
                        op, session = data[1], struct.unpack_from("!I", data, 2)[0]
                        seq = struct.unpack_from("!I", data, 6)[0] if len(data) >= 10 else 0
                        result = 0
                        if op == 0x10:
                            if session in self.retired_sessions:
                                result = 1
                            elif self.owner not in (0, session):
                                result = 2
                            else:
                                self.owner = session
                        elif session != self.owner:
                            result = 2
                        elif op == 0x11 and len(data) == 16:
                            velocity = struct.unpack_from("!hhh", data, 10)
                            self.commands.append((now, velocity))
                        elif op == 0x15:
                            if self.stop_result is None:
                                continue
                            result = self.stop_result
                            if result == 0:
                                self.flags |= 1024
                        if result == 0:
                            self.owner_deadline = now + 1.5
                        sock.sendto(struct.pack("!BBIIB", 1, op | 0x80, session, seq, result), peer)
            if now >= next_sample and self.online:
                self.sequence = (self.sequence + 1) % 65536
                header = struct.pack("!BBHIB", 1, 3, self.sequence, int((now - self.started) * 1000), 0)
                payload = struct.pack("!H6h4h2ihII", self.flags, 0, 0, 0, 200, -100, 300,
                                      0, 0, 0, 0, 1250, -750, 500, self.owner, 0)
                for peer in list(self.clients[0]):
                    self.sockets[0].sendto(header + payload, peer)
                for index in (1, 2):
                    if self.sensor_packets[index] is not None:
                        for peer in list(self.clients[index]):
                            self.sockets[index].sendto(self.sensor_packets[index], peer)
                next_sample = now + 0.02
            time.sleep(0.002)

    def close(self):
        self.running = False
        self.thread.join(timeout=2)
        for sock in self.sockets:
            sock.close()


class Robot:
    def __init__(self, **parameters):
        self.controller = Controller()
        self.namespace = "sdk_test_" + secrets.token_hex(4)
        self.probe = rclpy.create_node("probe", namespace=self.namespace)
        self.odometry = []
        self.battery = []
        self.inputs = []
        self.diagnostics = []
        self.subscription = self.probe.create_subscription(
            Odometry, "wheel/odometry", self.odometry.append, 10)
        self.publisher = self.probe.create_publisher(Twist, "cmd_vel", 1)
        self.sensor_subscriptions = [
            self.probe.create_subscription(BatteryState, "battery_state", self.battery.append, 10),
            self.probe.create_subscription(UInt8, "digital_inputs", self.inputs.append, 10),
            self.probe.create_subscription(DiagnosticArray, "diagnostics", self.diagnostics.append, 10),
        ]
        executable = os.environ.get("RK3506_DRIVER_EXECUTABLE", "/nonexistent/rk3506_driver")
        params = dict(robot_ip="127.0.0.1", base_port=self.controller.ports[0],
                      battery_port=self.controller.ports[1], di_port=self.controller.ports[2])
        params.update(parameters)
        args = [executable, "--ros-args", "-r", "__ns:=/" + self.namespace, "-r", "/tf:=tf"]
        for name, value in params.items():
            args += ["-p", f"{name}:={str(value).lower() if isinstance(value, bool) else value}"]
        try:
            self.process = subprocess.Popen(args)
        except Exception:
            self.probe.destroy_node()
            self.controller.close()
            raise

    def wait(self, predicate, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            rclpy.spin_once(self.probe, timeout_sec=0.01)
            assert self.process.poll() is None, "driver exited before completing scenario"
            if predicate():
                return
        pytest.fail("public ROS/UDP behavior did not arrive before deadline")

    def command(self, vx=0.2, vy=-0.1, wz=0.3):
        message = Twist()
        message.linear.x, message.linear.y, message.angular.z = float(vx), float(vy), float(wz)
        self.publisher.publish(message)

    def close(self):
        self.process.terminate()
        try:
            self.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            self.process.kill()
            self.process.wait()
        self.probe.destroy_node()
        self.controller.close()


@pytest.fixture(scope="module", autouse=True)
def ros_context():
    rclpy.init()
    yield
    rclpy.shutdown()


@pytest.fixture
def robot():
    instance = Robot()
    yield instance
    instance.close()


def test_velocity_and_controller_wheel_odometry_round_trip(robot):
    robot.wait(lambda: robot.publisher.get_subscription_count() > 0 and robot.odometry)
    assert not robot.controller.commands, "startup must not request motion without a fresh command"
    robot.command()
    robot.wait(lambda: any(value == (200, -100, 300) for _, value in robot.controller.commands))
    odom = robot.odometry[-1]
    assert odom.pose.pose.position.x == pytest.approx(1.25)
    assert odom.pose.pose.position.y == pytest.approx(-0.75)
    assert odom.twist.twist.linear.x == pytest.approx(0.2)
    assert odom.header.frame_id == "odom"
    assert odom.child_frame_id == "base_link"
    robot.wait(lambda: robot.controller.commands[-1][1] == (0, 0, 0))


@pytest.mark.parametrize("result", [0, 4, None])
def test_software_stop_reports_controller_acknowledgement(robot, result):
    robot.wait(lambda: robot.publisher.get_subscription_count() > 0 and robot.odometry)
    robot.command()
    robot.wait(lambda: robot.controller.commands)
    robot.controller.stop_result = result
    client = robot.probe.create_client(Trigger, "stop")
    robot.wait(lambda: client.service_is_ready())
    future = client.call_async(Trigger.Request())
    robot.wait(future.done)
    assert future.result().success == (result == 0)
    assert future.result().message


def test_battery_inputs_and_invalid_measurements_are_observable(robot):
    robot.wait(lambda: robot.battery and robot.inputs and robot.diagnostics)
    battery = robot.battery[-1]
    assert battery.voltage == pytest.approx(26.73)
    assert battery.current == pytest.approx(-1.2)
    assert battery.charge == pytest.approx(18.9)
    assert battery.capacity == pytest.approx(35.0)
    assert battery.percentage == pytest.approx(0.54)
    assert battery.temperature == pytest.approx(25.1)
    assert robot.inputs[-1].data == 3
    robot.controller.sensor_packets[1] = b"\x01"
    robot.controller.sensor_packets[2] = b"\x01\x00"
    robot.wait(lambda: all(any(status.name.endswith('/' + name) and status.level == DiagnosticStatus.ERROR
                              for status in robot.diagnostics[-1].status)
                          for name in ('battery', 'digital_inputs')))
    counts = len(robot.battery), len(robot.inputs)
    robot.wait(lambda: len(robot.diagnostics) >= 4)
    assert (len(robot.battery), len(robot.inputs)) == counts


def test_browser_takeover_discards_commands_until_fresh_ros_resumption(robot):
    robot.wait(lambda: robot.publisher.get_subscription_count() > 0 and robot.odometry)
    robot.command()
    robot.wait(lambda: robot.controller.commands and robot.controller.commands[-1][1] == (200, -100, 300))
    robot.controller.retired_sessions.add(robot.controller.owner)
    robot.controller.owner = 0xB0000001
    robot.wait(lambda: robot.diagnostics and any(any(field.key == 'owner_session' and field.value == str(0xB0000001)
                              for field in status.values) for status in robot.diagnostics[-1].status))
    robot.command(0.9, 0, 0)
    robot.wait(lambda: len(robot.diagnostics) >= 3)
    robot.controller.owner = 0
    robot.wait(lambda: all(not any(field.key == 'owner_session' and field.value == str(0xB0000001)
                                  for field in status.values) for status in robot.diagnostics[-1].status))
    resumed_at = len(robot.controller.commands)
    robot.wait(lambda: len(robot.diagnostics) >= 5)
    assert not any(any(velocity) for _, velocity in robot.controller.commands[resumed_at:])
    robot.command(0.1, 0, 0)
    robot.wait(lambda: any(velocity == (100, 0, 0) for _, velocity in robot.controller.commands[resumed_at:]))


def test_stamped_input_and_optional_tf_use_configured_frames():
    instance = Robot(use_stamped_cmd_vel=True, publish_tf=True, base_frame="robot_body", odom_frame="wheel_origin")
    try:
        instance.probe.destroy_publisher(instance.publisher)
        publisher = instance.probe.create_publisher(TwistStamped, "cmd_vel", 1)
        transforms = []
        subscription = instance.probe.create_subscription(TFMessage, "tf", transforms.append, 10)
        instance.wait(lambda: publisher.get_subscription_count() == 1 and instance.odometry and transforms)
        assert {info.topic_type for info in instance.probe.get_subscriptions_info_by_topic('cmd_vel')} == {'geometry_msgs/msg/TwistStamped'}
        message = TwistStamped()
        message.header.frame_id = "robot_body"
        message.header.stamp = instance.probe.get_clock().now().to_msg()
        message.twist.linear.y = 0.15
        publisher.publish(message)
        instance.wait(lambda: any(value == (0, 150, 0) for _, value in instance.controller.commands))
        transform = transforms[-1].transforms[0]
        assert transform.header.frame_id == "wheel_origin"
        assert transform.child_frame_id == "robot_body"
        assert transform.transform.translation.x == pytest.approx(1.25)
        assert instance.odometry[-1].pose.covariance[0] > 0
        message.header.stamp.sec -= 1
        message.twist.linear.x = 0.9
        publisher.publish(message)
        instance.wait(lambda: instance.controller.commands[-1][1] == (0, 0, 0))
        assert not any(value[0] == 900 for _, value in instance.controller.commands)
        assert subscription
    finally:
        instance.close()


def test_connection_recovery_requires_new_command(robot):
    robot.wait(lambda: robot.publisher.get_subscription_count() > 0 and robot.odometry and
               robot.controller.subscriptions[0] >= 2)
    robot.command()
    robot.wait(lambda: robot.controller.commands)
    robot.controller.online = False
    robot.wait(lambda: robot.diagnostics and any(status.name.endswith('/controller') and
               status.level == DiagnosticStatus.STALE for status in robot.diagnostics[-1].status))
    robot.controller.clients = [set(), set(), set()]
    robot.controller.retired_sessions.add(robot.controller.owner)
    robot.controller.owner = 0
    robot.controller.online = True
    old_count = len(robot.controller.commands)
    robot.wait(lambda: robot.diagnostics and any(status.name.endswith('/controller') and
               status.level == DiagnosticStatus.OK for status in robot.diagnostics[-1].status), timeout=3)
    assert not any(any(value) for _, value in robot.controller.commands[old_count:])
    robot.command(0.12, 0, 0)
    robot.wait(lambda: any(value == (120, 0, 0) for _, value in robot.controller.commands[old_count:]))


def test_invalid_motion_invalid_odometry_and_default_tf(robot):
    transforms = []
    subscription = robot.probe.create_subscription(TFMessage, "tf", transforms.append, 10)
    robot.wait(lambda: robot.publisher.get_subscription_count() > 0 and robot.odometry)
    robot.command()
    robot.wait(lambda: robot.controller.commands and any(robot.controller.commands[-1][1]))
    robot.command(float('nan'), 0, 0)
    robot.wait(lambda: robot.controller.commands[-1][1] == (0, 0, 0))
    robot.command(2.0, 0, 0)
    robot.controller.flags &= ~(16 | 256)
    robot.wait(lambda: robot.diagnostics and any(status.name.endswith('/controller') and
               status.level == DiagnosticStatus.WARN for status in robot.diagnostics[-1].status))
    count = len(robot.odometry)
    target = len(robot.diagnostics) + 1
    robot.wait(lambda: len(robot.diagnostics) >= target)
    assert len(robot.odometry) == count
    assert not transforms
    assert not any(abs(value[0]) > 1000 for _, value in robot.controller.commands)
    assert subscription


@pytest.mark.parametrize('parameters', [{'command_timeout': 0.0}, {'base_port': 0},
                                       {'robot_ip': 'invalid'}, {'pose_covariance': [0.1]}])
def test_invalid_startup_configuration_exits_without_control(parameters):
    instance = Robot(**parameters)
    try:
        assert instance.process.wait(timeout=5) != 0
        assert not instance.controller.commands
    finally:
        instance.close()
