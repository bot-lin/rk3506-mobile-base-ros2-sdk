---
status: accepted
---

# ROS 2 SDK boundary and initial scope

ROS 2 and navigation remain on the upper computer; the RK3506 retains the standalone C++ controller. This carries forward the existing project decision to keep ROS off the ARMv7 Buildroot controller because of its platform and resource constraints.

On 2026-10-19 the user agreed that the first release serves the navigation team: ROS 2 applications can command the mobile base and receive wheel odometry, battery telemetry, digital inputs and controller health. It targets the current Helishi two-swerve configuration; Kinco and other chassis are deferred so the first release can be verified against one concrete robot. Full autonomous navigation is a later integration milestone.

The user explicitly requires both ROS 2 Humble and Jazzy in the first release, and subsequently agreed to x86_64 and ARM64 Linux support. All four distribution/architecture combinations are in scope: Humble on Ubuntu 22.04 and Jazzy on Ubuntu 24.04. The accepted public interfaces, runtime policies, package location and validation scope are consolidated in `../ROS2_SDK_DESIGN.md`. Transport and delivery shape are recorded in ADR-0002; neither ADR claims implementation or hardware acceptance.
