# morfCollector

*Lire dans une autre langue : [English](README.md) · **Français** (ce document).*

[![Version](https://img.shields.io/badge/version-0.11.1-blue)](CHANGELOG.md)
![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus)
![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt)
![Build](https://img.shields.io/badge/CMake-3.21+-064F8C?logo=cmake)
![License](https://img.shields.io/badge/License-GPL--3.0--only-blue)

**Service de collecte et de conservation locale du parc morfSystem.** Un
fournisseur (SiteWatch, ...) pousse un manifeste de sources ; morfCollector
récupère les objets dans un coffre local et les conserve, sans jamais les
interpréter. Il expose une API HTTP, s'annonce sur le LAN (morfBeacon) et
s'installe comme service systemd **ou** Windows.

Connecteurs actuels : fichiers locaux (`fs`), SFTP, et trafic GitHub
(`github-traffic`). morfCollector exécute un manifeste ; il ne décide jamais
quoi collecter.

## Ce qu'il fournit

- **Moteur de collecte** - réconciliation déclarative des sources du manifeste,
  cycle de vie par source, synchronisation incrémentale, rétention.
- **API HTTP** - `GET /status` (compatible morfBeacon), `GET /healthz`, points
  d'entrée manifeste et secrets, actions par source (voir `docs/fr/CONTRAT.md`).
- **Coffre et magasin d'objets** - secrets et objets collectés conservés localement.
- **Annonce LAN** - heartbeat morfBeacon (embarqué, aucune dépendance externe).
- **Installation service** - `service.py` (systemd, services Windows, launchd).

## Compiler

Nécessite seulement **Qt 6** (Core, Network). morfBeacon est vendoré dans
`third_party/morf/beacon`.

```sh
cmake --preset mingw        # ou linux / linux-arm64
cmake --build --preset mingw
```

## Lancer

```sh
./build-mingw/service/morfcollector.exe
curl http://127.0.0.1:8792/status
```

## Réglages de sécurité

- **Liste blanche du connecteur `fs`** : une source `fs` n'est acceptée que sous une racine listée
  dans `fs_allowed_roots` du fichier de configuration admin (`/etc/morfsystem/morfcollector/`, en
  lecture seule, jamais modifiable par l'API). Sans cette liste, toute source `fs` est refusée. Les
  liens symboliques et les dossiers du coffre et du magasin sont toujours refusés.
  ```json
  "fs_allowed_roots": ["/mnt/nas/logs"]
  ```
- **Clé d'hôte SFTP** : renseigner `host_key_sha256` dans les paramètres de la source (hex, ou
  `SHA256:<base64>` comme l'affiche `ssh-keygen -lf`) pour vérifier la clé avant l'envoi de tout
  secret. Sans lui, la clé n'est pas vérifiée et l'empreinte vue est écrite dans le journal.
- **Écritures depuis un navigateur** : une écriture (POST/DELETE) dont l'en-tête `Origin` diffère de
  `Host` reçoit `403 cross_origin_forbidden` ; les clients hors navigateur ne sont pas concernés.

## Installer en service

```sh
# Toutes plateformes : Linux, Windows, Raspberry Pi
sudo ./service.py install      # compile si besoin, installe, demarre
sudo ./service.py update       # recompile, remplace le binaire, redemarre
sudo ./service.py uninstall    # desinscrit, en conservant votre configuration
./service.py status            # ce que le systeme en dit
```

Un seul point d'entree partout. Ce qu'est ce service -- son nom, son dossier,
ses configurations -- est declare dans `service.json` a cote ; les quatre etapes
d'installation vivent une seule fois pour tout le parc, et seul le gestionnaire
de services change selon la plateforme (systemd, services Windows, launchd).


## Documentation

- [Architecture](docs/fr/ARCHITECTURE.md) - les classes et le fil d'exécution.
- [Contrat `morfcollect/1`](docs/fr/CONTRAT.md) et [guide d'intégration fournisseur](docs/fr/INTEGRATION.md).

## Intégration continue

Des workflows GitHub Actions vivent dans `.github/workflows/` (générés par le script morfTools
`gen-ci-workflows.py`) :

- **`ci.yml`** : à chaque pull request (et push sur `main`/`dev`), compile et lance les tests sous
  Ubuntu 24.04 (Qt 6.4), Linux arm64 et Windows (MSYS2 UCRT64). Les changements de documentation seule l'ignorent et un run périmé est annulé
  par un commit plus récent.
- **`packages.yml`** : à chaque push sur `main` ou en lancement manuel (*Actions > Packages >
  Run workflow*), construit le `.deb` amd64, le `.deb` arm64 et les paquets Windows. Les paquets sont des artefacts du workflow conservés
  14 jours ; les releases sont publiées par l'outillage morfSystem, qui prend les paquets du run
  du commit publié et ne compile en local que ce qui manque.

## Licence

GPL-3.0-only - © 2026 morfredus (Frédéric Biron).
