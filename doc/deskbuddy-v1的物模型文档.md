# 物模型文档

适用范围：DeskBuddy V1  
产品标识：`deskbuddy-v1`

---

## 1. 概述

本文档定义 DeskBuddy 显示型桌宠设备的物模型，用于与 MQTT 网关、设备管理后台及业务服务对接。

物模型包含三大类定义：

- **属性（Property）**：设备运行状态、配置状态和服务端天气结果
- **服务（Service）**：服务端可下发给设备的控制指令
- **事件（Event）**：设备主动上报的重要行为和异常通知

本产品为**显示型桌宠**，聚焦以下能力：

- OLED 表情与页面展示
- 屏幕亮度调节
- 情绪切换
- 本地配置网页
- 本地联网与天气同步

本产品**不包含**以下通用 DeskPet 能力：

- 移动控制
- 电池管理
- 音量控制
- 语音播报
- 动画库下发
- 碰撞、跌落、语音唤醒等传感器事件

---

## 2. 产品与物模型

### 2.1 产品（Product）

产品代表一类设备型号，每个产品包含完整的物模型定义。

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| productKey | string | 产品唯一标识 |
| productName | string | 产品名称 |
| description | string | 产品描述 |
| status | enum | 状态：ACTIVE / DEPRECATED |

### 2.2 默认产品

系统初始化时创建默认产品 `deskbuddy-v1`。

建议产品信息如下：

| 字段 | 值 |
| --- | --- |
| productKey | `deskbuddy-v1` |
| productName | `DeskBuddy 显示型桌宠` |
| description | `基于 ESP32 的显示型桌宠，支持亮度调节、情绪切换、本地天气展示和配置网页` |
| status | `ACTIVE` |

---

## 3. 属性定义（Property）

### 3.1 属性结构

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| identifier | string | 属性标识符 |
| name | string | 属性名称 |
| dataType | enum | 数据类型 |
| accessMode | enum | 访问模式：R（只读）/ RW（读写） |
| required | boolean | 是否必填 |
| specs | object | 数据规格（取值范围、单位、枚举值等） |
| description | string | 属性描述 |

### 3.2 数据类型

| 类型 | 说明 | specs 示例 |
| --- | --- | --- |
| INT | 整数 | `{"min": 0, "max": 100, "unit": "%"}` |
| FLOAT | 浮点数 | `{"min": -50, "max": 80, "unit": "°C"}` |
| BOOL | 布尔值 | `{"0": "否", "1": "是"}` |
| STRING | 字符串 | `{"maxLength": 64}` |
| ENUM | 枚举 | `{"clock": "时钟页", "weather": "天气页"}` |

### 3.3 DeskBuddy 属性列表

#### 3.3.1 运行状态属性

| 标识符 | 名称 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- | --- |
| rssi | 信号强度 | INT | R | WiFi RSSI，单位 dBm |
| version | 固件版本 | STRING | R | 固件版本号 |
| wifiConnected | WiFi 连接状态 | BOOL | R | 是否已连接外部 WiFi |
| configPortalActive | 配置门户状态 | BOOL | R | 是否处于本地配置网页模式 |
| brightness | 屏幕亮度 | INT | RW | 0-100，设备内部可按档位映射 |
| currentPage | 当前页面 | ENUM | R | 当前显示页面 |
| currentMood | 当前情绪 | ENUM | R | 当前表情情绪 |

#### 3.3.2 非敏感配置状态属性

| 标识符 | 名称 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- | --- |
| city | 位置名称 | STRING | R | 本地显示使用的位置名称，可为空 |
| latitude | 纬度 | FLOAT | R | 天气请求使用的纬度 |
| longitude | 经度 | FLOAT | R | 天气请求使用的经度 |
| timezone | 时区 | STRING | R | 设备当前时区 |

#### 3.3.3 天气结果属性

| 标识符 | 名称 | 类型 | 访问 | 说明 |
| --- | --- | --- | --- | --- |
| weatherMain | 天气主状态 | STRING | R | 例如 Clear / Clouds / Rain |
| weatherDesc | 天气描述 | STRING | R | 服务端返回的天气描述文本 |
| temperature | 当前温度 | FLOAT | R | 单位 °C |
| feelsLike | 体感温度 | FLOAT | R | 单位 °C |
| humidity | 湿度 | INT | R | 0-100% |
| weatherReady | 天气数据可用 | BOOL | R | 是否已成功获取当前天气 |
| weatherForecast | 未来三天天气预报 | OBJECT | R | 对象结构，格式为 `{"days":[...]}` |

