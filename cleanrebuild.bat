@echo off
cd build
echo Cleaning build directory...
for /d %%x in (*) do rd /s /q "%%x"
del /q *
echo Build directory cleaned.
cmake .. -G "MinGW Makefiles"
cmake --build .
if errorlevel 1 (
    echo.
    echo Build FAILED - skipping installer.
    cd ..
    pause
    exit /b 1
)
windeployqt fileParser.exe
cd ..

echo.
echo Building installer...
set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
    echo Inno Setup not found at "%ISCC%" - skipping installer.
) else (
    "%ISCC%" installer.iss
    if errorlevel 1 (
        echo Installer build FAILED.
    ) else (
        echo Installer ready: dist\fileParser-setup.exe
    )
)

echo Done!
pause
