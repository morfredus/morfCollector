# Documentation de morfCollector (français)

Squelette réutilisable des services morfSystem : API HTTP, config JSON, annonce
LAN (morfBeacon), service systemd et Windows. On clone, on code le métier.

> 🇬🇧 English documentation: [`docs/en/`](../en/README.md) *(index, in progress)*.
> Retour au [README (français)](../../README.fr.md).

## Comprendre et utiliser

| Document | Contenu |
|---|---|
| [Contrat `morfcollect/1`](CONTRAT.md) | **Le protocole** fournisseur ↔ morfCollector : manifeste, cycle de vie d'une source, synchronisation, stockage, API HTTP, annonces morfBeacon, principes. À geler avant toute implémentation métier. |
| [Connecteur `github-traffic/1`](CONNECTEUR-GITHUB-TRAFFIC.md) | Sous-contrat du connecteur GitHub Traffic : params, coffre, objet brut daté, fenêtre de 14 jours. |
| [Architecture](ARCHITECTURE.md) | Les classes (`IModule`, `ModuleRegistry`, `HttpServer`, `Service`) et le fil d'exécution. |
| [Intégrer un fournisseur](INTEGRATION.md) | Côté fournisseur (SiteWatch, …) : découvrir, pousser un manifeste et des secrets, lire les copies. |

## À la racine du projet

| Document | Contenu |
|---|---|
| [README](../../README.md) | Présentation générale (anglais). |
| [README (français)](../../README.fr.md) | Présentation générale (français). |
| [Journal des versions](../../CHANGELOG.md) | Historique des versions. |
| [Roadmap](../../ROADMAP.md) | Évolutions envisagées du template. |
| [Contribuer](../../CONTRIBUTING.md) | Guide de contribution au template. |
