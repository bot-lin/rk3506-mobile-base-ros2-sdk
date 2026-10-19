# Validation

Software validation completed on 2026-10-19. Repeat the container checks with
`tools/validate-container.sh`; detailed logs and test XML are retained locally
under ignored `artifacts/` directories. No checks in this report deployed code
or contacted the physical robot.

The primary seam runs the actual C++ ROS node against an independent UDP peer
over loopback. Tests cover command/odometry round trips, command expiry, controller
stop acknowledgements/rejections/timeouts, invalid sensors, browser priority and
fresh resumption, stamped commands, optional/default TF, reconnect without replay,
invalid motion/odometry and invalid startup configuration.

| Target | Build, copied install, launch discovery, integration tests |
| --- | --- |
| Humble / Ubuntu 22.04 / ARM64 | PASS, native ARM64 Docker |
| Jazzy / Ubuntu 24.04 / ARM64 | PASS, native ARM64 Docker |
| Humble / Ubuntu 22.04 / x86_64 | PASS, Docker x86_64 emulation |
| Jazzy / Ubuntu 24.04 / x86_64 | PASS, Docker x86_64 emulation |

Each platform passed all 13 pytest cases. `colcon test-result` reports 14 tests
including the enclosing integration-test entry, with zero failures, errors or
skips. The x86_64 results establish functional build/integration compatibility
under emulation; native x86_64 timing and load behavior were not measured.

The Controller companion commit is `7cc26e4` in `rk3506-can-interface-cpp`.
Its isolated changes on baseline `5c297c5` passed all 23 CTest checks through
real UDP and the existing substituted-CAN/serial transports. New checks cover
remote navigation, browser preemption/release, fresh resumption, software stop,
rejection of retired sessions after 40 further handoffs, and zero targets for
both Helishi drives before acknowledging takeover while retaining DI2 enable.
The repeated-handoff regression failed against the former 32-session cache and
passed after preserving retired IDs for the reader process lifetime.

Separate standards and specification reviews found no remaining actionable
findings after fixes. Existing unrelated motor-direction changes in the
Controller working tree were preserved and excluded from this companion commit.

The supervised robot checklist is delivered separately. Physical acceptance
remains unperformed; platform/simulation results do not establish loaded-motion
or physical-stop performance.
