#!/usr/bin/env bash
# Run in an isolated workspace inside an existing ROS development container.
set -euo pipefail
sdk_container=${1:?Usage: validate-container.sh CONTAINER humble|jazzy}
sdk_distro=${2:?Usage: validate-container.sh CONTAINER humble|jazzy}
case "$sdk_distro" in humble|jazzy) ;; *) echo 'Expected humble or jazzy' >&2; exit 2 ;; esac
sdk_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sdk_workspace=$(docker exec "$sdk_container" mktemp -d /tmp/rk3506-sdk-validation.XXXXXX)
docker exec "$sdk_container" mkdir -p "$sdk_workspace/src"
docker cp "$sdk_root/rk3506_driver" "$sdk_container:$sdk_workspace/src/rk3506_driver"
sdk_result=0
docker exec -i -e SDK_WORKSPACE="$sdk_workspace" -e SDK_DISTRO="$sdk_distro" \
    -e ROS_DOMAIN_ID=173 -e ROS_LOCALHOST_ONLY=1 "$sdk_container" bash -s <<'BASH' || sdk_result=$?
set -eo pipefail
source "/opt/ros/$SDK_DISTRO/setup.bash"
cd "$SDK_WORKSPACE"
uname -m
colcon build --packages-select rk3506_driver --event-handlers console_direct+
source install/setup.bash
ros2 pkg prefix rk3506_driver
ros2 launch rk3506_driver driver.launch.py --show-args
colcon test --packages-select rk3506_driver --event-handlers console_direct+
colcon test-result --verbose
BASH
mkdir -p "$sdk_root/artifacts/$sdk_container"
docker cp "$sdk_container:$sdk_workspace/log" "$sdk_root/artifacts/$sdk_container/"
docker cp "$sdk_container:$sdk_workspace/build/rk3506_driver/test_results" "$sdk_root/artifacts/$sdk_container/" 2>/dev/null || true
printf 'Validation workspace retained: %s:%s\n' "$sdk_container" "$sdk_workspace"
exit "$sdk_result"
