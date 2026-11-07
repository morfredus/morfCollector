# Contribuer à morfCollector

morfCollector est le service de collecte et de conservation locale du parc
morfSystem. Son comportement est fixé par le contrat `morfcollect/1`
(`docs/fr/CONTRAT.md`) : toute évolution passe par des ajouts optionnels, jamais
par un changement incompatible.

## 1. Philosophie

- **Exécuter, jamais décider.** Le manifeste dit quoi collecter ; le service
  conserve les originaux sans les interpréter.
- **Qt Core + Network uniquement.** morfBeacon reste vendoré. Portable Windows /
  Linux x64 / Raspberry Pi.
- **Rien n'est détruit par effet de bord.** Seule une action explicite supprime
  des octets.

## 2. Vérifier une modification

```sh
cmake --preset mingw && cmake --build --preset mingw
ctest --test-dir build-mingw
```

## 3. Style

- C++17, conventions des projets frères : en-tête de licence SPDX, namespace
  `morfcollector`, commentaires en français expliquant le **pourquoi**.
- Fins de ligne : voir `.gitattributes` (LF dans le dépôt ; `.ps1` en CRLF).
