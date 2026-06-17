# File Parser

A secure, peer-to-peer file browsing and transfer application built with C++ and Qt 6.

## Overview

File Parser lets two devices connect over the network and browse, download, and
upload files between each other through an encrypted channel. A single executable
can run in either of two roles, chosen at launch:

- **Server** — exposes its local filesystem and listens for an incoming connection.
- **Client** — connects to a server, browses its directory tree remotely, and
  transfers files in both directions.

All traffic is encrypted with TLS, and every file transfer is verified with a
SHA-256 hash to guarantee integrity. Transfers are queued, run in the background,
and can be cancelled mid-flight, with live progress shown in the UI.

### Features

- Role selection at startup (run as Server or Client) via a launch window.
- TLS-encrypted connections using OpenSSL-backed Qt SSL sockets.
- Remote directory browsing with a lazy-loaded tree view.
- Queued downloads **and** uploads with per-transfer progress and cancellation.
- SHA-256 integrity verification on both ends of every transfer.
- Activity logging on the server side.

## Architecture

```
                          ┌───────────────────┐
                          │    LaunchWindow    │
                          │  (choose a role)   │
                          └─────────┬─────────┘
                  Server role       │       Client role
              ┌─────────────────────┴─────────────────────┐
              ▼                                            ▼
   ┌──────────────────────┐                    ┌──────────────────────┐
   │     ServerWindow      │                    │     ClientWindow      │
   │  status + activity    │                    │  remote file browser  │
   │        log            │                    │  + transfer dock      │
   └──────────┬───────────┘                    └──────────┬───────────┘
              ▼                                            ▼
   ┌──────────────────────┐                    ┌──────────────────────┐
   │   Server (QSslServer) │                    │  Client (state machine)│
   │  accepts TLS conns    │                    │  upload/download queues│
   └──────────┬───────────┘                    └──────────┬───────────┘
              │                                            │
              │   creates one Session per client           │
              ▼                                            │
   ┌──────────────────────┐                                │
   │       Session         │                                │
   │  owns both sockets,   │                                │
   │  parses commands,     │                                │
   │  drives FileTransfer  │                                │
   └──────────┬───────────┘                                │
              │                                            │
              │            TLS  (OpenSSL-backed)            │
              │   ┌─────────────────────────────────────┐  │
              ├──▶│  Control channel  (commands/replies) │◀─┤
              │   │  list dir · request · status · error │  │
              │   └─────────────────────────────────────┘  │
              │   ┌─────────────────────────────────────┐  │
              └──▶│  Data channel  (raw file byte stream)│◀─┘
                  │  chunked + SHA-256 verified          │
                  └─────────────────────────────────────┘
```

## Project Structure

| Path | Contents |
|------|----------|
| `src/` | Implementation files (`main.cpp`, `server.cpp`, `client.cpp`, window and widget sources) |
| `include/` | Headers (`server.h`, `client.h`, `launchwindow.h`, `serverwindow.h`, `clientwindow.h`, `transferrow.h`) |
| `ui/` | Qt Designer UI files (`clientwindow.ui`) |
| `certs/` | TLS certificate and key (`server.crt`, `server.key`) |
| `build/` | CMake build artifacts and the compiled executable |
| `generate_certs.bat` | Helper script to generate the self-signed TLS certificate |
| `installer.iss` | Inno Setup script for packaging a Windows installer |

## Tech Stack

| Layer | Technology |
|-------|-----------|
| Language | C++17 |
| GUI / App framework | Qt 6 (6.10.3) — modules: `Widgets`, `Network`, `Concurrent`, `CorePrivate` |
| Networking & security | Qt Network (`QSslServer` / `QSslSocket`) over TLS, OpenSSL (`libssl` / `libcrypto`) backend |
| Integrity | SHA-256 via `QCryptographicHash` |
| Build system | CMake 3.10+ (Qt AUTOMOC / AUTOUIC / AUTORCC) |
| Compiler / toolchain | MinGW (x86_64) |
| Packaging | Inno Setup (`installer.iss`) for Windows distribution |

## Building

### Prerequisites
- CMake 3.10+
- C++17 compatible compiler (MinGW x86_64)
- Qt 6.x (path configured in `CMakeLists.txt` via `CMAKE_PREFIX_PATH`)
- OpenSSL for Windows (DLLs copied into the output directory at build time)

### Build Instructions

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

The build automatically copies the required OpenSSL DLLs and the TLS certificate
and key into the executable's directory.

## Running

```bash
./build/fileParser.exe
```

At startup, choose **Server** to share this machine's files (enter a port) or
**Client** to connect to a server (enter its host and port).

## Note

This project is under active development. More features and documentation coming soon!

## Tags

`Qt` `C++` `SSL Socket`
