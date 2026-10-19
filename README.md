# RK3506 Mobile Base ROS 2 SDK

ROS 2 integration for the RK3506 Helishi two-swerve mobile robot controller.
The SDK runs on the upper computer and communicates with the standalone RK3506
controller over UDP.

The C++ package is `rk3506_driver`. Controller firmware remains in the separate
`rk3506-can-interface-cpp` repository. See [validation](docs/VALIDATION.md) for
executed checks and outstanding physical acceptance.

## Supported platforms

| ROS 2 distribution | Operating system | Architectures |
| --- | --- | --- |
| Humble | Ubuntu 22.04 | ARM64, x86_64 |
| Jazzy | Ubuntu 24.04 | ARM64, x86_64 |

All four combinations passed build and integration checks. ARM64 checks ran
natively in Docker; x86_64 checks used emulation. Physical robot acceptance
remains outstanding.

## Requirements

- An upper computer with the appropriate ROS 2 installation, a C++17 compiler,
  `colcon` and configured `rosdep`.
- Network access to the controller's UDP ports: `5003` for base commands/status,
  `5001` for battery and `5002` for digital inputs. Allow the corresponding UDP
  replies to the upper computer.
- The companion controller access/handoff update described in
  [controller integration](docs/CONTROLLER_INTEGRATION.md).

Configure the controller's `--navigation-peer` with the **upper computer's source
IPv4 address as seen by the controller**. The controller launcher accepts this
as `NAVIGATION_PEER`; persist it in the service environment for deployment.
The local browser bridge retains `--control-peer 127.0.0.1`.

The SDK's `robot_ip` is the **controller's address**. Installing the ROS package
does not deploy or configure the controller update. The original loopback-only
filter rejects remote motion commands, even if telemetry works.

## Build

Place this repository at
`~/rk3506_ros2_ws/src/rk3506-mobile-base-ros2-sdk`, then build from the workspace:

```bash
mkdir -p ~/rk3506_ros2_ws/src
cd ~/rk3506_ros2_ws
source /opt/ros/humble/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --packages-select rk3506_driver
source install/setup.bash
```

For Jazzy on Ubuntu 24.04, source `/opt/ros/jazzy/setup.bash` instead. Use a
separate workspace and terminal environment for each distribution. `rosdep`
installs the dependencies declared in [package.xml](rk3506_driver/package.xml).

## Configure and launch

Copy the supplied configuration outside the install tree:

```bash
cd ~/rk3506_ros2_ws
cp src/rk3506-mobile-base-ros2-sdk/rk3506_driver/config/driver.yaml ./rk3506-driver.yaml
```

Edit `robot_ip` to the controller's reachable IPv4 address. Review frames,
velocity limits and timeouts, then launch:

```bash
source install/setup.bash
ros2 launch rk3506_driver driver.launch.py params_file:="$PWD/rk3506-driver.yaml"
```

The supplied address is `192.168.50.1`. All driver parameters are read-only after
startup; edit the YAML and restart the node to apply changes. The default node
name is `rk3506_driver`. For a namespace:

```bash
ros2 launch rk3506_driver driver.launch.py \
  params_file:="$PWD/rk3506-driver.yaml" namespace:=robot1
```

The YAML uses `/**`, so it also applies to namespaced nodes. Interfaces in this
example become `/robot1/cmd_vel`, `/robot1/wheel/odometry`, `/robot1/stop`, etc.

To run the executable directly with a topic remapping:

```bash
ros2 run rk3506_driver rk3506_driver_node --ros-args \
  --params-file "$PWD/rk3506-driver.yaml" -r cmd_vel:=/navigation/cmd_vel
```

## Usage

Source the ROS environment and workspace in each command-line terminal. Once
the supervised [robot checklist](docs/ROBOT_CHECKLIST.md) permits motion,
applications publish body velocity directly; the SDK manages sessions internally.
There are no public acquire/release, motor-enable or fault-clear services.

With default Twist input, request a low forward speed at 20 Hz:

```bash
ros2 topic pub --rate 20 /cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.05, y: 0.0}, angular: {z: 0.0}}'
```

`linear.x` and `linear.y` are body-frame velocities in m/s; `angular.z` is yaw
rate in rad/s. Stop the publisher with Ctrl+C. The configurable command timeout
defaults to 300 ms and requests zero through the controller's deceleration logic;
it is not a guarantee of physical stopping time.