### 3.4 属性规格详情

#### brightness（屏幕亮度）

```json
{
  "min": 0,
  "max": 100,
  "unit": "%"
}
```

说明：

- 云端可按 0-100 表达亮度值
- 设备内部允许使用分档映射，不要求物理亮度线性一致

#### currentPage（当前页面）

```json
{
  "emo": "表情页",
  "clock": "时钟页",
  "weather": "天气页",
  "worldClock": "世界时钟页",
  "forecast": "天气预报页"
}
```

#### currentMood（当前情绪）

```json
{
  "normal": "普通",
  "happy": "开心",
  "sad": "伤心",
  "angry": "生气",
  "sleepy": "困倦",
  "excited": "兴奋",
  "love": "喜爱",
  "surprised": "惊讶",
  "suspicious": "疑惑"
}
```

---

## 4. 服务定义（Service）

### 4.1 服务结构

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| identifier | string | 服务标识符（对应 MQTT 指令 `type`） |
| name | string | 服务名称 |
| callType | enum | 调用类型：ASYNC / SYNC |
| inputParams | array | 输入参数列表 |
| outputParams | array | 输出参数列表 |
| description | string | 服务描述 |

### 4.2 DeskBuddy 服务列表

| 标识符 | 名称 | 调用类型 | 输入参数 | 说明 |
| --- | --- | --- | --- | --- |
| setEmotion | 设置情绪 | ASYNC | emotion | 设置当前显示情绪 |
| setBrightness | 设置亮度 | ASYNC | brightness | 设置屏幕亮度 |
| setPage | 切换页面 | ASYNC | page | 切换到指定页面 |
| setWeatherConfig | 设置天气配置 | ASYNC | city, latitude, longitude, timezone | 更新天气定位配置并立即生效 |
| reboot | 重启设备 | ASYNC | 无 | 重启设备 |

### 4.3 不适用于 deskbuddy-v1 的通用服务

以下服务虽然出现在通用 MQTT 指令集合中，但**当前产品不实现**：

- `move`
- `stop`
- `speak`
- `playAnimation`
- `setVolume`

处理要求：

- 若收到上述 `cmd.type`，设备应返回：
  - `ok = false`
  - `code = "UNSUPPORTED_TYPE"`
  - `message = "unsupported command for deskbuddy-v1"`

### 4.4 服务参数详情

#### setEmotion（设置情绪）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| emotion | ENUM | 是 | `idle / happy / sad / angry / sleepy / excited / confused` |

设备侧映射关系：

| 云端 emotion | 设备内部 mood |
| --- | --- |
| idle | normal |
| happy | happy |
| sad | sad |
| angry | angry |
| sleepy | sleepy |
| excited | excited |
| confused | suspicious |

说明：

- `love`、`surprised` 为设备本地扩展情绪
- 当前版本仅上报，不作为云端标准可下发值

#### setBrightness（设置亮度）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| brightness | INT | 是 | 0-100 |

#### setPage（切换页面）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| page | ENUM | 是 | `emo / clock / weather / worldClock / forecast` |

说明：

- 设备收到后应立即切换到指定页面
- 若页面值不支持，设备应返回 `BAD_PAYLOAD`
- `page` 的枚举值应与 `currentPage` 属性保持一致

#### setWeatherConfig（设置天气配置）

| 参数 | 类型 | 必填 | 说明 |
| --- | --- | --- | --- |
| city | STRING | 否 | 位置名称，仅用于本地显示 |
| latitude | STRING / FLOAT | 否 | 纬度 |
| longitude | STRING / FLOAT | 否 | 经度 |
| timezone | STRING | 否 | 时区字符串 |

说明：

- 至少需要包含一个支持的字段
- 若更新后 `latitude / longitude / timezone` 为空，设备应返回 `BAD_PAYLOAD`
- 设备保存配置后应立即生效，并补发遥测
- 设备不再保存第三方天气 API Key
- 设备后续应基于已保存的定位配置，通过 MQTT `req/resp` 向服务器请求天气数据

