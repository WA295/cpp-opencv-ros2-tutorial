> ⬅ 返回目录：[README](../README.md)

## 附录 D：官方文档索引与缺失文档

### 本机可读到的官方文档

| 文档 | 位置 | 内容 |
| --- | --- | --- |
| 发行说明 | `/usr/share/doc/tdt-campus-game/DEBIAN_RELEASE.md.gz` | 见附录 A（安装、启动、ROS、打包、诊断） |
| 许可 | `/usr/share/doc/tdt-campus-game/copyright` | Apache-2.0 |
| 接入包说明 | `~/下载/ros2_auto_framework/README.md` | 见附录 B（话题与接入约定） |
| 接入包脚本 | `~/下载/ros2_auto_framework/{build.sh,start.sh}` | 见附录 C |
| 接口源码 | `/usr/share/tdt-campus-game/tdt_interface/` | 5 个 `.msg` |
| 内置运行时清单 | `/opt/tdt-campus-game/ros-runtime/manifest.json` | ROS Jazzy / Python 3.12 / overlay 包 |

### 发行说明中提到、但本机未随包分发的文档

| 文档 | 被引用处 | 说明 |
| --- | --- | --- |
| `AUTOAIM_SYNC.md` | 发行说明「ROS 订阅」 | 自瞄时间同步与完整控制协议（开发仓库文档） |
| `STARTUP_PRESENTATION.md` | 发行说明「安装与启动」 | 启动画面与应用图标 |
| `SINGLE_PLAYER_JAZZY.md` | 发行说明「从源码生成安装包」 | 源码运行说明 |

> 这三份属于开发仓库文档，安装包内未包含；本机磁盘上也未找到。如需完整协议以 `AUTOAIM_SYNC.md` 为准。
