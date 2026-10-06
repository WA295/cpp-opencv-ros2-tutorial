> ⬅ 返回目录：[README](../README.md)

## 附录 A：官方发行说明（`DEBIAN_RELEASE.md`，v1.1.0 全文）

### Linux deb 安装版发布

版本 **1.1.0**，目标 **Ubuntu 24.04 amd64（x86_64）**。安装包包含 Unity 游戏、ROS 2 Jazzy Endpoint、图像/状态 Relay、`tdt_interface` 和 Python/ROS 运行依赖。玩家无需安装 Unity、Python 或 ROS，也不用手动 source 环境或启动通信桥；系统提供桌面环境、glibc 和显卡驱动。其他发行版、旧版 Ubuntu 和 ARM 不在此版本验证范围内。

#### 安装与启动

在收到安装包的目录打开终端：

```bash
sudo apt install ./tdt-campus-game_1.1.0_amd64.deb
```

安装后在应用菜单搜索 **T-DT 校园赛 2027**（英文环境为 **T-DT Campus Game 2027**），点击图标启动。应用菜单与窗口图标使用 `Assets/Resource/image/app-icon-rounded.png`，为原图 `gj.png` 加上 Apple 官方模板精确轮廓的连续圆角版本。也可以从任意工作目录运行：

```bash
tdt-campus-game
```

启动器预留通信端口后即启动 Unity，内置通信桥同时初始化，Unity 启动不等待桥就绪。有 `DISPLAY` 且 Player 目录包含 `libtdt_startup_window.so` 时，启动器仅为 Unity 子进程预加载该库，将同一 X11 主窗口的初始背景设为不透明黑色，窗口随正常创建流程显示。游戏随后在该窗口中进入轻量 `Startup` 场景，以 `Assets/Resource/image/startup-landscape.png` 16:9 横版扩图铺满窗口，用约 1.5 秒从黑色平滑渐亮，同时异步加载 `Menu`；渐亮完成且菜单首帧准备好后，以约 0.15 秒让照片淡出、菜单显现，在原窗口中完成交接，无额外停留。窗口和菜单始终不透明，转场结束后恢复输入。约 1.5 秒指渐亮动画时长，实际启动耗时还取决于进程初始化和场景加载。缺少 `DISPLAY` 或该库时使用常规 Unity 开屏。

开屏图片包含在 Unity Player 资源中，启动器不创建独立开屏窗口，也不执行窗口位置或尺寸跟随。该图由 imagegen 生成，只用于开屏。更新图片后需重新构建 Player 并打包，导入设置与原图保留规则见 [启动画面与应用图标](STARTUP_PRESENTATION.md)。

选择阵营和控制模式，进入对局后即可发布所选机器人的 ROS 图像和消息。菜单阶段没有比赛图像。全手动、两种半自动和全自动模式都支持发布；自动移动、瞄准和射击需要外部算法发布控制指令，安装包不包含自动控制算法。

需要调整采集目标或 ROS domain 时，从终端启动：

```bash
ROS_DOMAIN_ID=7 TDT_SENSOR_FPS=60 tdt-campus-game
```

默认 domain 为 `0`、图像采集目标为 `120 Hz`，实际接收频率取决于图形性能及传输负载。标准图像输出为 `960×720` JPEG。游戏参数也可直接传入，例如 `tdt-campus-game -screen-fullscreen 0`。

卸载：

```bash
sudo apt remove tdt-campus-game
```

卸载会移除应用和菜单入口，用户设置与日志保留在用户目录。不要使用 sudo 启动游戏。

#### ROS 订阅

应用启动时先预留通信端口，再启动游戏，并同时初始化内置 Endpoint 和 Relay；游戏窗口的启动不等待通信桥就绪，关闭游戏会结束本次启动的通信桥。默认连接 `127.0.0.1:10000`，该端口被占用时会自动分配空闲端口并传给游戏。已保存的远程 IP 或 ROS 禁用设置不会阻止本次自动连接，F8 可查看连接状态。

算法端使用相同的 `ROS_DOMAIN_ID` 和兼容的消息定义。标准压缩图像类型是 `sensor_msgs/msg/CompressedImage`；自定义消息定义安装在 `/usr/share/tdt-campus-game/tdt_interface/`，也可从仓库 `tdt_interface/` 获取。修改接口后应同步重建两端。

在安装了 ROS 2 Jazzy 的算法终端中：

```bash
source /opt/ros/jazzy/setup.bash
export ROS_DOMAIN_ID=0
ros2 topic list -t
ros2 topic hz /camera/image_raw/compressed
```

如算法工作区尚无自定义消息包，可从安装版复制并构建（需要已安装 colcon 和 ROS 消息生成工具）：