#### reboot（重启设备）

- payload 可为空或省略

---

## 5. 事件定义（Event）

### 5.1 事件结构

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| identifier | string | 事件标识符 |
| name | string | 事件名称 |
| eventType | enum | 事件类型：INFO / ALERT / ERROR |
| outputParams | array | 输出参数列表 |
| description | string | 事件描述 |

### 5.2 DeskBuddy 事件列表

| 标识符 | 名称 | 类型 | 输出参数 | 说明 |
| --- | --- | --- | --- | --- |
| touch | 触摸事件 | INFO | pressType, durationMs, pageBefore, pageAfter | 用户触摸交互 |
| configPortalEntered | 进入配置门户 | INFO | reason | 设备进入本地配置页 |
| wifiConnectFailed | WiFi 连接失败 | ALERT | ssid, reason, retryCount | 外部 WiFi 连接失败 |
| weatherRefreshFailed | 天气刷新失败 | ALERT | stage, statusCode | 服务端天气同步失败 |

### 5.3 不适用于 deskbuddy-v1 的通用事件

以下事件**当前产品不产生**，不应出现在默认物模型中：

- `collision`
- `lowBattery`
- `fall`
- `voiceWakeup`
- `buttonPress`

### 5.4 事件参数详情

#### touch（触摸事件）

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| pressType | ENUM | `short / double / long` |
| durationMs | INT | 触摸时长，单位毫秒 |
| pageBefore | ENUM | 触摸前页面 |
| pageAfter | ENUM | 触摸后页面 |

#### configPortalEntered（进入配置门户）

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| reason | ENUM | `boot_hold / wifi_connect_failed / missing_wifi_config` |

#### wifiConnectFailed（WiFi 连接失败）

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| ssid | STRING | 目标 WiFi SSID |
| reason | INT | 直接对应 ESP WiFi 断开原因码 |
| retryCount | INT | 本轮连接重试次数 |

#### weatherRefreshFailed（天气刷新失败）

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| stage | ENUM | `request / response / parse / timeout` |
| statusCode | INT | HTTP 状态码或内部错误码 |

---

## 6. MQTT 通讯

### 6.1 Topic 规范

DeskBuddy 沿用 MQTT 通讯文档定义的 Topic 规则：

| 方向 | Topic | QoS | 说明 |
| --- | --- | --- | --- |
| 订阅 | `pet/{deviceId}/cmd` | 1 | 下行指令 |
| 订阅 | `pet/{deviceId}/resp` | 1 | 设备主动请求天气等业务后的响应 |
| 发布 | `pet/{deviceId}/cmd/ack` | 1 | 指令回执 |
| 发布 | `pet/{deviceId}/telemetry` | 0 | 遥测上报 |
| 发布 | `pet/{deviceId}/event` | 1 | 事件上报 |
| 发布 | `pet/{deviceId}/req` | 1 | 设备主动发起天气等业务请求 |

### 6.2 DeskBuddy 专用约束

- 天气数据由设备通过 `pet/{deviceId}/req` 发起 `getWeather` 请求，由服务器通过 `pet/{deviceId}/resp` 返回
- 本地配置网页继续保留
- WiFi SSID、WiFi 密码仍仅由本地配置网页管理
- 天气相关配置允许通过 `setWeatherConfig` 指令远程下发
- 设备仅保存 `city`、`latitude`、`longitude`、`timezone` 四类天气定位配置
- 设备不再保存或使用第三方天气 API Key
- 设备仅实现 `setEmotion`、`setBrightness`、`setWeatherConfig`、`reboot` 四类指令
- 收到其他通用 `cmd.type` 时，统一返回 `UNSUPPORTED_TYPE`

### 6.3 天气请求与响应格式

设备请求：

Topic：`pet/{deviceId}/req`

```json
{
  "schemaVersion": 1,
  "reqId": "weather-uuid",
  "type": "getWeather",
  "ts": 1730000000,
  "payload": {
    "city": "Shenzhen",
    "latitude": 22.5431,
    "longitude": 114.0579,
    "timezone": "Asia/Shanghai"
  }
}
```

服务端响应：

Topic：`pet/{deviceId}/resp`

