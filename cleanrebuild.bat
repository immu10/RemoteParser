@echo off
cd build
echo Cleaning build directory...
for /d %%x in (*) do rd /s /q "%%x"
del /q *
echo Build directory cleaned.
cmake .. -G "MinGW Makefiles"
cmake --build .
windeployqt fileParser.exe
cd ..
echo Done!
pause