import argparse
import csv
import os
import subprocess
import sys
from pathlib import Path

import openpyxl


EXPECTED_HEADERS = ("授权码", "设备SN", "设备密钥", "产品标识")


def parse_args():
    parser = argparse.ArgumentParser(description="从 Excel 生成每台设备的 factory_nvs.bin")
    parser.add_argument("--excel", required=True, help="授权码 Excel 文件路径")
    parser.add_argument("--mqtt-host", required=True, help="MQTT Broker 地址")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT Broker 端口")
    parser.add_argument("--product-key", default="deskbuddy-v1", help="只导出指定产品标识，默认 deskbuddy-v1")
    parser.add_argument("--idf-path", default=os.environ.get("IDF_PATH", ""), help="ESP-IDF 根目录，可选")
    parser.add_argument("--size", default="0x2000", help="factory_nvs 分区大小，默认 0x2000")
    parser.add_argument("--output-dir", default="tools/factory_nvs/output", help="输出目录")
    return parser.parse_args()


def resolve_nvs_partition_gen(idf_path: str) -> Path:
    if not idf_path:
        raise FileNotFoundError("未提供 IDF_PATH，无法定位 nvs_partition_gen.py")
    script = Path(idf_path) / "components" / "nvs_flash" / "nvs_partition_generator" / "nvs_partition_gen.py"
    if not script.exists():
        raise FileNotFoundError(f"找不到 nvs_partition_gen.py: {script}")
    return script


def read_rows(excel_path: Path, product_key: str):
    workbook = openpyxl.load_workbook(excel_path, data_only=True)
    worksheet = workbook.worksheets[0]
    rows = list(worksheet.iter_rows(values_only=True))
    if not rows:
        return []

    headers = tuple("" if value is None else str(value).strip() for value in rows[0])
    if headers[:4] != EXPECTED_HEADERS:
        raise ValueError(f"Excel 表头不符合预期: {headers}")

    results = []
    for row in rows[1:]:
        if row is None:
            continue
        license_code, device_sn, device_secret, product = row[:4]
        if not device_sn or not device_secret or not product:
            continue
        if str(product).strip() != product_key:
            continue
        results.append(
            {
                "license_code": str(license_code).strip() if license_code else "",
                "device_sn": str(device_sn).strip(),
                "device_secret": str(device_secret).strip(),
                "product_key": str(product).strip(),
            }
        )
    return results


def write_csv(csv_path: Path, mqtt_host: str, mqtt_port: int, row: dict):
    csv_path.parent.mkdir(parents=True, exist_ok=True)
    with csv_path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["key", "type", "encoding", "value"])
        writer.writerow(["factory_cfg", "namespace", "", ""])
        writer.writerow(["mqtt_host", "data", "string", mqtt_host])
        writer.writerow(["mqtt_port", "data", "u32", mqtt_port])
        writer.writerow(["mqtt_device_id", "data", "string", row["device_sn"]])
        writer.writerow(["mqtt_secret", "data", "string", row["device_secret"]])
        writer.writerow(["product_key", "data", "string", row["product_key"]])


def build_bin(generator: Path, csv_path: Path, bin_path: Path, size: str):
    bin_path.parent.mkdir(parents=True, exist_ok=True)
    command = [sys.executable, str(generator), "generate", str(csv_path), str(bin_path), size]
    subprocess.run(command, check=True)


def main():
    args = parse_args()
    excel_path = Path(args.excel)
    if not excel_path.exists():
        raise FileNotFoundError(f"找不到 Excel 文件: {excel_path}")

    generator = resolve_nvs_partition_gen(args.idf_path)
    rows = read_rows(excel_path, args.product_key)
    if not rows:
        raise RuntimeError("没有找到可导出的设备记录")

    output_dir = Path(args.output_dir)
    csv_dir = output_dir / "csv"
    bin_dir = output_dir / "bin"

    generated = 0
    for row in rows:
        device_id = row["device_sn"]
        csv_path = csv_dir / f"{device_id}.csv"
        bin_path = bin_dir / f"{device_id}.bin"
        write_csv(csv_path, args.mqtt_host, args.mqtt_port, row)
        build_bin(generator, csv_path, bin_path, args.size)
        generated += 1
        print(f"[OK] {device_id} -> {bin_path}")

    print(f"\n完成，共生成 {generated} 个 factory_nvs.bin")


if __name__ == "__main__":
    main()
