# RK3506 Mobile Robot Controller

Language for the mobile base and its ROS 2 integration.

## Language

**Mobile base**:
The robot platform that translates and rotates using two independently steered and driven wheel modules.

**Controller**:
The RK3506 system responsible for operating the mobile base and reporting its state.
_Avoid_: ROS computer, navigation computer

**ROS 2 SDK**:
The ROS node package through which ROS 2 applications command the controller and receive mobile-base telemetry.
_Avoid_: RK3506 ROS runtime

**Upper computer**:
The computer that runs ROS 2 and navigation applications and communicates with the controller.
_Avoid_: Controller

**Wheel odometry**:
An estimate of mobile-base pose and velocity derived from wheel motion and steering feedback.
_Avoid_: Ground-truth pose, fused localization

**Velocity owner**:
The command source currently permitted to request mobile-base velocity. This permission is separate from hardware motor enable.
_Avoid_: Motor owner

**Operator takeover**:
An explicit transfer of velocity ownership from ROS navigation to the browser operator.

**ROS resumption**:
The automatic return to ROS velocity commands after browser control ends, requiring a newly arriving command. Commands received during browser control are discarded.

**Motor enable**:
The hardware-governed permission for the motors to operate, requiring a fresh released DI1 and asserted DI2 in the current controller mode.
_Avoid_: Velocity ownership, software ARM
