import argparse
from pathlib import Path

from package_single_device import copy_runtime, detect_python_env, package_device


def parse_args():
    parser = argparse.ArgumentParser(description="批量生成所有设备的 Windows 烧录包")
    parser.add_argument("--build-dir", default="build", help="固件构建输出目录，默认 build")
    parser.add_argument("--factory-bin-dir", default="tools/factory_nvs/output/bin", help="factory_nvs bin 目录")
    parser.add_argument("--output-dir", default="tools/flashing/package", help="烧录包输出目录")
    parser.add_argument("--prefix", default="deskbuddy-v1-", help="只打包指定前缀的设备 bin，默认 deskbuddy-v1-")
    parser.add_argument("--python-env", default="", help="可选，指定共享 Python 运行时目录")
    parser.add_argument("--skip-runtime", action="store_true", help="不复制共享 Python 运行时")
    return parser.parse_args()


def main():
    args = parse_args()

    root = Path.cwd()
    build_dir = root / args.build_dir
    factory_bin_dir = root / args.factory_bin_dir
    output_dir = root / args.output_dir

    if not factory_bin_dir.exists():
        raise FileNotFoundError(f"找不到 factory_nvs bin 目录: {factory_bin_dir}")

    device_bins = sorted(factory_bin_dir.glob(f"{args.prefix}*.bin"))
    if not device_bins:
        raise RuntimeError(f"没有找到匹配前缀 {args.prefix} 的设备 bin")

    if not args.skip_runtime:
        runtime_dir = Path(args.python_env) if args.python_env else detect_python_env(root)
        if runtime_dir is None:
            raise FileNotFoundError("未找到可打包的 Python 运行时，请通过 --python-env 指定")
        shared_runtime_dir = output_dir / "_shared_runtime"
        copy_runtime(runtime_dir, shared_runtime_dir)
        print(f"[OK] 已打包共享 Python 运行时: {shared_runtime_dir}")

    packaged = 0
    for bin_file in device_bins:
        device_id = bin_file.stem
        package_dir = package_device(root, build_dir, factory_bin_dir, output_dir, device_id)
        packaged += 1
        print(f"[OK] {device_id} -> {package_dir}")

    print(f"\n完成，共生成 {packaged} 个烧录包")


if __name__ == "__main__":
    main()
