import argparse
import shutil
from pathlib import Path


def parse_args():
    parser = argparse.ArgumentParser(description="按设备 SN 生成单台 Windows 烧录包")
    parser.add_argument("--device-id", required=True, help="设备 SN，例如 deskbuddy-v1-000011")
    parser.add_argument("--build-dir", default="build", help="固件构建输出目录，默认 build")
    parser.add_argument("--factory-bin-dir", default="tools/factory_nvs/output/bin", help="factory_nvs bin 目录")
    parser.add_argument("--output-dir", default="tools/flashing/package", help="烧录包输出目录")
    parser.add_argument("--python-env", default="", help="可选，指定要打包的 Python 运行时目录")
    parser.add_argument("--skip-runtime", action="store_true", help="不复制包内 Python 运行时")
    return parser.parse_args()


def require_file(path: Path, label: str):
    if not path.exists():
        raise FileNotFoundError(f"找不到{label}: {path}")


def copy_file(src: Path, dst: Path):
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)


def detect_python_env(root: Path) -> Path | None:
    candidates = []
    espressif_dir = Path.home() / ".espressif" / "python_env"
    if espressif_dir.exists():
        candidates.extend(sorted(espressif_dir.glob("*/Scripts/python.exe")))

    for candidate in reversed(candidates):
        return candidate.parent.parent
    return None


def copy_runtime(runtime_dir: Path, destination_dir: Path):
    destination_dir.parent.mkdir(parents=True, exist_ok=True)
    shutil.copytree(runtime_dir, destination_dir, dirs_exist_ok=True)


def package_device(root: Path, build_dir: Path, factory_bin_dir: Path, output_dir: Path, device_id: str):
    bootloader = build_dir / "bootloader" / "bootloader.bin"
    partition_table = build_dir / "partition_table" / "partition-table.bin"
    app_bin = build_dir / "deskpetv1.bin"
    factory_nvs = factory_bin_dir / f"{device_id}.bin"

    flash_script = root / "tools" / "flashing" / "flash_package.bat"
    flash_readme = root / "tools" / "flashing" / "README-烧录说明.txt"
    lite_readme = root / "tools" / "flashing" / "README-轻量版环境准备.txt"

    require_file(bootloader, "bootloader.bin")
    require_file(partition_table, "partition-table.bin")
    require_file(app_bin, "app.bin")
    require_file(factory_nvs, "factory_nvs.bin")
    require_file(flash_script, "flash_package.bat")
    require_file(flash_readme, "README-烧录说明.txt")
    require_file(lite_readme, "README-轻量版环境准备.txt")

    package_dir = output_dir / device_id
    firmware_dir = package_dir / "firmware"

    copy_file(bootloader, firmware_dir / "bootloader.bin")
    copy_file(partition_table, firmware_dir / "partition-table.bin")
    copy_file(app_bin, firmware_dir / "app.bin")
    copy_file(factory_nvs, firmware_dir / "factory_nvs.bin")
    copy_file(flash_script, package_dir / "一键烧录.bat")
    copy_file(flash_readme, package_dir / "README-烧录说明.txt")
    copy_file(lite_readme, package_dir / "README-轻量版环境准备.txt")

    info_path = package_dir / "设备信息.txt"
    info_path.write_text(
        "\n".join(
            [
                f"设备SN: {device_id}",
                "芯片: ESP32-C3",
                "烧录地址:",
                "  bootloader.bin      -> 0x0",
                "  partition-table.bin -> 0x8000",
                "  factory_nvs.bin     -> 0xD000",
                "  app.bin             -> 0x10000",
            ]
        ),
        encoding="utf-8",
    )

    return package_dir


def main():
    args = parse_args()

    root = Path.cwd()
    build_dir = root / args.build_dir
    factory_bin_dir = root / args.factory_bin_dir
    output_dir = root / args.output_dir
    device_id = args.device_id
    package_dir = package_device(root, build_dir, factory_bin_dir, output_dir, device_id)

    if not args.skip_runtime:
        runtime_dir = Path(args.python_env) if args.python_env else detect_python_env(root)
        if runtime_dir is None:
            raise FileNotFoundError("未找到可打包的 Python 运行时，请通过 --python-env 指定")
        copy_runtime(runtime_dir, package_dir / "tools" / "runtime")

    print(f"[OK] 已生成单台烧录包: {package_dir}")
    print(f"[OK] 固件目录: {package_dir / 'firmware'}")
    if not args.skip_runtime:
        print(f"[OK] 已打包 Python 运行时: {package_dir / 'tools' / 'runtime'}")


if __name__ == "__main__":
    main()
