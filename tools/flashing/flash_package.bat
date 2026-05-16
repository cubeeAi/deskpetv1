@echo off
setlocal EnableDelayedExpansion

set "ROOT=%~dp0"
set "FIRMWARE_DIR=%ROOT%firmware"
set "BOOTLOADER=%FIRMWARE_DIR%\bootloader.bin"
set "PARTITION_TABLE=%FIRMWARE_DIR%\partition-table.bin"
set "APP_BIN=%FIRMWARE_DIR%\app.bin"
set "FACTORY_NVS=%FIRMWARE_DIR%\factory_nvs.bin"
set "BUNDLED_PYTHON=%ROOT%tools\runtime\Scripts\python.exe"
set "SHARED_PYTHON=%ROOT%..\_shared_runtime\Scripts\python.exe"
set "PYTHON_CMD="

echo ========================================
echo Cubee Flash Tool
echo ========================================
echo.

if not exist "%BOOTLOADER%" goto missing_bootloader
if not exist "%PARTITION_TABLE%" goto missing_partition
if not exist "%APP_BIN%" goto missing_app
if not exist "%FACTORY_NVS%" goto missing_factory

set "PORT=%~1"
if "%PORT%"=="" goto ask_port
goto port_ready

:ask_port
echo Available serial ports:
set "PORT_COUNT=0"
set "LAST_PORT="
for /f "usebackq delims=" %%I in (`powershell -NoProfile -Command "$ports = [System.IO.Ports.SerialPort]::GetPortNames(); [Array]::Sort($ports); $ports" 2^>nul`) do (
  echo   %%I
  set "LAST_PORT=%%I"
  set /a PORT_COUNT+=1
)
if "!PORT_COUNT!"=="0" goto no_ports
if "!PORT_COUNT!"=="1" goto one_port
goto many_ports

:no_ports
echo   none detected
echo.
echo [INFO] Please check USB cable, driver, and whether the board is powered on.
echo.
set /p PORT=Enter serial port manually (example COM7): 
goto port_input_done

:one_port
echo.
echo [INFO] Suggested port: !LAST_PORT!
set /p PORT=Enter serial port [!LAST_PORT!]: 
if "!PORT!"=="" set "PORT=!LAST_PORT!"
goto port_input_done

:many_ports
echo.
set /p PORT=Enter serial port (example COM7): 

:port_input_done
if "%PORT%"=="" goto missing_port

:port_ready
if exist "%BUNDLED_PYTHON%" set "PYTHON_CMD=%BUNDLED_PYTHON%"
if "%PYTHON_CMD%"=="" if exist "%SHARED_PYTHON%" set "PYTHON_CMD=%SHARED_PYTHON%"

if "%PYTHON_CMD%"=="" (
  python -m esptool version >nul 2>nul
  if errorlevel 1 goto missing_python
  set "PYTHON_CMD=python"
)

echo.
echo [INFO] PORT=%PORT%
echo [INFO] BOOTLOADER=%BOOTLOADER%
echo [INFO] PARTITION_TABLE=%PARTITION_TABLE%
echo [INFO] FACTORY_NVS=%FACTORY_NVS%
echo [INFO] APP_BIN=%APP_BIN%
echo [INFO] PYTHON=%PYTHON_CMD%
echo.

"%PYTHON_CMD%" -m esptool --chip esp32c3 --port %PORT% --baud 460800 write_flash 0x0 "%BOOTLOADER%" 0x8000 "%PARTITION_TABLE%" 0xD000 "%FACTORY_NVS%" 0x10000 "%APP_BIN%"
if errorlevel 1 goto flash_failed

echo.
echo [OK] Flash completed. Power-cycle the device.
echo.
pause
goto end

:missing_bootloader
echo [ERROR] Missing bootloader.bin:
echo %BOOTLOADER%
goto fail

:missing_partition
echo [ERROR] Missing partition-table.bin:
echo %PARTITION_TABLE%
goto fail

:missing_app
echo [ERROR] Missing app.bin:
echo %APP_BIN%
goto fail

:missing_factory
echo [ERROR] Missing factory_nvs.bin:
echo %FACTORY_NVS%
goto fail

:missing_port
echo [ERROR] Serial port was not provided.
goto fail

:missing_python
echo [ERROR] No bundled runtime found, and system Python/esptool is unavailable.
goto fail

:flash_failed
echo.
echo [ERROR] Flash failed.
goto fail

:fail
echo.
pause
exit /b 1

:end
endlocal
