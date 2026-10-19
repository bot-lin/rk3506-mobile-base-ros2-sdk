# RK3506 mobile-base ROS 2 SDK — version 1

## Problem Statement

The navigation team needs to command the current Helishi two-swerve Mobile base
and consume its feedback from ROS 2 without implementing the Controller's UDP
protocol in every application. The RK3506 already runs a standalone C++
Controller, but there is no ROS node connecting it to the Upper computer.

The integration must support Humble and Jazzy on both x86_64 and ARM64, coexist
with browser operation, and retain hardware motor-enable authority. Version 1
must stay simple: applications publish velocity directly, without explicit
control-acquisition or release calls, and existing telemetry formats remain.

## Solution

Deliver a launchable C++ ROS 2 SDK in the separate
`rk3506-mobile-base-ros2-sdk` repository. One node accepts body velocity, handles
UDP sessions internally, publishes standard ROS telemetry and diagnostics, and
provides a software-stop service. Include YAML configuration, a launch file,
examples and a supervised robot-validation checklist.

The Controller retains kinematics, velocity smoothing, watchdogs and motor
interlocks. Browser Operator takeover stops ROS motion and takes priority.
ROS resumption is automatic after browser release, but requires a newly
arriving command; commands received during browser control are discarded.

The SDK targets Humble on Ubuntu 22.04 and Jazzy on Ubuntu 24.04, each on
x86_64 and ARM64. Controller-side network access and ownership changes remain
in the separate `rk3506-can-interface-cpp` repository.

## User Stories

1. As a navigation developer, I want a standalone ROS 2 SDK repository, so that I can work on Upper-computer integration separately from Controller firmware.
2. As a navigation developer, I want one launchable C++ node, so that I can connect the Mobile base without building a new application around its protocol.
3. As an integrator, I want YAML configuration and a launch file, so that deployment does not require source changes.
4. As a Humble user, I want support on Ubuntu 22.04, so that I can use the SDK with my existing ROS stack.
5. As a Jazzy user, I want support on Ubuntu 24.04, so that I can use the SDK with my newer ROS stack.
6. As a developer using an x86_64 Upper computer, I want a validated build and integration path, so that I can develop and deploy on that architecture.
7. As a developer using an ARM64 Upper computer, I want the same SDK behavior, so that I can deploy on an embedded navigation computer.
8. As a navigation application, I want to command forward, lateral and rotational body velocity, so that I can use the two-swerve Mobile base's motion capabilities.
9. As an existing navigation-stack user, I want Twist input by default, so that I can connect my current velocity publisher.
10. As an integrator using stamped commands, I want configurable TwistStamped input, so that I can use that interface without running two competing velocity subscriptions.
11. As an integrator, I want topic names to be remappable, so that the SDK fits the robot's existing ROS graph.
12. As a navigation application, I want UDP session management to be automatic, so that publishing velocity is sufficient without acquire/release service calls.
13. As an operator, I want startup to wait for a fresh command before requesting motion, so that a saved command cannot move the robot when the node starts.
14. As an operator, I want stopping to be requested after the configurable command timeout, defaulting to 300 ms, so that an abandoned ROS command does not remain active indefinitely.
15. As an integrator, I want the Controller to retain velocity smoothing, so that acceleration and deceleration follow the existing robot configuration.
16. As an integrator, I want the ROS integration to bypass duplicate upstream smoothing while retaining collision checking, so that the command path has one shaping authority.
17. As an operator, I want the Controller's independent watchdogs and interlocks to remain active, so that the SDK does not replace existing protections.
18. As a browser operator, I want explicit Operator takeover to stop ROS motion and give me control, so that navigation does not compete with my manual commands.
19. As a browser operator, I want ROS commands discarded during browser control, so that they cannot accumulate for later execution.
20. As a navigation user, I want automatic ROS resumption after browser release on a fresh command, so that I do not need an explicit reacquisition step.
21. As an operator, I want cached motion excluded from handoff and reconnection, so that an old command is never replayed as a new request.
22. As a navigation developer, I want telemetry reconnection to happen automatically, so that a temporary network interruption does not require restarting the node.
23. As an operator, I want motion after a detected connection loss to require a newly received command, so that reconnection alone cannot replay motion.
24. As an operator, I want a ROS software-stop service, so that an application can request the Controller's existing latched motion inhibition.
25. As an operator, I want faults cleared only explicitly through the existing web UI, so that a reconnecting SDK does not silently clear a stop or fault.
26. As an operator, I want DI1/DI2 to retain Motor enable authority, so that ROS velocity ownership cannot bypass physical input requirements.
27. As a navigation application, I want Wheel odometry pose and body velocity from the Controller, so that I can use feedback without integrating the same motion a second time.
28. As a navigation application, I want invalid Wheel odometry withheld, so that a retained pose or unavailable feedback is not represented as a valid new measurement.
29. As an integrator using an estimator, I want SDK TF publication disabled by default, so that there is no competing odometry transform publisher.
30. As an integrator without a separate estimator, I want optional odometry TF publication, so that I can use the SDK's Wheel odometry directly.
31. As an integrator, I want configurable odometry frames and covariance, so that I can connect my estimator without claiming unmeasured precision.
32. As an integrator, I want Controller-restart pose resets documented, so that I can recover the estimator without assuming the SDK preserves pose continuity.
33. As a monitoring application, I want standard ROS battery messages, so that I can consume battery feedback with existing ROS tools.
34. As a monitoring application, I want all eight digital-input bits, so that I can observe the Controller's physical inputs.
35. As an operator, I want Controller flags and link/sensor status in diagnostics, so that unavailable feedback is distinguishable from healthy feedback.
36. As a telemetry consumer, I want invalid measurements withheld and receive-age problems reported, so that missing or invalid packets do not silently look healthy.
37. As an integrator, I want telemetry stamped at ROS reception time with its limitations documented, so that I do not mistake reception time for sensor acquisition time.
38. As an existing Controller-client user, I want existing telemetry packets preserved, so that the SDK does not force a telemetry-format migration.
39. As an integrator, I want Controller access configured for the remote SDK and local browser bridge, so that the loopback-only deployment does not silently reject ROS control.
40. As a navigation developer, I want examples for publishing velocity and consuming feedback, so that I can connect the SDK without reverse-engineering its interfaces.
41. As a maintainer, I want behavior checked through the real ROS node and UDP boundary, so that tests verify what applications and the Controller actually observe.
42. As a maintainer, I want command loss, link loss and browser handoff exercised with a simulated Controller, so that regressions can be reproduced without physical motion.
43. As a Controller maintainer, I want existing simulated-CAN runtime checks retained when access and ownership change, so that integration does not regress Controller behavior.
44. As a release owner, I want evidence for all four distro/architecture combinations, so that supported platforms are distinguishable from untested targets.
45. As a robot integrator, I want a separate supervised physical checklist for motion, hardware stops and Wheel odometry, so that software checks are not confused with robot acceptance.
46. As a release owner, I want incomplete platform and physical checks reported accurately, so that I know what remains before deployment acceptance.

