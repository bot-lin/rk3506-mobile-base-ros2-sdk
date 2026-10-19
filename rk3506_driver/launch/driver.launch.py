from pathlib import Path

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    default_config = str(Path(get_package_share_directory("rk3506_driver")) / "config" / "driver.yaml")
    return LaunchDescription([
        DeclareLaunchArgument("params_file", default_value=default_config),
        DeclareLaunchArgument("namespace", default_value=""),
        Node(package="rk3506_driver", executable="rk3506_driver_node", name="rk3506_driver",
             namespace=LaunchConfiguration("namespace"), output="screen",
             parameters=[LaunchConfiguration("params_file")]),
    ])