```json
{
  "schemaVersion": 1,
  "reqId": "weather-uuid",
  "type": "getWeather",
  "ok": true,
  "code": "DONE",
  "message": "success",
  "ts": 1730000001,
  "payload": {
    "city": "Shenzhen",
    "timezone": "Asia/Shanghai",
    "weatherMain": "Clouds",
    "weatherDesc": "多云",
    "temperature": 29.5,
    "feelsLike": 33.2,
    "humidity": 78,
    "weatherForecast": {
      "days": [
        {
          "date": "2026-03-28",
          "weatherMain": "Rain",
          "weatherDesc": "小雨",
          "tempMin": 24.1,
          "tempMax": 29.0,
          "humidity": 81
        }
      ]
    }
  }
}
```

说明：

- 设备应优先使用本地已保存的 `city / latitude / longitude / timezone` 作为天气请求参数
- 服务端负责对接第三方天气服务、管理 API Key 和结果转换
- 设备仅消费 `weatherForecast.days` 的前 3 天用于三日预报页展示
- 设备收到成功响应后，应刷新本地显示并在后续遥测中上报最新天气结果

### 6.4 遥测上报格式

DeskBuddy 遥测字段固定建议如下：

```json
{
  "schemaVersion": 1,
  "ts": 1730000000,
  "rssi": -55,
  "brightness": 80,
  "version": "1.0.0",
  "wifiConnected": true,
  "configPortalActive": false,
  "currentPage": "weather",
  "currentMood": "happy",
  "city": "Shenzhen",
  "latitude": 22.5431,
  "longitude": 114.0579,
  "timezone": "CST-8",
  "weatherMain": "Clouds",
  "weatherDesc": "BROKEN CLOUDS",
  "temperature": 29.5,
  "feelsLike": 33.2,
  "humidity": 78,
  "weatherReady": true,
  "weatherForecast": {
    "days": [
      {
        "date": "2026-03-28",
        "dayName": "周六",
        "weatherMain": "Rain",
        "weatherDesc": "小雨",
        "tempMin": 24.1,
        "tempMax": 29.0,
        "humidity": 81
      }
    ]
  }
}
```

说明：

- 不再上报 `battery`
- 不再上报 `volume`
- `city` 仅作为显示用位置名称，可为空
- `latitude / longitude / timezone` 为天气请求所必需的配置状态
- 天气字段表示最近一次服务器返回并被设备接受的天气结果

### 6.5 指令下发格式

```json
{
  "schemaVersion": 1,
  "type": "setBrightness",
  "reqId": "uuid",
  "ts": 1730000000,
  "payload": {
    "brightness": 70
  }
}
```

### 6.6 指令回执格式

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
  "ok": true,
  "code": "DONE",
  "message": "brightness updated",
  "ts": 1730000002
}
```

不支持指令示例：

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
  "ok": false,
  "code": "UNSUPPORTED_TYPE",
  "message": "unsupported command for deskbuddy-v1",
  "ts": 1730000002
}
```

### 6.7 事件上报格式

#### touch 事件示例

```json
{
  "eventId": "touch",
  "eventType": "info",
  "timestamp": 1730000000000,
  "params": {
    "pressType": "double",
    "durationMs": 120,
    "pageBefore": "clock",
    "pageAfter": "clock"
  }
}
```

#### wifiConnectFailed 事件示例

```json
{
  "eventId": "wifiConnectFailed",
  "eventType": "alert",
  "timestamp": 1730000000000,
  "params": {
    "ssid": "Xiaomi",
    "reason": 201,
    "retryCount": 3
  }
}
```

---

## 7. 物模型 API

### 7.1 产品管理

```text
POST   /api/admin/products
GET    /api/admin/products
GET    /api/admin/products/{productKey}
PUT    /api/admin/products/{productKey}
DELETE /api/admin/products/{productKey}
```

### 7.2 属性管理

```text
POST   /api/admin/products/{productKey}/properties
PUT    /api/admin/products/{productKey}/properties/{id}
DELETE /api/admin/products/{productKey}/properties/{id}
```

### 7.3 服务管理

```text
POST   /api/admin/products/{productKey}/services
PUT    /api/admin/products/{productKey}/services/{id}
DELETE /api/admin/products/{productKey}/services/{id}
```

