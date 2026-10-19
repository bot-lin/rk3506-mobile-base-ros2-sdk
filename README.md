# rk3506-mobile-base-ros2-sdk

ROS 2 integration for the RK3506 Helishi two-swerve mobile robot controller.
The SDK runs on the upper computer and communicates with the standalone RK3506
controller over UDP.

Planned targets are Humble/Ubuntu 22.04 and Jazzy/Ubuntu 24.04 on
x86_64 and ARM64. The ROS package will be `rk3506_driver/`.

The version 1 design is recorded; driver implementation and platform/hardware
validation have not started. Controller firmware remains in the separate
`rk3506-can-interface-cpp` repository.

- [Version 1 design](docs/ROS2_SDK_DESIGN.md)
- [Implementation specification](docs/IMPLEMENTATION_SPEC.md)
- [GitHub specification issue](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/1)
- [Version 1 implementation ticket](https://github.com/bot-lin/rk3506-mobile-base-ros2-sdk/issues/2)
- [Domain glossary](CONTEXT.md)
- [Architecture decisions](docs/adr/0001-ros-sdk-boundary-and-initial-scope.md)
