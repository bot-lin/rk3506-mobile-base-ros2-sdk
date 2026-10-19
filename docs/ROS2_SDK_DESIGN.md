# RK3506 ROS 2 SDK — version 1 design

The user accepted these design decisions on 2026-10-19, including the final
failure-handling and telemetry defaults, and subsequently requested a separate
repository named `rk3506-mobile-base-ros2-sdk`. The driver is implemented;
see [validation](VALIDATION.md) for executed checks. Deployment and supervised
hardware acceptance remain outstanding.

## Scope and platform support

Deliver a C++ ROS node package for the navigation team to command the current
Helishi two-swerve robot and receive wheel odometry, battery, digital inputs
and controller health. ROS/navigation runs on the upper computer; the RK3506
retains its standalone C++ motion controller.

Place the package at `rk3506_driver/` in this standalone repository, using
`ament_cmake`/`rclcpp`. Deliver one node, YAML configuration, a launch file,
usage examples and a robot-validation checklist.

The standalone controller remains in `rk3506-can-interface-cpp`. Controller-side
network access and ownership changes belong there; the ROS SDK belongs here.

| ROS distribution | Host OS | Architectures to validate |
| --- | --- | --- |
| Humble | Ubuntu 22.04 | x86_64, ARM64 |
| Jazzy | Ubuntu 24.04 | x86_64, ARM64 |

These are the official binary platform pairings documented by
[ROS Humble](https://raw.githubusercontent.com/ros2/ros2_documentation/humble/source/Installation/Ubuntu-Install-Debs.rst)
and [ROS Jazzy](https://raw.githubusercontent.com/ros2/ros2_documentation/jazzy/source/Installation/Ubuntu-Install-Debs.rst).

## ROS interfaces

Use standard ROS message/service types. Topic and frame names are configurable
or remappable. Exactly one velocity input type is active; `Twist` is the default.

| Interface | Type and behavior |
| --- | --- |
| `cmd_vel` | `geometry_msgs/msg/Twist` or `geometry_msgs/msg/TwistStamped`; body-velocity input |
| `wheel/odometry` | `nav_msgs/msg/Odometry`; controller pose/twist, published only with valid odometry |
| `battery_state` | `sensor_msgs/msg/BatteryState`; unavailable measurements are not presented as healthy |
| `digital_inputs` | `std_msgs/msg/UInt8`; eight DI bits, valid samples only, health in diagnostics |
| `diagnostics` | `diagnostic_msgs/msg/DiagnosticArray`; controller flags and link/sensor receive-age status |
| `stop` | `std_srvs/srv/Trigger`; existing latched software stop, distinct from physical motor disable |

The application publishes velocity directly. UDP session management is internal
to the SDK; there are no ROS acquire/release services. Fault clearing stays in
the existing web UI. There are no software motor-enable controls: DI1/DI2 retain
that authority.

## Transport and browser priority

Use direct UDP: battery on 5001, digital inputs on 5002, base commands/status
on 5003. Preserve existing telemetry packet formats. Adapt the controller's
network-access policy and ownership handoff so the remote SDK and local browser
bridge can coexist; the deployed loopback-only filter does not support this yet.

The browser can explicitly take control, stopping ROS motion first. Discard ROS
commands received during browser control. After browser release, ROS automatically
resumes on a newly arriving velocity command. No cached motion is replayed.

Keep the controller's existing kinematics, interlocks and velocity smoothing.
Bypass the upstream ROS velocity smoother while retaining collision checking.

## Timeout, connection and fault behavior

- The configurable ROS-command timeout defaults to 300 ms. Expiry requests zero
  velocity through the controller's existing deceleration logic; this specifies
  when stopping is requested, not a guaranteed physical stopping time.
- Retain the controller's existing independent watchdogs. The SDK's 300 ms timer
  does not replace the board's existing navigation-command/owner timeouts.
- Reconnect telemetry automatically, but require a fresh command after startup
  or a detected connection loss before requesting motion. Drop cached commands.
- Faults are never cleared automatically. Software stop remains latched until
  explicitly cleared through the web UI. Clearing may permit motor initialization
  when the physical DI inputs allow it, with no previous velocity replay.

## Telemetry and odometry limits

Stamp telemetry with ROS receive time. Publish invalid/stale conditions through
diagnostics and withhold invalid measurements. Local receive age describes the
network observation; it does not prove measurement age, because battery/input
GET and subscription-renewal replies may contain cached samples.

Use controller wheel pose/twist directly. TF publication is disabled by default;
offer `odom -> base_link` publication for systems without another estimator.
Frames and covariance are configurable, with no claim of measured covariance.
Sensor fusion stays on the upper computer.

A reader restart resets the wheel-odometry origin. V1 neither stitches pose
across restarts nor guarantees restart detection from current telemetry.
Estimator recovery after a known controller restart is an operational step.

## Acceptance and deferred work

Build and exercise command/telemetry behavior on all four distro/architecture
combinations. Use a simulated controller to check command/link loss, browser
takeover, automatic fresh-command resumption and rejection of cached replay.
Exercise existing controller protections with the controller's simulated-CAN
tests where ownership/access behavior changes.

Provide a separate supervised robot checklist for motion, hardware stops and
wheel odometry. Software/simulation checks do not establish physical acceptance.
Record platform coverage and remaining hardware checks accurately.

Defer Kinco/other chassis, a public C++ client library, fused odometry, automatic
fault recovery, pose continuity across restarts and a new telemetry version with
source timestamps/sequences/controller-run identifiers. Full autonomous navigation
is a later integration milestone.

## Decision references

- [Scope and compatibility](adr/0001-ros-sdk-boundary-and-initial-scope.md)
- [Direct UDP and ROS node delivery](adr/0002-direct-udp-ros-node.md)
- [Operator takeover and velocity smoothing](adr/0003-operator-takeover-and-controller-smoothing.md)
- [Wheel odometry and TF ownership](adr/0004-controller-odometry-and-optional-tf.md)
- [Domain language](../CONTEXT.md)
