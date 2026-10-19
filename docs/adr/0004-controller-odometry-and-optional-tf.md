---
status: accepted
---

# Publish controller wheel odometry with optional TF

On 2026-10-19 the user chose to publish the controller's wheel-derived pose and body velocity on `wheel/odometry`. Reusing the controller estimate avoids integrating the same wheel motion independently in the SDK. Sensor fusion remains on the upper computer.

The SDK disables `odom -> base_link` TF publication by default and offers it as an option for systems without a separate estimator. This supports the existing EKF integration without creating a competing TF publisher while still permitting standalone odometry use. The user subsequently accepted receive-time stamping, withholding invalid measurements and leaving controller-restart pose resets visible in v1. Frames and covariance remain configurable; preserving pose continuity would require additional recovery logic deferred from this release. See `../ROS2_SDK_DESIGN.md` for the consolidated contract.