### 7.4 事件管理

```text
POST   /api/admin/products/{productKey}/events
PUT    /api/admin/products/{productKey}/events/{id}
DELETE /api/admin/products/{productKey}/events/{id}
```

### 7.5 导入导出

```text
GET    /api/admin/products/{productKey}/export
POST   /api/admin/products/{productKey}/import
```

---

## 8. 设备事件查询

```text
GET    /api/devices/{deviceId}/events
GET    /api/devices/{deviceId}/events/stats
```

### 8.1 事件历史查询参数

| 参数 | 类型 | 说明 |
| --- | --- | --- |
| eventId | string | 事件标识符（可选） |
| eventType | string | 事件类型（可选） |
| startTime | datetime | 开始时间（可选） |
| endTime | datetime | 结束时间（可选） |
| page | int | 页码（默认0） |
| size | int | 每页大小（默认20） |

### 8.2 事件统计响应示例

```json
{
  "total": 32,
  "byEventId": {
    "touch": 18,
    "configPortalEntered": 4,
    "wifiConnectFailed": 6,
    "weatherRefreshFailed": 4
  },
  "byEventType": {
    "INFO": 22,
    "ALERT": 10,
    "ERROR": 0
  }
}
```

---

## 9. 设备侧实现要求

### 9.1 遥测上报

- 建议频率：5 秒一次
- 必填字段：`schemaVersion`、`ts`、`version`
- 推荐字段：`rssi`、`brightness`、`wifiConnected`、`configPortalActive`、`currentPage`、`currentMood`
- 推荐字段：`city`、`latitude`、`longitude`、`timezone`、`weatherMain`、`weatherDesc`、`temperature`、`feelsLike`、`humidity`、`weatherReady`、`weatherForecast`
- 天气字段表示设备最近一次成功同步的服务端天气结果

### 9.2 事件上报

- 事件发生时立即上报
- `timestamp` 使用毫秒级时间戳
- 触摸事件应区分 `short / double / long`
- WiFi 和天气失败事件建议在关键失败点上报，便于运维诊断
- 天气失败事件建议覆盖请求发送失败、响应超时、响应解析失败等场景

### 9.3 指令处理

- 幂等：缓存最近 N 个 `reqId`，重复不重复执行
- 不支持指令：统一返回 `UNSUPPORTED_TYPE`
- 亮度写入：允许设备内部按档位映射到物理亮度
- `setEmotion` 必须按本文档规定完成云端枚举到本地情绪的映射
- `setWeatherConfig` 成功后应立即持久化并用于后续天气请求
- 设备请求天气时应通过 MQTT `req/resp` 流程向服务器获取，不再直接访问第三方天气 API
- 设备应校验经纬度范围：纬度 `-90 ~ 90`，经度 `-180 ~ 180`

### 9.4 配置策略

- WiFi SSID、WiFi 密码继续由本地配置网页管理
- 云端物模型暴露 `city`、`latitude`、`longitude`、`timezone` 等配置状态
- 云端允许通过 `setWeatherConfig` 修改天气相关配置
- 不提供远程修改 WiFi SSID 或 WiFi 密码的服务定义

### 9.5 回执 code 枚举

| code | 说明 |
| --- | --- |
| DONE | 成功执行 |
| BAD_PAYLOAD | 参数缺失或非法 |
| DUPLICATE | 重复 reqId |
| BUSY | 设备忙 |
| INTERNAL_ERROR | 设备内部异常 |
| UNSUPPORTED_TYPE | 当前产品不支持该指令 |

---

## 10. 与 MQTT 通讯文档的对应关系

为避免实施歧义，DeskBuddy 与 MQTT 文档的关系明确如下：

- 协议通道、Topic、QoS、鉴权方式完全复用 [MQTT通讯文档.md](/c:/Users/LiChennan/esp/deskpet/deskpetv1/doc/MQTT通讯文档.md)
- 本文档只定义 `deskbuddy-v1` 在该协议上的**能力子集**
- MQTT 文档中的通用能力并不自动等于 `deskbuddy-v1` 必须实现
- 本产品启用 MQTT 文档中的天气 `req/resp` 流程，设备侧不再直接访问第三方天气 API
