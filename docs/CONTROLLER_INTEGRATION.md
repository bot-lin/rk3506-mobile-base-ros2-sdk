# Controller integration

The SDK talks directly to the existing telemetry and command endpoints. Its
companion change belongs in `rk3506-can-interface-cpp`; deploying the ROS package
alone does not update the RK3506.

## Access and handoff

The reader retains `--control-peer 127.0.0.1` for the local browser bridge and
adds optional `--navigation-peer <upper-computer-IPv4>`. The two addresses must
be distinct; the browser peer must be loopback in this mode. Only the configured
navigation peer can issue navigation commands remotely. The existing IP/session
filter is not cryptographic authentication.

The launch script accepts `NAVIGATION_PEER` as an environment setting and passes
the optional reader argument. Configure it persistently in the board's service
environment when preparing deployment; an interactive setting alone does not
persist across boots. No board configuration or deployment is performed by the
SDK build or test commands.

Navigation can claim, send navigation velocity, request software stop, keep its
session alive and release it. It cannot teleoperate, clear faults, ARM or DISARM.
The local browser can preempt navigation with its existing explicit claim.
The Controller clears motion and, when moving in DI2-controlled mode, writes zero
to both drive targets before acknowledging takeover. Motor enable remains owned
by the physical inputs.

The browser bridge uses an additional version-1 control operation, RELEASE
`0x18`, with the existing session/sequence layout (10 bytes) and ACK `0x98`
(11 bytes). Release immediately clears velocity ownership without changing
DI2 enable. The bridge rotates its session after release. This is a control
extension; existing telemetry formats remain unchanged.

The Controller rejects claims using any navigation session retired during its
process lifetime. The SDK rotates its session after losing ownership or
communication. Retired IDs are retained until the reader restarts; protocol v1
does not provide replay protection across reader restarts. Source sample age and
guaranteed restart identity remain v1 limitations.

## Integration checks

The Controller's `navigation_handoff` test uses real UDP clients at distinct
loopback addresses. `navigation_handoff_physical_codec` exercises Helishi startup
and both drive-zero CAN encodings with the existing substituted-CAN fixture.
Neither test sends physical CAN traffic. Existing runtime and browser tests
remain part of the Controller regression suite.

Deployment and the supervised physical checks are separate operational steps.
