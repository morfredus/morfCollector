# morfCollector

*Read in another language: **English** (this document) · [Français](README.fr.md).*

[![Version](https://img.shields.io/badge/version-0.4.2-blue)](CHANGELOG.md)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)
![License](https://img.shields.io/badge/License-GPL--3.0--only-blue)

**Reusable skeleton for morfSystem services.** Clone it, code the business logic
in modules, and you get - for free - an HTTP API, a JSON config loader, a LAN
announce (morfBeacon), a systemd service **and** a Windows service, all working
first try.

It is distilled from morfSensor and morfNotify: same architecture, no domain code.

## What you get out of the box

- **`IModule` extension point** - plug your business logic as one or more modules
  (a sensor, a notifier, a collector…). A working `ExampleModule` ships as a
  starting point.
- **HTTP API** (GET + POST) - `GET /status` (morfBeacon-compatible), `GET /healthz`,
  `GET /modules`, `GET /modules/{id}`, and a `POST /example` showing body parsing.
- **Config** - JSON file with a `modules` list; a factory turns it into modules.
- **LAN announce** - morfBeacon heartbeat (bundled, no external dependency).
- **Service install** - `scripts/linux/` (systemd) and `scripts/windows/`
  (Task Scheduler), copying binary + config to a fixed location.
- **Clone helper** - `scripts/new-service.sh` / `.ps1` scaffolds a renamed project.

## Bootstrap a new service

```sh
scripts/new-service.sh morfwatch morfWatch     # Linux/macOS
# scripts\new-service.ps1 morfwatch morfWatch  # Windows
```

Creates `../morfWatch` with every name replaced (`morfCollector` →
`morfWatch`, `morfcollector` → `morfwatch`, `MORFCOLLECTOR` → `MORFWATCH`). It
compiles as-is. Then:

1. Code your logic in `src/ExampleModule.*` (rename it) - implement `IModule`.
2. Register your type(s) in `src/ModuleFactory.cpp` + `knownTypes()`.
3. Adapt the HTTP routes (`src/HttpServer.cpp`) and CMake source list.
4. Update the comments / config / docs.

## Build

Only needs **Qt 6** (Core, Network). morfBeacon is vendored under
`third_party/morf/beacon`.

```sh
cmake --preset mingw        # or linux / linux-arm64
cmake --build --preset mingw
```

## Run

```sh
./build-mingw/service/morfcollector.exe          # 'example' module fallback
curl http://127.0.0.1:8792/modules
```

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

French documentation in [`docs/fr/`](docs/fr/README.md): architecture, and a
step-by-step guide to turn this template into your service.

## License

GPL-3.0-only - © 2026 morfredus (Frédéric Biron).
