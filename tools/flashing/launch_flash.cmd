@echo off
setlocal
cd /d "%~dp0"

set "LOG_FILE=%~dp0flash_run.log"

echo ======================================== > "%LOG_FILE%"
echo Cubee Flash Launcher >> "%LOG_FILE%"
echo ======================================== >> "%LOG_FILE%"
echo Started at %date% %time% >> "%LOG_FILE%"
echo. >> "%LOG_FILE%"

echo Launching flash script...
echo A detailed log will be saved to:
echo %LOG_FILE%
echo.

call ".\一键烧录.bat" >> "%LOG_FILE%" 2>&1
set "EXIT_CODE=%ERRORLEVEL%"

echo. >> "%LOG_FILE%"
echo Finished at %date% %time% with exit code %EXIT_CODE% >> "%LOG_FILE%"

echo.
echo Flash script finished with exit code %EXIT_CODE%.
echo If something went wrong, open flash_run.log in this folder.
echo.
pause
endlocal
