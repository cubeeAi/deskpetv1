# ESP32 NVS 量产烧录方案

本方案适用于当前 `deskpet/deskbuddy` 项目，目标是实现：

- 一份公共固件
- 每台设备一份独立的 MQTT 出厂配置
- Windows 下可供非开发人员使用的一键烧录流程

## 核心思路

### 1. 固件与秘钥分离

应用固件统一使用同一份：

- `bootloader.bin`
- `partition-table.bin`
- `app.bin`

每台设备独有的 MQTT 配置单独烧录到 `factory_nvs` 分区：

- `factory_nvs.bin`

### 2. 固件读取优先级

设备启动时：

1. 优先从 `factory_nvs / factory_cfg` 读取：
   - `mqtt_host`
   - `mqtt_port`
   - `mqtt_device_id`
   - `mqtt_secret`
   - `product_key`
2. 如果读取不到，再 fallback 到编译期默认值

## 分区设计

当前推荐分区：

```csv
# Name,        Type, SubType, Offset,   Size,      Flags
nvs,           data, nvs,     0x9000,   0x4000,
factory_nvs,   data, nvs,     0xD000,   0x2000,    readonly
phy_init,      data, phy,     0xF000,   0x1000,
factory,       app,  factory, 0x10000,  0x300000,
```

## 从 Excel 生成 factory_nvs.bin

Excel 表头要求：

- `授权码`
- `设备SN`
- `设备密钥`
- `产品标识`

生成命令示例：

```powershell
python tools\factory_nvs\generate_factory_nvs_from_excel.py `
  --excel "C:\path\to\licenses-deskbuddy-v1.xlsx" `
  --mqtt-host "43.153.134.2" `
  --mqtt-port 1883 `
  --output-dir "tools\factory_nvs\output"
```

输出：

- `tools/factory_nvs/output/csv/<device_id>.csv`
- `tools/factory_nvs/output/bin/<device_id>.bin`

## Windows 一键烧录包

建议每台设备一个单独烧录包：

```text
Cubee-deskbuddy-v1-000011/
  一键烧录.bat
  README-烧录说明.txt
  firmware/
    bootloader.bin
    partition-table.bin
    app.bin
    factory_nvs.bin
  tools/
    runtime/
```

其中：

- `一键烧录.bat` 可使用 `tools/flashing/flash_package.bat`
- `firmware/factory_nvs.bin` 使用对应设备的专属 bin

## 自动生成单台烧录包

在已经完成 `idf.py build` 且 `factory_nvs.bin` 已生成后，可执行：

```powershell
python tools\flashing\package_single_device.py --device-id deskbuddy-v1-000011
```

执行后会生成：

```text
tools/flashing/package/deskbuddy-v1-000011/
  一键烧录.bat
  README-烧录说明.txt
  设备信息.txt
  firmware/
    bootloader.bin
    partition-table.bin
    app.bin
    factory_nvs.bin
  tools/
    runtime/
```

默认会自动把 ESP-IDF Python 运行时一起打包进单台设备目录，小白机器上无需额外安装 Python 或 `esptool`。

## 批量生成所有设备烧录包

如果整批设备的 `factory_nvs.bin` 都已经生成，可以直接执行：

```powershell
python tools\flashing\package_all_devices.py
```

执行后会自动按设备 SN 批量生成：

```text
tools/flashing/package/_shared_runtime/
tools/flashing/package/deskbuddy-v1-000011/
tools/flashing/package/deskbuddy-v1-000012/
tools/flashing/package/deskbuddy-v1-000013/
...
```

批量打包默认使用共享 Python 运行时，避免每个设备目录重复复制整套运行时。

如果你希望先批量准备所有设备包，但暂时不复制运行时，可以执行：

```powershell
python tools\flashing\package_all_devices.py --skip-runtime
```

后续当你准备把某一台设备交付给用户时，再单独补充运行时：

```powershell
python tools\flashing\attach_runtime_to_package.py --device-id deskbuddy-v1-000011
```

这样只会为对应设备目录复制：

```text
tools/flashing/package/deskbuddy-v1-000011/tools/runtime/
```

## 烧录地址

基于当前项目 `build/flash_args`：

- `bootloader.bin` -> `0x0`
- `partition-table.bin` -> `0x8000`
- `factory_nvs.bin` -> `0xD000`
- `app.bin` -> `0x10000`

## 小白操作流程

1. 安装串口驱动
2. 用 USB 连接设备
3. 双击 `一键烧录.bat`
4. 输入串口号，例如 `COM7`
5. 等待提示“烧录完成，请重新上电设备”

## 注意事项

- `factory_nvs.bin` 必须和设备 SN 对应，不能混用
- 如果更换分区大小，生成 NVS bin 时的 `--size` 要同步修改
- 如果更换分区地址，`一键烧录.bat` 中的 `0xD000` 也要同步修改
