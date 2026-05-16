import argparse
from pathlib import Path

from package_single_device import copy_runtime, detect_python_env


def parse_args():
    parser = argparse.ArgumentParser(description="给已生成的单台烧录包补充 ESP-IDF Python 运行时")
    parser.add_argument("--device-id", required=True, help="设备 SN，例如 deskbuddy-v1-000011")
    parser.add_argument("--package-dir", default="tools/flashing/package", help="烧录包根目录，默认 tools/flashing/package")
    parser.add_argument("--python-env", default="", help="可选，指定要复制的 Python 运行时目录")
    return parser.parse_args()


def main():
    args = parse_args()

    root = Path.cwd()
    package_root = root / args.package_dir
    package_dir = package_root / args.device_id
    if not package_dir.exists():
        raise FileNotFoundError(f"找不到设备烧录包目录: {package_dir}")

    runtime_dir = Path(args.python_env) if args.python_env else detect_python_env(root)
    if runtime_dir is None:
        raise FileNotFoundError("未找到可复制的 Python 运行时，请通过 --python-env 指定")

    destination = package_dir / "tools" / "runtime"
    copy_runtime(runtime_dir, destination)

    print(f"[OK] 已为 {args.device_id} 补充运行时: {destination}")


if __name__ == "__main__":
    main()
