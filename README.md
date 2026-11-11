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
- Windows: MSVC (Visual Studio 2022 Build Tools) or MinGW-w64 GCC. MSVC is
  driven by `start.ps1`; MinGW-w64 works with either `start.ps1` or
  `make`/`mingw32-make`.

## Build

On Linux/macOS:

```sh
make
```

On Windows, run the start script; it builds the server when needed:

```
.\start.ps1
```

If you have GNU make and MinGW-w64 GCC (the `mingw32-make` that ships with
winlibs, or `make` from MSYS2), the same targets work there as well:

```bat
mingw32-make
mingw32-make clean
mingw32-make run
```

MinGW-w64 distributions name the program `mingw32-make.exe`, so `make` on its
own is not on PATH. Either type `mingw32-make`, or copy it once so both names
work:

```bat
copy "C:\Users\you\Apps\mingw64\bin\mingw32-make.exe" "C:\Users\you\Apps\mingw64\bin\make.exe"
```

The Makefile picks GCC by default on Windows, writes `build\handme.exe`, and
links `ws2_32`. `CC` from the environment or command line still wins, e.g.
`mingw32-make CC=clang`. MSVC is only wired up in `start.ps1`, because `cl.exe`
does not accept the GCC-style command line the Makefile uses.

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
listens. Allow access on the network you are actually on — Windows classifies
new Wi-Fi networks as Public — so other devices can connect. If they cannot,
see [Troubleshooting](#troubleshooting).

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

## Troubleshooting

### Another device cannot connect (Windows)

`http://127.0.0.1:8080/files` answering on the host does not prove the LAN
works: loopback traffic bypasses Windows Defender Firewall. Check the listener
and the machine's own LAN address (`ipconfig` prints it):

```powershell
netstat -ano | findstr :8080          # expect 0.0.0.0:8080 LISTENING
Invoke-WebRequest http://<your-lan-ip>:8080/files
```

If that answers but the phone times out, packets are dropped before they reach
the server. The usual cause is that the firewall's allow rule does not match
the current network profile: Windows marks new Wi-Fi networks as **Public**,
while the rule created by the firewall prompt (and the common
`profile=private,domain`) only covers Private and Domain. Inspect both in an
elevated PowerShell:

```powershell
Get-NetConnectionProfile | Select-Object Name, InterfaceAlias, NetworkCategory
Get-NetFirewallRule -DisplayName '*handme*' |
    Select-Object DisplayName, Direction, Action, Enabled, Profile
```

Fix it either by marking a trusted network as Private, so a Private-scoped rule
applies:

```powershell
Set-NetConnectionProfile -InterfaceAlias 'Wi-Fi' -NetworkCategory Private
```

or by allowing the port on every profile — simpler, but the port is then also
open on public networks and handme has no authentication:

```powershell
netsh advfirewall firewall add rule name="handme 8080" dir=in action=allow protocol=TCP localport=8080 profile=any
```

If you once dismissed the firewall prompt, Windows may have stored a **Block**
rule for `handme.exe`, and block rules win over allow rules:

```powershell
Get-NetFirewallApplicationFilter |
    Where-Object { $_.Program -like '*handme*' } |
    Get-NetFirewallRule | Select-Object DisplayName, Action, Enabled
```

To prove the firewall is the cause, disable it for a moment, test from the
phone, and enable it again:

```powershell
netsh advfirewall set allprofiles state off   # test now
netsh advfirewall set allprofiles state on
```

If the phone still cannot connect with the firewall off, the traffic never
reaches the host. Check that both devices are on the same SSID and subnet
(`ipconfig` on the PC, the Wi-Fi details on the phone), that the phone is not
on a guest network, that the router's AP/client isolation is off, and that no
VPN app is active on the phone.

## Development

```sh
make format   # clang-format with the LLVM style
make clean    # remove build/
```

The code style is LLVM (see `.clang-format`).

On Windows, `start.ps1` rebuilds only when `src/` or `include/` changed; delete
`build\` to force a full rebuild. `make`/`mingw32-make` works on Windows too and
defaults to GCC there; override it with `CC=clang` if clang is what you have.

All file names are handled as UTF-8. On Windows the server converts them to
UTF-16 for the file APIs, so non-ASCII names upload, download, and delete
correctly regardless of the system code page.
