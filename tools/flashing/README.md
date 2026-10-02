# Windows 设备烧录包

工具将同一构建的公共镜像与单台 factory_nvs.bin 配对。入口：[单台打包](package_single_device.py)、[批量打包](package_all_devices.py)、[补充运行时](attach_runtime_to_package.py)。操作从仓库根执行，先完成[出厂 NVS](../factory_nvs/README.md)。

```powershell
python tools/flashing/package_single_device.py --device-id deskbuddy-example --factory-bin-dir ./private-output/factory/bin --skip-runtime
python tools/flashing/package_all_devices.py --prefix deskbuddy- --factory-bin-dir ./private-output/factory/bin --skip-runtime
python tools/flashing/attach_runtime_to_package.py --device-id deskbuddy-example --python-env C:/Espressif/python-env
```

这些占位命令只说明参数，需要实际授权设备的对应 BIN。单台/批量支持 --build-dir（默认 build）、--factory-bin-dir（默认 tools/factory_nvs/output/bin）、--output-dir（默认 tools/flashing/package）、--python-env 和 --skip-runtime。补运行时使用 --package-dir，不是 --output-dir。上述两条打包命令显式消费上一节生成的 private-output/factory/bin；省略参数会读取默认旧目录，打包前核对目录和设备身份，避免混入旧凭据。

单台包复制 bootloader/partition-table/deskpetv1 应用和该设备 BIN，生成一键烧录.bat、设备信息、两份说明及 firmware 目录。未指定 skip-runtime 时尝试复制本机 ESP-IDF Python；批量默认一份 _shared_runtime，单台默认 tools/runtime。不能保证复制任意 Python 环境就能在其他 Windows 机器运行，交付前核验 esptool 与依赖。

[flash_package.bat](flash_package.bat)优先包内运行时，其次同级共享运行时，再尝试系统 Python。它检查镜像存在、提示串口，最终固定执行 ESP32-C3 write_flash：bootloader 0x0、partition-table 0x8000、factory_nvs 0xD000、app 0x10000，波特率 460800。必须与实际 build/flash_args 及分区配置一致；它不会自动推导新分区地址，也不是 OTA。

随包说明：[烧录步骤](README-烧录说明.txt)、[轻量环境准备](README-轻量版环境准备.txt)。这两个文件由打包脚本复制，保持路径可用。每个包只给对应设备，避免泄漏或复用 secret。烧录完成再核对启动和设备身份，工具退出成功不能替代硬件、网络与长期稳定性验收。