For stamped input, set `use_stamped_cmd_vel: true` in the YAML and restart. The
ROS CLI refreshes `stamp: now` for each publication:

```bash
ros2 topic pub --rate 20 /cmd_vel geometry_msgs/msg/TwistStamped \
  '{header: {stamp: now, frame_id: base_link}, twist: {linear: {x: 0.05}, angular: {z: 0.0}}}'
```

Use the configured `base_frame` or an empty frame ID. Keep the publisher and
driver ROS clocks aligned. Exactly one input message type is active.

Read feedback in separate terminals as needed:

```bash
ros2 topic echo /wheel/odometry
ros2 topic echo /battery_state
ros2 topic echo /digital_inputs
ros2 topic echo /diagnostics
```

Request a software stop:

```bash
ros2 service call /stop std_srvs/srv/Trigger '{}'
```

`success: true` means the controller acknowledged the stop request. Rejection,
another velocity owner, an unavailable controller or missing acknowledgement
returns `success: false` with an explanation. This does not prove physical standstill.

Software stop is latched. Stop the velocity publisher and clear it explicitly
through the existing web UI. DI1/DI2 retain motor-enable authority; the SDK never
clears a stop or fault automatically.

## Browser control and reconnection

Browser control takes priority. ROS messages during browser control are discarded;
after browser release, the next new ROS velocity command resumes navigation
automatically. A still-running ROS publisher can therefore resume immediately;
stop it before releasing browser control when no further motion is intended.

Node startup alone does not request motion. After a detected communication loss,
telemetry subscriptions recover automatically and cached velocity is discarded.
Motion requires a fresh ROS command. The controller's independent watchdogs and
physical motor interlocks remain active.

## Interfaces

Names below are relative to the node namespace, except the standard `/tf` topic.

