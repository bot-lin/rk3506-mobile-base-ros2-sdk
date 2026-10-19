# rk3506-mobile-base-ros2-sdk

ROS 2 integration for the RK3506 Helishi two-swerve mobile robot controller.
The SDK runs on the upper computer and communicates with the standalone RK3506
controller over UDP.

Targets: Humble/Ubuntu 22.04 and Jazzy/Ubuntu 24.04 on x86_64 and ARM64.
The package is `rk3506_driver`. Controller firmware remains in the separate
`rk3506-can-interface-cpp` repository. See [validation](docs/VALIDATION.md) for
executed checks and outstanding physical acceptance.

## Build and run

In a ROS 2 workspace with this repository under its source directory:

```bash
source /opt/ros/humble/setup.bash  # or /opt/ros/jazzy/setup.bash
rosdep install --from-paths src --ignore-src -r -y
colcon build --packages-select rk3506_driver
source install/setup.bash
ros2 launch rk3506_driver driver.launch.py params_file:=/absolute/path/to/driver.yaml
```

Copy the supplied configuration and set `robot_ip` to the Controller's reachable
IPv4 address. Parameters are read-only after startup. Optional `namespace` and
standard ROS topic remapping support integration into an existing robot graph.

The Controller needs the companion access/handoff update described in
[controller integration](docs/CONTROLLER_INTEGRATION.md). The original deployed
loopback-only filter rejects remote motion commands, even if telemetry works.

Once the supervised [robot checklist](docs/ROBOT_CHECKLIST.md) permits motion,
applications only publish body velocity; there are no acquire/release services:

```bash
ros2 topic pub --rate 20 /cmd_vel geometry_msgs/msg/Twist \
  '{linear: {x: 0.05, y: 0.0}, angular: {z: 0.0}}'
ros2 topic echo /wheel/odometry
ros2 service call /stop std_srvs/srv/Trigger '{}'
```

The configurable command timeout defaults to 300 ms and requests zero through
the Controller's deceleration logic. Software stop is latched; clear it explicitly
through the existing web UI. DI1/DI2 retain motor-enable authority.

Browser control takes priority. ROS messages during browser control are discarded;
after browser release, the next new ROS velocity command resumes navigation
automatically. A still-running ROS publisher can therefore resume immediately.

## Interfaces

| Interface | ROS type | Default behavior |
| --- | --- | --- |
| `cmd_vel` | `geometry_msgs/msg/Twist` | Optional `TwistStamped` via `use_stamped_cmd_vel`; one active type |
| `wheel/odometry` | `nav_msgs/msg/Odometry` | Valid controller pose/twist only |
| `battery_state` | `sensor_msgs/msg/BatteryState` | Valid battery samples only |
| `digital_inputs` | `std_msgs/msg/UInt8` | Valid DI1–DI8 bit mask only |
| `diagnostics` | `diagnostic_msgs/msg/DiagnosticArray` | Flags, health, ownership and UDP receive ages |
| `stop` | `std_srvs/srv/Trigger` | Reports controller acknowledgement, rejection or timeout |

`TwistStamped` requires a current ROS-clock timestamp and an empty or configured
body frame ID. Stamps older or further in the future than `command_timeout` are
rejected. Planar commands must be finite, within configured limits, and have zero
linear Z and angular X/Y. Invalid commands clear the SDK's desired velocity.

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

## Test

```bash
colcon test --packages-select rk3506_driver --event-handlers console_direct+
colcon test-result --verbose
```

Tests start the real node with a simulated UDP peer on loopback and an isolated
ROS domain. To repeat build/install/launch/test checks in an existing container:

```bash
tools/validate-container.sh ros2-humble-dev humble
tools/validate-container.sh ros2-jazzy-dev jazzy
```

The script creates a temporary workspace and a copied install, preserving other
workspaces. Logs and XML results go to ignored `artifacts/` directories.

## Design

- [Version 1 design](docs/ROS2_SDK_DESIGN.md)
- [Implementation specification](docs/IMPLEMENTATION_SPEC.md)
- [GitHub specification issue](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/1)
- [Version 1 implementation ticket](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/2)
- [Domain glossary](CONTEXT.md)
- [Architecture decisions](docs/adr/0001-ros-sdk-boundary-and-initial-scope.md)
