# MQTT 通讯文档（设备固件对接）

适用范围：DeskPet V0.3
协议风格：MQTT + UTF-8 JSON

---

## 1. 连接参数

| 项目 | 值 |
| --- | --- |
| Broker 地址 | `mqtt-gateway` 所在主机 |
| Broker 端口 | 默认 `1883`（配置项：`mqtt.port`） |
| ClientId | `deviceId` |
| Username | `deviceId` |
| Password | 设备注册时的 `secret` |
| TLS | 可选（如需后续升级可扩展） |

> 鉴权失败时，网关返回连接拒绝（Bad user name or password）。

---

## 2. Topic 与 QoS

| 方向 | Topic | QoS | 说明 |
| --- | --- | --- | --- |
| 订阅 | `pet/{deviceId}/cmd` | 1 | 下行指令 |
| 订阅 | `pet/{deviceId}/resp` | 1 | 设备主动请求的业务响应（V0.6 新增） |
| 发布 | `pet/{deviceId}/cmd/ack` | 1 | 指令回执 |
| 发布 | `pet/{deviceId}/telemetry` | 0 | 遥测上报 |
| 发布 | `pet/{deviceId}/event` | 1 | 事件上报（V0.3 新增） |
| 发布 | `pet/{deviceId}/req` | 1 | 设备主动发起业务请求（V0.6 新增） |

注意：
- 网关强校验 Topic 白名单与 `deviceId` 一致性。
- 在线状态由连接事件决定。
- `req/resp` 用于“设备请求 -> 服务端响应”的业务调用，不替代 `cmd/ack`。

---

## 3. 订阅/发布规则（ACL）

设备只能：
- 订阅 `pet/{deviceId}/cmd`
- 订阅 `pet/{deviceId}/resp`
- 发布 `pet/{deviceId}/telemetry`、`pet/{deviceId}/cmd/ack`、`pet/{deviceId}/event`、`pet/{deviceId}/req`

其他 Topic 会被网关忽略或拒绝。

---

## 4. 消息格式

### 4.1 指令（Core -> Device）

Topic：`pet/{deviceId}/cmd`

```json
{
  "schemaVersion": 1,
  "type": "move",
  "reqId": "uuid",
  "ts": 1730000000,
  "payload": {
    "direction": "forward",
    "speed": 0.6,
    "durationMs": 800
  }
}
```

type 取值（V0.3）：
- `move` - 移动控制
- `stop` - 停止
- `setEmotion` - 设置表情
- `speak` - 语音播报
- `playAnimation` - 播放动画
- `setBrightness` - 设置亮度
- `setWeatherConfig` - 设置天气相关配置
- `setVolume` - 设置音量
- `reboot` - 重启设备

### 4.2 回执（Device -> Core）

