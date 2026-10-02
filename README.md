# DeskPet V1 / DeskBuddy 固件

本仓当前编译 ESP32-C3 的 DeskBuddy 显示型桌宠：128×64 OLED、触摸交互、时钟/天气、WiFi 配网页与 MQTT 服务。实现基线 `a1182c3`；不等同 ESP32-S3 语音固件或桌面模拟器，也没有证据据此宣布本仓废弃或在线运行。

## 按任务阅读

- 理解启动、天气与交互：[运行设计](doc/设计/DeskBuddy运行设计.md)。
- 接 MQTT 与核对模型：[协议与物模型](doc/参考/设备协议与物模型.md)。
- 准备构建、一机一码与烧录：[设备配置与烧录](doc/操作/设备配置与烧录.md)。
- 工具参数：[NVS 生成工具](tools/factory_nvs/README.md)、[Windows 打包工具](tools/flashing/README.md)。
- 产品模型：[DeskBuddy JSON](doc/deskbuddy-v1-thing-model.json)。

当前源码使用 `pet/` 且构造普通 `mqtt://` URI，与当前 Core 的 `cubee/` ACL 有兼容缺口。没有 `writeProperty` 通用处理器、LiveKit 语音或 `ota.switch`；模型可写位不能代替实际服务分发。不要按模拟器或旧通用桌宠模型推断本硬件能力。
