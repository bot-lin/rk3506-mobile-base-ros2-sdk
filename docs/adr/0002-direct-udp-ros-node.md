---
status: accepted
---

# Deliver a ROS node using direct controller UDP

On 2026-10-19 the user chose direct UDP for the ROS 2 SDK, accepting changes to the controller's network access policy while retaining its motion-control logic. The existing HTTP bridge sends teleoperation commands, whereas direct UDP exposes the controller's Navigation command path. The deployed loopback-only control filter must therefore be adapted before remote ROS control works; browser/ROS ownership rules are recorded in ADR-0003.

The initial deliverable is a launchable ROS node package with topics, services, configuration and examples. A public reusable C++ client library is deferred until a concrete application needs it. This gives the navigation team a ROS integration without committing to a second public programming interface.

The user subsequently requested a separate repository named `rk3506-mobile-base-ros2-sdk`, outside `rk3506-can-interface-cpp`. The planned ROS package is `rk3506_driver/` in the SDK repository. Controller-side changes remain in the standalone controller repository.
