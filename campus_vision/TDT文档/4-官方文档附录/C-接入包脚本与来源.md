> ⬅ 返回目录：[README](../README.md)

## 附录 C：官方接入包脚本与来源说明

### `build.sh`（编译）
```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
set +u
source /opt/ros/jazzy/setup.bash
set -u
cd "$root"
colcon build --base-paths "$root/src" \
    --cmake-args -DPython3_EXECUTABLE=/usr/bin/python3
```

### `start.sh`（启动 TCP 桥）
```bash
#!/usr/bin/env bash
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
[[ -f "$root/install/local_setup.bash" ]] || { printf '请先运行 ./build.sh\n' >&2; exit 1; }
set +u
source /opt/ros/jazzy/setup.bash
source "$root/install/local_setup.bash"
set -u
export ROS_AUTOMATIC_DISCOVERY_RANGE="${ROS_AUTOMATIC_DISCOVERY_RANGE:-LOCALHOST}"
exec ros2 run ros_tcp_endpoint default_server_endpoint --ros-args \
    -p "ROS_IP:=${TDT_ROS_IP:-127.0.0.1}" -p "ROS_TCP_PORT:=${TDT_ROS_PORT:-10000}" "$@"
```

说明：`build.sh` 编译 `src/` 下的 `tdt_interface` 与 `ros_tcp_endpoint`；`start.sh` 以 `ROS_AUTOMATIC_DISCOVERY_RANGE=LOCALHOST` 启动 Unity ROS TCP Endpoint，默认监听 `127.0.0.1:10000`（可用 `TDT_ROS_IP` / `TDT_ROS_PORT` 覆盖）。**这是给算法端单机自检用的 TCP 桥**；正常安装版游戏已内置同样的桥，二者择一即可。

### `ros_tcp_endpoint` 来源
```
Unity-Technologies/ROS-TCP-Endpoint main-ros2
commit 54c1a64b6d5ef6ffa0a0431570bb74329b79b15b
Local packaging: remove development/launch files; initialize executor before
accepting TCP registrations and handle Ctrl+C shutdown in the default entry.
```
包名 `ros_tcp_endpoint`，版本 `0.0.1`，maintainer `Unity Robotics <unity-robotics@unity3d.com>`，Apache 2.0。
