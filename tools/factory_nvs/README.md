# 出厂 NVS 生成工具

[generate_factory_nvs_from_excel.py](generate_factory_nvs_from_excel.py)将每台设备的 MQTT 参数转成独立 CSV 和 BIN。需要 Python、openpyxl，以及 ESP-IDF 的 nvs_partition_gen.py。从仓库根运行；此命令会创建包含设备密钥的产物，本次未执行。

```powershell
python tools/factory_nvs/generate_factory_nvs_from_excel.py `
  --excel ./private-input/devices.xlsx `
  --mqtt-host broker.example.invalid `
  --mqtt-port 1883 `
  --product-key deskbuddy-v1 `
  --idf-path C:/Espressif/esp-idf `
  --size 0x2000 `
  --output-dir ./private-output/factory
```

域名和路径是无效/占位示例，必须替换成授权隔离环境。输入读取首个 worksheet，前四列表头严格为授权码、设备SN、设备密钥、产品标识；数据行缺 SN/密钥/产品或产品不匹配会跳过。授权码被读入但不写进 NVS。脚本没有完整重复 SN 防护，同名输出可能覆盖，生成前人工检查唯一性。

参数中 --excel/--mqtt-host 必填，--mqtt-port 默认 1883，--product-key 默认 deskbuddy-v1，--idf-path 默认 IDF_PATH，--size 默认 0x2000，--output-dir 默认 tools/factory_nvs/output。若缺 IDF_PATH 或生成器文件会失败。

输出 csv/{deviceId}.csv 与 bin/{deviceId}.bin；namespace 为 factory_cfg，写 mqtt_host/mqtt_port/mqtt_device_id/mqtt_secret/product_key。当前固件读取前四项，product_key 仅随产物保留。生成大小必须匹配[分区表](../../partitions_3m_app.csv)，设备身份文件不与公共固件混发。

生成成功只证明工具输出，不证明设备已注册、秘密正确、TLS 或 MQTT 互通。详细流程见[配置与烧录](../../doc/操作/设备配置与烧录.md)。