## Implementation Decisions

- Keep ROS 2 and navigation on the Upper computer. Retain the RK3506's standalone C++ Controller and target the current Helishi two-swerve Mobile base only.
- Deliver the `rk3506_driver` ROS package in the separate SDK repository, using `ament_cmake` and `rclcpp`, with one node, YAML configuration, a launch file and examples. Controller-side changes belong in the existing Controller repository.
- Use direct UDP for battery on port 5001, digital inputs on port 5002 and base commands/status on port 5003. Preserve existing telemetry packet formats and protocol validation at the boundary.
- Adapt Controller network access and ownership arbitration to allow the remote ROS client and local browser bridge to coexist. The currently deployed loopback-only control filter and non-preemptible owner cannot satisfy the agreed handoff behavior without changes.
- Manage UDP claims and sessions inside the SDK. Applications publish velocity directly; there are no ROS acquire/release services.
- Give explicit browser Operator takeover priority over ROS motion. Stop ROS motion before giving browser control, discard ROS commands during browser control, and automatically resume ROS only on a command newly received after browser release. Never replay a cached command across the handoff.
- Accept body velocity on remappable `cmd_vel`, using either `geometry_msgs/msg/Twist` or `geometry_msgs/msg/TwistStamped` according to configuration. `Twist` is the default and exactly one input type is active.
- Default the configurable ROS-command timeout to 300 ms. On expiry, request zero through the Controller's existing deceleration logic. Preserve the Controller's independent navigation-command and owner watchdogs; the SDK timeout is not a physical stopping-time guarantee.
- Start and reconnect without replaying motion. Reconnect telemetry automatically and require a fresh ROS command after startup or detected connection loss before requesting motion.
- Leave kinematics and acceleration/deceleration shaping in the Controller. Bypass the upstream ROS velocity smoother in the navigation integration while retaining collision checking.
- Provide `stop` using `std_srvs/srv/Trigger` and the Controller's existing latched software stop. Report the request outcome honestly rather than treating a local service invocation as proof of physical stopping.
- Keep fault clearing explicit in the existing web UI. Do not clear faults automatically, add software Motor enable controls, or bypass DI1/DI2. Fault clearing may permit DI-controlled motor initialization, with previous velocity erased.
- Publish valid Controller pose and body velocity as `nav_msgs/msg/Odometry` on `wheel/odometry`. Do not independently integrate wheel motion in the SDK or publish invalid retained estimates as valid measurements.
- Disable `odom` to `base_link` TF publication by default; make it optional for systems without a separate estimator. Frames and covariance are configurable, without claiming empirically measured covariance. Sensor fusion stays on the Upper computer.
- Publish battery feedback as `sensor_msgs/msg/BatteryState` on `battery_state`, valid eight-bit digital inputs as `std_msgs/msg/UInt8` on `digital_inputs`, and Controller/link/sensor health as `diagnostic_msgs/msg/DiagnosticArray` on `diagnostics`.
- Use ROS receive-time stamps and receive-age diagnostics. Withhold invalid measurements. Cached battery/input replies cannot establish source measurement age; the SDK must not claim otherwise.
- Expose the Controller's pose reset after a reader restart without continuity stitching. Current packets cannot guarantee restart identification. Recovery of an estimator after a known restart is an operational integration step.
- Validate Humble/Ubuntu 22.04 and Jazzy/Ubuntu 24.04 on both x86_64 and ARM64. Keep implementation small and avoid a separate public C++ client API or a new telemetry protocol in v1.

