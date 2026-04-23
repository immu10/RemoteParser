@echo off
cd build
cmake --build . --target clean
cmake --build .
windeployqt fileParser.exe
cd ..
echo Done!
pause