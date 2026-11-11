# handme

A small LAN file transfer tool: one server on your machine, a browser on any
device in the network. Upload, download, and delete files through a clean dark
interface.

## Features

- Upload files with drag & drop or the file picker (multiple files at once)
- Live upload progress
- List, download, and delete files stored in `uploads/`
- Dark, responsive web interface served by the same server
- No database, no accounts — files live in the `uploads/` directory
- Single binary with mongoose vendored in
- Runs on Linux, macOS, and Windows

## Requirements

- Linux/macOS: a C compiler (Clang or GCC) with pthreads support, and `make`
  (optional, but recommended)
- Windows: MSVC (Visual Studio 2022 Build Tools) or MinGW-w64 GCC. `make` is
  not required — `start.ps1` drives the compiler directly.

## Build

On Linux/macOS:

```sh
make
```

On Windows, run the start script; it builds the server when needed:

```
.\start.ps1
```

The binary is written to `build/handme` (`build\handme.exe` on Windows).

## Run

On Linux/macOS:

```sh
./start.sh
```

On Windows, from PowerShell:

```
.\start.ps1
```

or from cmd / by double-clicking:

```
start.cmd
```

Both build the server on first run and then start it in the foreground. If you
have already built it, you can also run `./build/handme` (`build\handme.exe` on
Windows) directly.

The server listens on all interfaces at port `8080`. Open
`http://<your-lan-ip>:8080` on another device to transfer files.

To use a different port:

```sh
PORT=9000 ./start.sh
```

```powershell
$env:PORT = 9000
.\start.ps1
```

```bat
set PORT=9000 && start.cmd
```

On Windows, the firewall may ask for permission the first time the server
listens. Allow access on private networks so other devices can connect.

## Project structure

```
handme/
├── include/mongoose.h   # vendored mongoose header
├── src/mongoose.c       # vendored mongoose implementation
├── src/main.c           # HTTP server, file API, static frontend
├── web/                 # frontend (HTML, CSS, JS)
│   ├── index.html
│   ├── style.css
│   └── app.js
├── uploads/             # uploaded files (created at runtime, git-ignored)
├── build/               # compiled binary (git-ignored)
├── Makefile
├── start.sh             # build + run (Linux/macOS)
├── start.ps1            # build + run (Windows, PowerShell)
└── start.cmd            # build + run (Windows, cmd.exe / double-click)
```

## HTTP API

| Method | Path               | Description                               |
| ------ | ------------------ | ----------------------------------------- |
| GET    | `/files`           | JSON list of files: `{"ok":true,"files":[{"name":"...","size":123}]}` |
| POST   | `/upload`          | Multipart form upload; field name `file`  |
| GET    | `/download?file=`  | Download a file as an attachment          |
| DELETE | `/delete?file=`    | Delete a file                             |

All endpoints return JSON on error, for example
`{"ok":false,"error":"file not found"}`.

Example with `curl`:

```sh
curl -F "file=@photo.jpg" http://localhost:8080/upload
curl http://localhost:8080/files
curl -OJ "http://localhost:8080/download?file=photo.jpg"
curl -X DELETE "http://localhost:8080/delete?file=photo.jpg"
```

## Security notes

- `handme` has no authentication. Anyone who can reach the port can upload,
  download, and delete files. Use it only on a trusted local network.
- Filenames are validated: path separators, control characters, and quotes are
  rejected, so files cannot escape the `uploads/` directory.
- The frontend is served from `web/`; no directory listing is exposed.

## Development

```sh
make format   # clang-format with the LLVM style
make clean    # remove build/
```

The code style is LLVM (see `.clang-format`).

On Windows, `start.ps1` rebuilds only when `src/` or `include/` changed; delete
`build\` to force a full rebuild. If you prefer `make` on Windows (MSYS2, Git
Bash, or a MinGW-w64 toolchain with `mingw32-make`), it works too; pass
`CC=gcc` if GCC is the compiler you have installed.

All file names are handled as UTF-8. On Windows the server converts them to
UTF-16 for the file APIs, so non-ASCII names upload, download, and delete
correctly regardless of the system code page.