## Testing Decisions

- Use one primary integration seam: launch the actual SDK node against a simulated Controller through real UDP sockets, drive its public ROS inputs/services, and assert public ROS outputs plus observed UDP behavior. This is the highest shared boundary already agreed during design; no new internal interfaces are required solely for tests.
- The user already approved the simulated-Controller seam, all four platform combinations and the separate robot checklist. This spec synthesizes those decisions rather than reopening the interview.
- Good tests assert externally visible behavior, bounded command expiry and the absence of cached nonzero commands after handoff or reconnection. They do not assert internal class structure, private flags, timer callback counts or exact scheduling instants.
- Exercise the node's command forwarding, selected input message type, automatic sessions, command expiry, stop outcome, telemetry decoding/publication and diagnostics through this seam. Cover malformed or invalid packets sufficiently to ensure they are not published as healthy measurements.
- Simulate browser ownership and release at the Controller boundary. Verify ROS commands during browser control are discarded, automatic resumption requires a newly arriving command, and delayed or cached commands cannot become resumed motion.
- Simulate lost command publication, lost Controller communication and restored communication. Verify zero is requested on SDK command expiry, the node does not keep an abandoned nonzero command alive, and reconnecting does not replay old motion.
- Check known telemetry payloads against the standard ROS message fields, validity behavior and receipt-time convention. Exercise default-disabled TF and the explicitly enabled TF mode. Do not test for source-time guarantees or pose continuity excluded from v1.
- Reuse the Controller project's existing production-runtime tests with real UDP sockets and substituted CAN transport when modifying access or ownership. Prior art includes session/sequence checks, input interlocks, owner timeout and fault handling; avoid inventing another motor-protocol mock layer.
- Use existing serial UDP tests with simulated serial devices as prior art for battery/input packet fixtures. Preserve compatibility with existing clients; source sample-age proof is outside the current wire format.
- Build and run applicable integration checks for each of the four distro/architecture combinations. Record actual results and missing coverage; successful checks on one host do not establish the entire matrix.
- Maintain a separate supervised robot checklist for remote control, browser takeover/resumption, command loss, physical input stops, Wheel odometry and the existing stop/clear behavior. Physical acceptance requires robot evidence, not only a simulated Controller or substituted CAN transport.

## Out of Scope

- Running ROS 2, DDS, navigation or an estimator on the RK3506.
- Kinco or other chassis/controller configurations in the first release.
- A public standalone C++ client library, Python client SDK or extra application framework.
- Explicit application-level acquire/release calls or ROS acquire/release services.
- A new telemetry format, source timestamps, source sample sequences, Controller-run identifiers or guaranteed restart detection.
- Pose stitching across Controller restarts, IMU fusion, fused odometry or localization in the SDK.
- Automatic fault clearing, software replacement of DI1/DI2 Motor enable, or another velocity smoother.
- A ROS fault-clear service; use the existing web UI for v1 recovery.
- Full autonomous-navigation acceptance, new navigation algorithms or replacement of collision checking.
- Claims of hard real-time stopping, source measurement-age certainty, measured covariance or physical acceptance without corresponding evidence.
- Broad refactoring of the standalone Controller or unrelated browser/network features.

## Further Notes

The glossary and four accepted ADRs are the domain and architecture references
for this feature. The final separate-repository instruction supersedes the
earlier suggestion to place the SDK inside the Controller repository.

The SDK repository currently contains design documentation only. No driver
implementation, platform-matrix validation or physical acceptance has been
completed. Remote control depends on the accompanying Controller access and
ownership changes; subscribing to telemetry alone does not make the deployed
Controller remotely controllable.

Track this specification in GitHub Issues for
`bot-lin/rk3506-mobile-base-ros2-sdk`, using the configured `ready-for-agent`
triage label. Repository agent configuration records the issue-tracker workflow,
five canonical triage labels and single-context domain-document rules.