Topic：`pet/{deviceId}/cmd/ack`

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
  "ok": true,
  "code": "DONE",
  "message": "moved forward",
  "ts": 1730000002
}
```

建议 code：
`DONE` / `BAD_PAYLOAD` / `SAFETY_LIMIT` / `BUSY` / `DUPLICATE` / `INTERNAL_ERROR`

### 4.3 遥测（Device -> Core）

Topic：`pet/{deviceId}/telemetry`

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

### 4.4 事件（Device -> Core）（V0.3 新增）

Topic：`pet/{deviceId}/event`

```json
{
  "eventId": "collision",
  "eventType": "alert",
  "timestamp": 1730000000000,
  "params": {
    "direction": "front",
    "intensity": 75
  }
}
```

#### 事件类型（eventType）

| 类型 | 说明 |
| --- | --- |
| info | 普通信息事件 |
| alert | 告警事件 |
| error | 错误事件 |

#### 预定义事件（eventId）

| eventId | eventType | 说明 | 参数 |
| --- | --- | --- | --- |
| collision | alert | 碰撞事件 | direction, intensity |
| touch | info | 触摸事件 | position, duration |
| lowBattery | alert | 低电量告警 | level |
| fall | error | 跌落事件 | height |
| voiceWakeup | info | 语音唤醒 | keyword, confidence |
| buttonPress | info | 按键事件 | button, pressType |

### 4.5 设备请求（Device -> Core）（V0.6 新增）

Topic：`pet/{deviceId}/req`

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
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

字段说明：
- `reqId`：请求唯一标识，用于去重、超时控制和匹配响应
- `type`：请求类型，建议使用动作名，如 `getWeather`、`chat`、`getConfig`
- `payload`：请求参数，结构由 `type` 决定

设备请求 type 建议值（V0.6）：
- `getWeather` - 查询天气
- `chat` - 获取对话回复
- `getConfig` - 拉取设备配置

### 4.6 设备请求响应（Core -> Device）（V0.6 新增）

Topic：`pet/{deviceId}/resp`

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
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

失败响应示例：

```json
{
  "schemaVersion": 1,
  "reqId": "uuid",
  "type": "getWeather",
  "ok": false,
  "code": "UPSTREAM_ERROR",
  "message": "weather service unavailable",
  "ts": 1730000001
}
```

建议 code：
`DONE` / `BAD_PAYLOAD` / `UNSUPPORTED_TYPE` / `TIMEOUT` / `UPSTREAM_ERROR` / `RATE_LIMITED` / `INTERNAL_ERROR`

### 4.7 天气查询示例（Device -> Core -> Device）

设备请求：

Topic：`pet/{deviceId}/req`

```json
{
  "schemaVersion": 1,
  "reqId": "7f4d1c9e-8a1a-4d35-8f62-1f7c23b2d111",
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
  "reqId": "7f4d1c9e-8a1a-4d35-8f62-1f7c23b2d111",
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

---

## 5. 指令 payload 说明

### move
- direction：`forward` / `backward` / `left` / `right` / `stop`
- speed：0.0 ~ 1.0（设备需二次限幅）
- durationMs：建议 50 ~ 5000

### stop
- payload 允许为空或省略

### setEmotion
- emotion：`idle` / `happy` / `sad` / `angry` / `sleepy` / `excited` / `confused`

### speak
- text：要播报的文本（最大 200 字符）
- voice：音色 `default` / `cute` / `robot`

### playAnimation
- animation：`wave` / `dance` / `nod` / `shake` / `sleep` / `wakeup`

### setBrightness
- brightness：0 ~ 100

### setWeatherConfig
- city：位置名称，仅用于本地显示（可选）
- latitude：纬度，支持字符串或数值（可选）
- longitude：经度，支持字符串或数值（可选）
- timezone：时区字符串（可选）
- 至少应包含一个支持字段

### setVolume
- volume：0 ~ 100

### reboot
- payload 允许为空或省略

---

## 6. 设备侧处理要求

1. **幂等**：缓存最近 N 个 `reqId`，重复 `reqId` 不重复执行，仅回执。
2. **安全限制**：对 speed/duration 做限幅，触发限制时回执 `SAFETY_LIMIT`。
3. **频率限制**：高频指令可拒绝并回 `BUSY`。
4. **时间戳**：`ts` 使用 Unix 秒级时间戳，事件 `timestamp` 使用毫秒级。
5. **请求匹配**：设备对 `req` / `resp` 必须使用 `reqId` 做一一匹配。
6. **请求超时**：设备发起 `req` 后建议等待 5 秒，超时后本地标记失败，可按业务决定是否重试。
7. **并发控制**：设备同时在途的 `req` 数量建议不超过 3 个，避免响应乱序带来复杂度。
8. **持久化策略**：`resp` 不建议使用 retained message；设备掉线期间的响应默认可丢弃，由上层业务重试。

---

## 7. 建议的上报频率

- **遥测上报**：2~5 秒一次（建议 5 秒）
- **事件上报**：事件发生时立即上报

---

## 8. 交互流程（简版）

1. 设备连接 MQTT，完成鉴权。
2. 设备订阅 `pet/{deviceId}/cmd` 与 `pet/{deviceId}/resp`。
3. Core 下发指令 -> 设备执行 -> 设备回 `cmd/ack`。
4. 设备定期上报 `telemetry`。
5. 设备检测到事件时上报 `event`。
6. 设备需要服务端数据时，发布 `req` -> 网关转发 `pet-core` / `pet-ai` -> 服务端回 `resp`。

### 8.1 设备主动请求天气时序

1. 设备发布 `pet/{deviceId}/req`，`type=getWeather`。
2. mqtt-gateway 校验 Topic 与设备身份，转发请求给 `pet-core`。
3. pet-core 调用内部天气服务或 `pet-ai` 能力组装结果。
4. pet-core 通过 mqtt-gateway 发布 `pet/{deviceId}/resp`。
5. 设备按 `reqId` 匹配响应并播报或展示天气结果。

---

## 9. 版本兼容

- `schemaVersion` 当前为 1。
- 未识别字段应忽略并保持向后兼容。
- 如收到不支持的版本或缺失字段，回执 `BAD_PAYLOAD`。
