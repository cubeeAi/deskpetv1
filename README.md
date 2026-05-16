# DeskPet V1 / DeskBuddy

这是一个基于 `ESP-IDF v5.5.x` 的 `ESP32-C3 mini` 项目，当前主程序已经切换为 DeskBuddy 风格的显示型桌宠实现，并已经按 `doc/deskbuddy-v1的物模型文档.md` 与 `doc/MQTT通讯文档.md` 的约束接入 MQTT 通道。

当前工程不再包含旧版小车控制页面、运动控制接口或 RoboEyes 依赖。

## 当前功能

- `ESP32-C3` 平台，原生 `ESP-IDF` 构建
- `128x64` I2C OLED 显示
- 表情眼睛动画与多种情绪状态
- 本地时钟与世界时钟页面
- 通过 MQTT `req/resp` 向服务器请求当前天气与 3 日预报
- 触摸单击、双击、长按交互
- WiFi / 位置名称 / 经纬度 / 时区配置门户
- 使用 NVS 持久化保存本地配置
- MQTT 接入：订阅指令、发布遥测、事件与指令回执

## MQTT 接入说明

设备当前按 `deskbuddy-v1` 物模型实现 MQTT 对接。

### MQTT 特性

- 订阅：
  - `pet/{deviceId}/cmd`
  - `pet/{deviceId}/resp`
- 发布：
  - `pet/{deviceId}/cmd/ack`
  - `pet/{deviceId}/telemetry`
  - `pet/{deviceId}/event`
- 当前已实现的下行服务：
  - `setEmotion`
  - `setBrightness`
  - `setPage`
  - `setWeatherConfig`
  - `setWeatherTheme`
  - `reboot`
- 当前未实现的通用服务会回：
  - `UNSUPPORTED_TYPE`

### MQTT 配置方式

MQTT 相关参数**不通过网页配置**。量产设备应通过 `factory_nvs` 分区写入一机一码配置，源码中的常量只作为开发兜底。

请在 [app_types.h](/c:/Users/LiChennan/esp/deskpet/deskpetv1/main/app/app_types.h) 顶部修改以下常量：

```cpp
constexpr char kMqttBrokerHost[] = "43.153.134.2";
constexpr int kMqttBrokerPort = 1883;
constexpr char kMqttDeviceId[] = "deskbuddy-v1-000011";
constexpr char kMqttSecret[] = "REPLACE_WITH_DEVICE_SECRET";
```

说明：

- `Broker Host`：MQTT Broker 地址
- `Broker Port`：默认 `1883`
- `Device ID`：作为 `ClientId` 与 `Username`
- `Secret`：作为 MQTT `Password`

如果 `factory_nvs` 中没有真实设备密钥，且 `kMqttSecret` 仍是占位值，设备会自动禁用 MQTT 并在串口日志中提示。

## 目录结构

- `main/app/app_main.cpp`：DeskBuddy 主程序入口
- `main/include/arduino_compat.h`：轻量 Arduino 兼容工具，仅提供 `millis/random`
- `Arduino/deskbuddy.ino`：原始 Arduino 参考实现
- `doc/MQTT通讯文档.md`：MQTT 通讯协议说明
- `doc/deskbuddy-v1的物模型文档.md`：DeskBuddy 专用物模型
- `main/CMakeLists.txt`：主组件构建配置

## 硬件映射

- `TOUCH = GPIO7`
- `SDA = GPIO8`
- `SCL = GPIO9`
- OLED 地址：`0x3C`
- OLED 分辨率：`128x64`
- 若使用 `1.3` 寸 `SH1106` 屏，当前固件已按 `0x02` 列偏移输出，避免直接沿用 `SSD1306` 起始列导致画面左右偏移

## 运行逻辑

### 启动

1. 初始化 NVS、触摸输入和 OLED
2. 读取 `deskbuddy` NVS namespace
3. 若开机时触摸脚保持高电平约 3 秒，进入配置门户
4. 若没有已保存的 WiFi SSID，进入配置门户
5. 否则尝试连接 WiFi
6. WiFi 成功后：
   - 启动 MQTT
   - 同步时间
   - 通过 MQTT 请求服务器天气
   - 进入主循环

### 配置门户

设备进入配置模式后会开启热点：

- SSID：`DeskBuddy-Setup`
- Password：`12345678`

浏览器访问：

- `http://192.168.4.1/`

可配置字段：

- WiFi SSID
- WiFi Password
- Location Name（可选，仅本地显示）
- Latitude
- Longitude
- Timezone

保存后设备会自动重启。

注意：

- MQTT Broker、`deviceId`、`secret` 不在网页中配置
- 这些参数由固件常量决定

### 页面与交互

- `Page 0`：表情页
- `Page 1`：时钟页
- `Page 2`：天气页
- `Page 3`：世界时钟页
- `Page 4`：3 日预报页
- `Page 5`：坏点检测页（支持手动切换白屏、黑屏、竖线、横线、棋盘格、十字边框）

交互规则：

- 单击：在主页面间切换；若在扩展页则返回对应主页面
- 双击：切换屏幕亮度档位
- 长按：表情页切换情绪；时钟页进入世界时钟；天气页进入预报；预报页进入坏点检测；坏点检测页返回预报
- 坏点检测操作：进入后默认先看白屏，单击依次切换黑屏、竖线、横线、棋盘格、十字边框，继续单击会回到白屏
- 自动轮播：仅在 `0-2` 页面间循环

## 遥测与事件

当前遥测上报字段包括：

- `rssi`
- `brightness`
- `version`
- `wifiConnected`
- `configPortalActive`
- `currentPage`
- `currentMood`
- `city`
- `latitude`
- `longitude`
- `timezone`
- `weatherMain`
- `weatherDesc`
- `temperature`
- `feelsLike`
- `humidity`
- `weatherReady`
- `weatherForecast`
  - 对象结构：`{"days":[...]}`

当前事件上报包括：

- `touch`
- `configPortalEntered`
- `wifiConnectFailed`
- `weatherRefreshFailed`

## 构建

请先进入 ESP-IDF 提供的终端环境，再在项目根目录执行：

```bash
idf.py set-target esp32c3
idf.py build
```

烧录与串口监视：

```bash
idf.py -p COM7 flash monitor
```

请将 `COM7` 替换为你的实际串口号。

## 调试建议

串口日志中可重点关注以下关键字：

- `app_main start`
- `connected to wifi`
- `mqtt starting`
- `mqtt connected`
- `got ip`
- `wifi disconnected`
- `wifi connect failed`
- `weather updated from mqtt`

如果设备进入配置页，常见原因通常是：

- WiFi 连接失败
- 开机时触摸脚被误判为长按
- 没有已保存的 WiFi 配置

## 说明

- 天气由设备发布 `pet/{deviceId}/req` 的 `getWeather` 请求，再由服务器回到 `pet/{deviceId}/resp`
- 若未配置位置经纬度与时区，天气页面会提示进入配置门户设置
- 当前仓库中的 [deskbuddy.ino](/c:/Users/LiChennan/esp/deskpet/deskpetv1/Arduino/deskbuddy.ino) 仅作为迁移参考，不再作为运行入口
- 当前 MQTT 接入能力以 [deskbuddy-v1的物模型文档.md](/c:/Users/LiChennan/esp/deskpet/deskpetv1/doc/deskbuddy-v1的物模型文档.md) 为准
