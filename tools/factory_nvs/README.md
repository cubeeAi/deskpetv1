# Cubee Factory NVS 工具说明

这一组工具用于把 Excel 里的“一机一码”设备信息转换成 ESP32 可烧录的 `factory_nvs.bin`。

## 依赖

- Windows
- Python 3.11+
- `openpyxl`
- 已安装 ESP-IDF，或至少能访问 `nvs_partition_gen.py`

## 输入来源

当前默认适配的 Excel 表头：

- `授权码`
- `设备SN`
- `设备密钥`
- `产品标识`

其中：

- `设备SN` -> `mqtt_device_id`
- `设备密钥` -> `mqtt_secret`
- `产品标识` -> `product_key`

## 生成内容

脚本会输出两类文件：

- `output/csv/<device_id>.csv`
- `output/bin/<device_id>.bin`

CSV 用于审阅，BIN 用于烧录到 `factory_nvs` 分区。

## 典型用法

```powershell
python tools\factory_nvs\generate_factory_nvs_from_excel.py `
  --excel "C:\path\to\licenses-deskbuddy-v1.xlsx" `
  --mqtt-host "43.153.134.2" `
  --mqtt-port 1883 `
  --output-dir "tools\factory_nvs\output"
```

如果 `IDF_PATH` 没有配置，可以显式传入：

```powershell
python tools\factory_nvs\generate_factory_nvs_from_excel.py `
  --excel "C:\path\to\licenses-deskbuddy-v1.xlsx" `
  --idf-path "C:\Espressif\frameworks\esp-idf-v5.5.1"
```

## 输出的 NVS 键

每台设备的 `factory_nvs.bin` 中会包含这些键：

- `mqtt_host`
- `mqtt_port`
- `mqtt_device_id`
- `mqtt_secret`
- `product_key`

固件启动时会优先从 `factory_nvs/factory_cfg` 读取这些键；如果读不到，才会 fallback 到编译期默认值。

## 分区要求

当前项目约定：

- 分区名：`factory_nvs`
- namespace：`factory_cfg`
- 分区大小：`0x2000`

如果你修改了分区大小，生成脚本里的 `--size` 也要同步调整。
