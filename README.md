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

The application is organized around two roles and a dual-socket session model.

### Roles & UI

- **`LaunchWindow`** — the entry screen. Lets the user pick a role and enter the
  port (server) or host/port (client). Also displays the machine's local
  addresses to ease connection setup.
- **`ServerWindow`** — server-side dashboard showing listen status, connected
  clients, and a live activity log.
- **`ClientWindow`** — the file browser. Presents the remote filesystem as a tree
  and list view, a path bar with navigation, a context menu for download/upload
  actions, and a transfer dock listing active and queued transfers.
- **`TransferRow`** — a widget representing a single queued or active transfer
  (download or upload) with status and a cancel button.

### Connection model

Each client/server session uses **two sockets**:

- A **control channel** for commands and responses (directory listings, transfer
  requests, status, errors).
- A **data channel** for the raw file byte stream.

On the server, `Server` accepts incoming TLS connections via `QSslServer` and
creates a `Session` for each client. A `Session` owns both sockets, parses control
commands, and drives transfers. Outgoing file streaming is handled by a dedicated
`FileTransfer` object that writes file data over the socket in backpressure-aware
chunks (`writeMore` / `bytesWritten`).

### Flow chart

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

### Source layout

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

`qt` `c++` `sslsocket`
