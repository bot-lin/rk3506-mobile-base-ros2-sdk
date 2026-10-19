---
status: accepted
---

# Operator takeover and controller-owned velocity smoothing

The direct UDP ROS client and the browser bridge are separate command sources. On 2026-10-19 the user chose explicit browser takeover: stop ROS motion before giving the browser control. The user subsequently removed explicit ROS acquire/release operations to simplify v1: the SDK manages the required UDP sessions internally. The user then confirmed automatic ROS resumption on a fresh velocity command after browser release. Discard ROS commands while browser control is active and never replay cached motion on handoff. DI1/DI2 retain motor-enable authority throughout; velocity ownership does not replace those inputs.

The RK3506 retains acceleration/deceleration shaping. The ROS integration bypasses the existing upstream velocity smoother while retaining collision checking, avoiding two independently configured smoothing stages. The SDK forwards fresh body-velocity commands; it does not add another smoother. The existing board protocol cannot implement cross-client takeover as-is, so ownership/access changes are required alongside the SDK.