| Interface | ROS type | Default behavior |
| --- | --- | --- |
| `cmd_vel` | `geometry_msgs/msg/Twist` | Optional `TwistStamped` via `use_stamped_cmd_vel`; one active type |
| `wheel/odometry` | `nav_msgs/msg/Odometry` | Valid controller pose/twist only |
| `battery_state` | `sensor_msgs/msg/BatteryState` | Valid battery samples only |
| `digital_inputs` | `std_msgs/msg/UInt8` | Valid DI1–DI8 bit mask only |
| `diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | Flags, health, ownership and UDP receive ages |
| `stop` | `std_srvs/srv/Trigger` | Reports controller acknowledgement, rejection or timeout |
| `/tf` | `tf2_msgs/msg/TFMessage` | Optional odometry-to-body transform; off by default |

`cmd_vel` is subscribed; telemetry and TF are published; `stop` is a service.
`digital_inputs` encodes DI1–DI8 in bits 0–7.

`TwistStamped` requires a current ROS-clock timestamp and an empty or configured
body frame ID. Stamps older or further in the future than `command_timeout` are
rejected. Planar commands must be finite, within configured limits, and have zero
linear Z and angular X/Y. Invalid commands clear the SDK's desired velocity.

## Parameters

All durations are seconds. Ports must be distinct and in `1..65535`.

| Parameter | Default | Meaning |
| --- | --- | --- |
| `robot_ip` | `192.168.50.1` | Controller IPv4 address; hostnames are not accepted |
| `base_port` | `5003` | Base command/status UDP port |
| `battery_port` | `5001` | Battery UDP port |
| `di_port` | `5002` | Digital-input UDP port |
| `command_timeout` | `0.3` | Received-command timeout and stamped-input age limit |
| `base_timeout` | `0.5` | Base UDP receive-age threshold |
| `battery_timeout` | `6.0` | Battery UDP receive-age threshold |
| `di_timeout` | `1.0` | Digital-input UDP receive-age threshold |
| `max_linear_velocity` | `1.0` | Per-axis absolute X/Y velocity limit, m/s |
| `max_angular_velocity` | `1.0` | Absolute yaw-rate limit, rad/s |
| `use_stamped_cmd_vel` | `false` | Select TwistStamped instead of Twist |
| `publish_tf` | `false` | Publish the odometry-to-body transform |
| `odom_frame` | `odom` | Wheel-odometry parent frame |
| `base_frame` | `base_link` | Body frame and odometry child frame |
| `pose_covariance` | `[0.1, 0.1, 1000000.0, 1000000.0, 1000000.0, 0.2]` | Six pose covariance diagonal entries |
| `twist_covariance` | `[0.1, 0.1, 1000000.0, 1000000.0, 1000000.0, 0.2]` | Six twist covariance diagonal entries |

Covariance entries are ordered X, Y, Z, roll, pitch, yaw and must be finite and
nonnegative. Defaults are uncalibrated. Frame IDs must be nonempty, distinct and
have no leading slash. Timeouts and velocity limits must be finite and positive;
velocity limits cannot exceed the protocol range of `32.767`.

The editable template is [driver.yaml](rk3506_driver/config/driver.yaml).

## Odometry and navigation integration

Wheel odometry comes from the controller; the SDK does not integrate motion a
second time. Sensor fusion and localization remain on the upper computer.

TF is off by default. Enable `publish_tf` only when no other estimator publishes
the configured odometry-to-body transform. Covariance defaults are uncalibrated
and configurable. In the navigation stack, bypass the upstream velocity smoother
and remap the collision-checked body velocity output to this node's `cmd_vel`;
retain collision checking and keep smoothing on the Controller.

Telemetry uses ROS reception time. Missing or invalid measurements are withheld;
diagnostics report failure/staleness. Cached battery/input packets cannot prove
source sample age. Battery current follows the BMS signed current convention;
temperature is the mean pack NTC temperature, while unavailable per-cell voltage
and design capacity remain NaN. No source clock synchronization is claimed.

The wheel-odometry origin resets when the reader restarts. V1 neither stitches
pose across restarts nor guarantees restart detection; recover the upper-level
estimator after a known restart.

## Diagnostics and troubleshooting

Diagnostics include controller flags, owner session, command age, sensor health,
malformed-packet counts and receive age. They distinguish missing recent samples
(`STALE`), failed samples or latched controller faults/stops (`ERROR`), and
inhibited motion, another velocity owner or simulation (`WARN`).

| Symptom | Check |
| --- | --- |
| Package not found | Build the workspace and source `install/setup.bash` in the current terminal |
| No telemetry or stale diagnostics | Check controller IP, UDP ports, routing, reply traffic and running controller services |
| Telemetry works but commands do not | Check the companion update, navigation-peer source IP, DI1/DI2 state, latched faults/stops and browser ownership |
| No matching `cmd_vel` subscriber | Check namespace, remapping and the selected Twist/TwistStamped type |
| Stamped commands rejected | Check timestamp freshness, clock alignment and body frame ID |
| Parameter update rejected | Edit the YAML and restart; driver parameters are read-only |
| Duplicate odometry TF | Keep one publisher for the configured odometry-to-body transform |

## Test

```bash
colcon test --packages-select rk3506_driver --event-handlers console_direct+
colcon test-result --verbose
```

The 13 integration cases start the real node with a simulated UDP peer on
loopback and an isolated ROS domain. They cover forwarding, command expiry,
handoff, reconnect, stop outcomes, telemetry, invalid input, stamped commands,
optional TF and startup configuration. They do not contact a physical robot.

From the repository root, repeat isolated checks in existing ROS containers:

```bash
tools/validate-container.sh ros2-humble-dev humble
tools/validate-container.sh ros2-jazzy-dev jazzy
```

The script requires Docker and a running container with the selected ROS
distribution and package dependencies installed. It creates a temporary
workspace and a copied install, preserving other workspaces. Logs and XML
results go to ignored `artifacts/` directories.

## Design

- [Supervised robot acceptance checklist](docs/ROBOT_CHECKLIST.md)
- [Controller access and browser integration](docs/CONTROLLER_INTEGRATION.md)
- [Version 1 design](docs/ROS2_SDK_DESIGN.md)
- [Implementation specification](docs/IMPLEMENTATION_SPEC.md)
- [GitHub specification issue](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/1)
- [Version 1 implementation ticket](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/2)
- [Domain glossary](CONTEXT.md)
- [Architecture decisions](docs/adr/0001-ros-sdk-boundary-and-initial-scope.md)
