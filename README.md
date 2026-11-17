# morfCollector

*Read in another language: **English** (this document) · [Français](README.fr.md).*

[![Version](https://img.shields.io/badge/version-0.10.2-blue)](CHANGELOG.md)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)
![License](https://img.shields.io/badge/License-GPL--3.0--only-blue)

**Local collection and storage service for the morfSystem fleet.** A provider
(SiteWatch, ...) pushes a manifest of sources; morfCollector pulls the objects
into a local vault and keeps them, without ever interpreting them. It exposes an
HTTP API, announces itself on the LAN (morfBeacon) and installs as a systemd
**or** Windows service.

Connectors currently include local files (`fs`), SFTP, and GitHub traffic
(`github-traffic`) : morfCollector executes a manifest, it never decides what
to collect.

## What it provides

- **Collection engine** - declarative reconciliation of sources from the manifest,
  per-source lifecycle, incremental synchronization, retention.
- **HTTP API** - `GET /status` (morfBeacon-compatible), `GET /healthz`, manifest and
  credentials endpoints, per-source actions (see `docs/fr/CONTRAT.md`).
- **Vault and object store** - secrets and collected objects kept locally.
- **LAN announce** - morfBeacon heartbeat (bundled, no external dependency).
- **Service install** - `service.py` (systemd, Windows services, launchd).

## Build

Only needs **Qt 6** (Core, Network). morfBeacon is vendored under
`third_party/morf/beacon`.

```sh
cmake --preset mingw        # or linux / linux-arm64
cmake --build --preset mingw
```

## Run

```sh
./build-mingw/service/morfcollector.exe
curl http://127.0.0.1:8792/status
```

## Security settings

- **`fs` connector allow-list**: a `fs` source is accepted only under a root listed in
  `fs_allowed_roots` in the admin configuration file (`/etc/morfsystem/morfcollector/`, read-only,
  never changed through the API). Without that list every `fs` source is refused. Symbolic links and
  the vault/store folders are always refused.
  ```json
  "fs_allowed_roots": ["/mnt/nas/logs"]
  ```
- **SFTP host key**: set `host_key_sha256` in the source params (hex, or `SHA256:<base64>` as printed
  by `ssh-keygen -lf`) so the key is checked before any secret is sent. Without it the key is not
  verified and the fingerprint seen is written to the log.
- **Browser writes**: a write (POST/DELETE) carrying an `Origin` different from `Host` gets
  `403 cross_origin_forbidden`; non-browser clients are unaffected.

## Install as a service

```sh
# Any platform: Linux, Windows, Raspberry Pi
sudo ./service.py install      # build if needed, install, start
sudo ./service.py update       # rebuild, replace the binary, restart
sudo ./service.py uninstall    # deregister, keeping your configuration
./service.py status            # what the system says about it
```

One entry point everywhere. What this service is -- its name, its directory,
its configurations -- is declared in `service.json` beside it; the four install
steps live once for the whole parc, and only the service manager differs by
platform (systemd, Windows, launchd).

## Documentation

French documentation in [`docs/fr/`](docs/fr/README.md): the `morfcollect/1` contract,
architecture, and the provider integration guide.

## License

GPL-3.0-only - © 2026 morfredus (Frédéric Biron).
