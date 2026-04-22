# File Parser

A file parsing application built with C++ and Qt.

## Overview

A file parser application that allows you to access and view files across two connected devices. This project consists of two parts:

**Part 1 (Current):** File parser executable with Qt GUI
**Part 2 (Planned):** Docker containerization for remote deployment

## Use Case

Connect and access files between two devices.

## Building

### Prerequisites
- CMake 3.16+
- C++17 compatible compiler
- Qt 6.x

### Build Instructions

```bash
mkdir build
cd build
cmake ..
make
```

## Running

```bash
./fileParser.exe
```

Or run the pre-built executable:
```bash
./build/fileParser.exe
```

## Project Structure

- `src/` - Source files (main.cpp, mainwindow.cpp)
- `include/` - Header files
- `ui/` - Qt UI files
- `build/` - Build artifacts and executable

## License

[Add your license here]

## Note

This project is under active development. More features and documentation coming soon!