```bash
mkdir -p ~/tdt_ros_ws/src
cp -R /usr/share/tdt-campus-game/tdt_interface ~/tdt_ros_ws/src/
cd ~/tdt_ros_ws
colcon build --packages-select tdt_interface
source install/setup.bash
```

已构建接口时，只需 source 对应工作区，再执行：

```bash
ros2 topic echo /autoaim/state
```

主要话题：

| 话题 | 类型 / 用途 |
| --- | --- |
| `/autoaim/frame` | `tdt_interface/msg/AutoAimFrame`，同步图像、相机参数和状态 |
| `/autoaim/state` | `tdt_interface/msg/AutoAimState`，自身状态 |
| `/camera/image_raw/compressed` | `sensor_msgs/msg/CompressedImage`，标准 JPEG 图像 |

完整协议及控制指令见 [自瞄时间同步说明](AUTOAIM_SYNC.md)。内置 ROS 运行库仅供应用自身使用，不修改算法终端的系统 ROS 环境。

#### 从源码生成安装包

构建机使用 Ubuntu 24.04 amd64，安装项目版本的 Unity Linux Build Support、ROS 2 Jazzy、colcon，以及 `dpkg-deb`、Python 3 和 `desktop-file-validate`。主窗口启动库的编译还需要 `cc`（可由 `build-essential` 安装）和 `libx11-dev`。ROS 构建准备见 [源码运行说明](SINGLE_PLAYER_JAZZY.md#构建与启动)。保存并关闭使用此工程的 Unity 编辑器，在项目根目录执行：

```bash
./scripts/build_linux.sh
bash scripts/setup_ros_tcp_endpoint.sh
./scripts/package_deb.sh
```

`scripts/build_linux.sh` 在 Unity Player 构建成功后自动调用 `scripts/build_startup_window.sh`，将 `libtdt_startup_window.so` 编译到 Player 根目录；deb 封包会随 Player 一起复制该库。

发布版本读取 `ProjectSettings/ProjectSettings.asset` 中的 `bundleVersion`，当前为 `1.1.0`。修改版本后先重新构建 Player，再封包；打包脚本使用已有 Player，不会自动重编译游戏。

封包时使用内置 Python 为标准库生成可搬移的哈希校验字节码缓存，减少每次启动时的解释器编译开销；运行时仍保持只读并禁止写入缓存。

默认产物：

```text
Builds/Release/tdt-campus-game_1.1.0_amd64.deb
Builds/Release/tdt-campus-game_1.1.0_amd64.deb.sha256
```

脚本同时生成 SHA-256 校验文件。分发 `.deb` 即可，玩家不需要源码或相邻的 Unity 资源目录。

其他输入与输出位置可通过 `./scripts/package_deb.sh --help` 查看。相对路径按项目根目录解析，输出须在 Player 和 ROS 工作区之外；图标默认使用 `Assets/Resource/image/app-icon-rounded.png`。`--icon` 只替换安装后的应用菜单图标，Player 图标与开屏由 Unity 构建配置决定。打包在临时目录进行，不修改输入 Player，排除 Unity 标为不应分发的调试/备份目录，ROS 工作区符号链接在封包时转为实际文件。

#### 安装布局与诊断

| 路径 | 内容 |
| --- | --- |
| `/opt/tdt-campus-game/` | 游戏资源、内置 ROS/Python 运行库和启动管理器 |
| `/usr/bin/tdt-campus-game` | 命令入口 |
| `/usr/share/applications/tdt-campus-game.desktop` | 应用菜单入口 |
| `/usr/share/pixmaps/tdt-campus-game.png` | 应用图标 |
| `/usr/share/doc/tdt-campus-game/` | 安装说明与许可 |
| `/usr/share/tdt-campus-game/tdt_interface/` | 供算法端编译的 ROS 自定义消息包源码 |
| `~/.local/state/tdt-campus-game/logs/` | 每次启动的独立日志目录 |

设置了绝对路径的 `XDG_STATE_HOME` 时，日志写入该目录下的 `tdt-campus-game/logs/`。查看 `launcher.log`、`endpoint.log`、`relay.log` 和 `unity.log` 可区分通信桥启动问题与游戏问题。应用目录由 root 持有，普通用户运行不需写入安装目录。

维护者应验证安装包的版本、架构、文件权限和桌面入口，并从安装路径启动游戏，进入对局实际订阅图像和状态，再关闭窗口检查桥进程退出。构建或 ROS 节点注册成功不能替代图像实收检查。

打包格式参考 [dpkg-deb 官方手册](https://manpages.debian.org/bookworm/dpkg/dpkg-deb.1.en.html)，菜单入口字段遵循 [freedesktop 桌面入口规范](https://specifications.freedesktop.org/desktop-entry/latest/recognized-keys.html)。

> 附：本机 `DEBIAN_RELEASE.md` 路径与读取方式
> ```bash
> zcat /usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz
> ```
