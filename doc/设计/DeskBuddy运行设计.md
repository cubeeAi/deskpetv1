# DeskBuddy 运行设计

## 职责与入口

[app_main](../../main/app/app_main.cpp)编排启动与循环；[app_services](../../main/services/app_services.cpp)管理 NVS、WiFi、MQTT、配置门户、时间和天气；[ui_runtime](../../main/ui/ui_runtime.cpp)负责触摸和页面；[display](../../main/display)负责 OLED、文字与资源。[CMake 源表](../../main/CMakeLists.txt)是当前编译范围，不能由历史 Arduino 参考文件推断运行入口。

本产品是显示与轻交互设备，不负责大模型推理、音频采集或移动控制。天气由 MQTT 请求 Core，不让设备持有天气服务 API key；设备凭据与用户 WiFi 配置分区保存，方便同一公共固件配合一机一码。

## 启动与恢复

1. 初始化 NVS、触摸 GPIO、OLED、亮度和网络栈。
2. 读取用户 namespace deskbuddy，以及 factory_nvs/factory_cfg 的 MQTT 参数。
3. 开机触摸保持约 3 秒或 WiFi SSID 缺失时进入配置门户；否则尝试 STA 连接，等待上限 15 秒，失败退回门户。
4. 联网后启动 MQTT、应用时区并同步时间。MQTT CONNECTED 时订阅 cmd/resp、发全量遥测，位置已配置时请求天气。
5. 主循环处理延迟重启、触摸、天气响应超时、周期更新和渲染；配置模式不执行正常页面循环。

MQTT 断开会清理 pending 天气请求。当前 DATA 回调直接用本次 data_len 解析消息，没有完整分片组装或独立命令工作队列，不能照搬其他固件的大报文/并发保证。

## 状态与参数

[app_types](../../main/app/app_types.h)定义触摸 GPIO7、I2C SDA8/SCL9、地址 0x3C、128×64 屏幕和 0x02 列偏移。换屏尤其 SSD1306/SH1106 差异须单独检查，不只改分辨率。

| 参数 | 当前值与含义 |
| --- | --- |
| 开机配置保持 | 3000 ms |
| 长按／双击间隔 | 800 ms／300 ms |
| 全量遥测周期 | 40000 ms，另有变化差量 |
| 天气刷新周期／响应超时 | 600000 ms／15000 ms |
| 重启延迟 | 1500 ms，reboot ACK 发出时尚未证明重启完成 |
| reqId 缓存 | 最近 12 个，内存去重 |

页面有表情、时钟、天气、世界时钟、三日预报与坏点检测；主轮播只在前三页。触摸单击/双击/长按依当前页执行切换、亮度或扩展页动作，细节以 ui_runtime 为准。坏点检测只能观察屏幕，不是整机自检。

## 天气请求与失败

update_weather 先检查 WiFi、MQTT、经纬度/时区和 pending；成功发送后保存 reqId、发送时刻，响应只处理 schemaVersion=1、type=getWeather 及匹配 reqId。成功更新当前天气/预报并上报；解析、平台拒绝、发送或超时错误由 weatherRefreshFailed 与状态文本区分。

NO WIFI、NO MQTT、SET WEATHER、TIMEOUT 分别表示联网、连接、位置和响应问题。已有缓存可带 stale 状态，旧天气仍显示不代表刷新成功；城市名只作显示，实际查询依据经纬度。原始响应日志可能含位置，分享前脱敏。

## 验证边界

维护变更分别检查 NVS 回退、配网失败、MQTT 重连、重复命令、天气关联/超时和页面切换。固件编译、串口、物理亮度、供电、断电恢复需专门测试板验证。代码核对不替代真机验收。
