Cubee 轻量版烧录包环境准备说明

适用对象：

- 已有一定电脑基础的用户
- 使用“轻量版”烧录包的用户

轻量版烧录包默认不包含 Python 运行时，因此首次使用前，需要先准备：

1. Python 3.10 或更高版本
2. esptool

官方参考文档：

- esptool 安装说明：
  https://docs.espressif.com/projects/esptool/en/latest/esp32/installation.html

推荐安装步骤（Windows）：

1. 安装 Python
   - 下载并安装 Python 3.10 或更高版本
   - 安装时建议勾选“Add Python to PATH”

2. 打开命令行（cmd 或 PowerShell）

3. 检查 Python 是否可用：

   python --version

4. 安装 esptool：

   python -m pip install esptool

5. 检查 esptool 是否安装成功：

   python -m esptool version

如果能正常显示版本号，说明环境已经准备完成，可以继续使用轻量版烧录包。

常见问题：

1. 提示找不到 python
   - 说明 Python 没装好，或没有加入 PATH

2. 提示找不到 pip
   - 可尝试：
     python -m ensurepip --upgrade

3. 安装 esptool 失败
   - 检查网络是否正常
   - 可尝试升级 pip：
     python -m pip install --upgrade pip

4. `python -m esptool` 能运行，但 `esptool` 不能运行
   - 这是正常情况，Windows 下建议始终使用：
     python -m esptool

说明：

- 如果你不想安装 Python 和 esptool，请改用“完整版烧录包”
- 完整版烧录包通常已内置运行时，适合普通用户直接烧录
