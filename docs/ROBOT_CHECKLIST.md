# Supervised robot acceptance checklist

These checks require an operator at the robot. They have not been executed by
the software integration tests. Record the controller revision, SDK revision,
configuration, platform and observed results with each run.

- [ ] Verify steering/wheel calibration, clear workspace and physical stop access.
  Begin with DI2 off and the navigation publisher stopped.
- [ ] Install the reviewed Controller companion update and configure the actual
  navigation-peer IPv4. Verify normal browser operation and telemetry first.
- [ ] Launch the SDK with the actual robot IP, correct frames and conservative
  velocity limits. Confirm battery, DI bits, controller health and wheel feedback.
- [ ] Confirm node startup alone causes no motion and does not enable motors.
- [ ] Test DI1 and DI2 state transitions under local supervision before driving.
  DI1 pressed overrides DI2; enabling remains hardware-controlled.
- [ ] With the robot ready, command low forward, lateral and rotational velocity
  individually. Check directions and measured Wheel odometry against the motion.
- [ ] Stop publishing. Confirm zero is requested after the configured command
  timeout and record actual deceleration/stopping behavior; 300 ms is not a
  guaranteed physical stopping time.
- [ ] During low-speed ROS operation, explicitly take browser control. Confirm
  navigation stops, manual control works and DI2 enable is preserved.
- [ ] Stop the ROS publisher before releasing browser control when no further
  navigation is intended. Separately verify that an intentionally continuing
  publisher resumes automatically only through new post-release commands.
- [ ] Interrupt network communication during low-speed operation. Confirm the
  existing Controller watchdog stops movement. Restore communication with the
  publisher stopped and verify cached motion is not replayed.
- [ ] Call the ROS stop service and confirm its acknowledgement plus observed
  latched motion inhibition. Clear explicitly in the web UI with the publisher
  stopped; confirm old velocity is not replayed. Clear can reinitialize motors
  when the current physical inputs permit it.
- [ ] With navigation stopped, restart the reader and observe its odometry-origin
  reset. Recover the upper-level estimator before restarting navigation.
- [ ] Verify only one odometry-to-body TF publisher. Keep SDK TF disabled when
  the upper-level estimator owns that transform.

Full-speed/loaded operation, physical steering accuracy, repeated power-cycle
reliability and complete autonomous-navigation acceptance require their own
recorded robot evidence. Do not infer those results from simulated CAN or UDP.
