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

## Requirements

- A C compiler (Clang or GCC) with pthreads support
- `make` (optional, but recommended)

## Build

```sh
make
```

The binary is written to `build/handme`.

## Run

```sh
./start.sh
```

or, if you have already built:

```sh
./build/handme
```

The server listens on all interfaces at port `8080`. Open
`http://<your-lan-ip>:8080` on another device to transfer files.

To use a different port:

```sh
PORT=9000 ./start.sh
```

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
└── start.sh
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
