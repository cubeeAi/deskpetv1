# Windows 小白一键烧录方案

这个目录提供一个适合 Windows 用户的“一键烧录包”模板。

## 推荐交付方式

建议按“每台设备一个独立烧录包”交付，目录结构如下：

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
      Scripts/
        python.exe
```

## 固件地址

当前项目基于 `ESP32-C3`，烧录地址按 `build/flash_args` 约定：

- `bootloader.bin` -> `0x0`
- `partition-table.bin` -> `0x8000`
- `app.bin` -> `0x10000`
- `factory_nvs.bin` -> `0xD000`

## 使用方式

1. 把 `flash_package.bat` 复制到烧录包根目录，重命名为 `一键烧录.bat`
2. 把 `README-烧录说明.txt` 复制到烧录包根目录
3. 把对应设备的 `factory_nvs.bin` 放到 `firmware/factory_nvs.bin`
4. 用户双击 `一键烧录.bat`

## 自动打包单台设备

如果你已经有：

- `build/bootloader/bootloader.bin`
- `build/partition_table/partition-table.bin`
- `build/deskpetv1.bin`
- `tools/factory_nvs/output/bin/<device_id>.bin`

可以直接执行：

```powershell
python tools\flashing\package_single_device.py --device-id deskbuddy-v1-000011
```

执行后会生成：

```text
tools/flashing/package/deskbuddy-v1-000011/
  一键烧录.bat
  README-烧录说明.txt
  README-轻量版环境准备.txt
  设备信息.txt
  firmware/
    bootloader.bin
    partition-table.bin
    app.bin
    factory_nvs.bin
  tools/
    runtime/
```

默认会自动把 `C:\Users\<你>\.espressif\python_env\...` 中的 Python 运行时一起打包进单台设备包，小白机器上无需再安装 Python 或 `esptool`。

## 批量打包所有设备

如果你已经生成了整批 `factory_nvs/output/bin/*.bin`，可以直接执行：

```powershell
python tools\flashing\package_all_devices.py
```

执行后会批量生成：

```text
tools/flashing/package/_shared_runtime/
tools/flashing/package/deskbuddy-v1-000011/
tools/flashing/package/deskbuddy-v1-000012/
tools/flashing/package/deskbuddy-v1-000013/
...
```

批量打包默认只复制一份共享 Python 运行时到 `_shared_runtime`，每个设备包里的 `一键烧录.bat` 会优先使用这套共享运行时。
每个设备目录也会自动包含 `README-轻量版环境准备.txt`，方便需要系统 Python 环境的用户直接查看安装步骤。

如果你想限制设备前缀，也可以加：

```powershell
python tools\flashing\package_all_devices.py --prefix deskbuddy-v1-
```

如果你只是想先批量准备所有设备包，不想一开始就复制 Python 运行时，可以这样：

```powershell
python tools\flashing\package_all_devices.py --skip-runtime
```

## 后续给单个设备补充运行时

当你准备把某一台设备的烧录包发给用户时，再执行：

```powershell
python tools\flashing\attach_runtime_to_package.py --device-id deskbuddy-v1-000011
```

这样会只给这一台设备的目录补充：

```text
tools/flashing/package/deskbuddy-v1-000011/tools/runtime/
```

如果你想用指定的 Python 运行时目录，也可以这样：

```powershell
python tools\flashing\attach_runtime_to_package.py --device-id deskbuddy-v1-000011 --python-env "C:\Users\LiChennan\.espressif\python_env\idf5.5_py3.11_env"
```

## 依赖

小白环境建议只准备：

- 串口驱动（CH340 或 CP210x，按你的板子实际型号）

如果烧录包中没有 `tools/runtime` 或 `_shared_runtime`，脚本才会 fallback 到系统 Python。

## 轻量版用户环境准备

如果你准备把“轻量版烧录包”发给已经有一定基础的用户，请一并附带：

- [README-轻量版环境准备.txt](C:/Users/LiChennan/esp/deskpet/deskpetv1/tools/flashing/README-轻量版环境准备.txt)

这份说明会指导用户：

- 安装 Python 3.10+
- 使用 `python -m pip install esptool`
- 使用 `python -m esptool version` 验证环境

官方参考文档：

- [esptool Installation](https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html)
