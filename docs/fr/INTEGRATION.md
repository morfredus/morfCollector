# Intégrer un fournisseur à morfCollector

Retour à l'[index de la documentation](README.md).

Prérequis : le [contrat `morfcollect/1`](CONTRAT.md). Ce guide montre le **côté
fournisseur** : ce qu'une application (SiteWatch, ou une autre) fait pour confier
ses collectes à morfCollector. Une implémentation de référence existe dans
SiteWatch : `src/collector/CollectorClient.*` et l'outil `sitewatch-collector-sync`.

---

## Vue d'ensemble

```
SiteWatch (fournisseur)                         morfCollector
     │                                                │
     │  1. decouverte morfBeacon (capacite 'collection')
     │───────────────────────────────────────────────►│
     │  2. GET /status  (compatibilite collect.proto) │
     │  3. GET /manifest/state?instance=…  (revision)  │
     │  4. POST /manifest  (sources a collecter)       │
     │  5. POST /credentials  (secrets, une fois)      │
     │  6. GET /sources … /objects  (copies locales)   │
     └───────────────────────────────────────────────►│
```

L'application reste **autonome** : si aucune étape n'aboutit (pas de collecteur),
elle fonctionne comme avant. morfCollector est une amélioration, jamais une
dépendance.

## 1. Découvrir morfCollector

Écouter le heartbeat morfBeacon (UDP `45454`) et retenir le premier service qui
annonce la **capacité `collection`** — jamais le nom `morfCollector`, qui est
modifiable. L'adresse source du datagramme + `status_port` donnent l'URL de base.

```cpp
CollectorClient::Discovered d;
if (CollectorClient::discover(4000, d, err))
    baseUrl = d.baseUrl();     // ex. http://192.168.1.14:8792
```

## 2. Vérifier la compatibilité

`GET /status` expose un bloc `collect` : vérifier que `collect.proto` contient la
version que le fournisseur sait produire (`morfcollect/1`). Si absente, ne rien
pousser.

## 3. Comparer les révisions

Chaque fournisseur possède une **lignée** (`manifest_generation`, un UUID) et une
**révision** monotone à l'intérieur de cette lignée (voir CONTRAT.md §1.4). Les
conserver localement (ici un fichier `config.json.collector.json`).

`GET /manifest/state?instance=<app@host>` renvoie la génération et la révision
**appliquées** par le collecteur. Comparer :

- configuration inchangée **et** collecteur à jour → **ne rien faire** ;
- configuration modifiée → **révision + 1**, puis pousser ;
- collecteur en retard (redémarré, il a oublié les sources — elles ne sont pas
  persistées côté collecteur) → repousser la révision courante.

Frapper une **nouvelle génération** uniquement en cas de restauration, de
réinstallation ou de retour à une ancienne configuration.

## 4. Construire et pousser le manifeste

Chaque source porte un **`source_id` stable** (UUID **persisté par le
fournisseur**, jamais dérivé du nom : un site peut changer d'hôte, de domaine, de
chemin, il reste la même source). `POST /manifest` :

```jsonc
{
  "proto": "morfcollect/1",
  "provider": { "app": "SiteWatch", "instance": "SiteWatch@fredpc" },
  "manifest_generation": "56d584cd-…",
  "revision": 2,
  "sources": [
    {
      "source_id": "6a0ad95e-…",
      "label": "monsite.fr",
      "connector": { "name": "sftp", "version": 1 },
      "params": { "host": "…", "remote_dir": "…", "match": "", "site_key": "monsite.fr" },
      "credentials_ref": "sw-6a0ad95e-…",
      "schedule": { "daily_at": "02:00" },
      "retention": { "mode": "keep_forever" }
    }
  ]
}
```

Réponses : `200` appliqué, `409` révision périmée ou source d'un autre
fournisseur, `422` manifeste non applicable. Retirer une source, c'est
simplement l'**omettre** d'un manifeste ultérieur (la collecte s'arrête, les
archives restent).

## 5. Remettre les secrets (une fois)

Les coordonnées d'accès ne voyagent pas dans le manifeste : le manifeste ne porte
qu'une `credentials_ref`. `POST /credentials` remet le secret **une fois** ; le
collecteur le range dans son coffre chiffré et ne le renvoie plus jamais.

```jsonc
{ "ref": "sw-6a0ad95e-…", "secret": { "user": "fred", "password": "…" } }
```

Pour SFTP, le secret peut porter `user` + `password`, ou une clé
(`key_file` local au collecteur, ou `private_key` en mémoire) + `passphrase`.
Réponse `204`.

## 6. Lire les copies locales

- `GET /sources` : état de chaque source (administratif + opérationnel).
- `GET /sources/{id}` : détail, dont le triplet `manifest_enabled` /
  `operator_override` / `effective_state`.
- `GET /sources/{id}/periods` et `/objects` : ce qui est conservé.
- `GET /objects/{object_id}` : récupérer l'original (c'est ici que l'application
  lit la copie locale au lieu de l'hébergeur).

Administration (menu « Copies locales ») : `POST /sources/{id}/collect|suspend|resume`,
`DELETE /objects/{id}`, `DELETE /sources/{id}/objects`, `DELETE /sources/{id}`
(refusée tant que la source n'est pas retirée).

## Vérifier l'intégration

Depuis SiteWatch, sans la GUI :

```sh
sitewatch-collector-sync config.json                 # decouverte morfBeacon
sitewatch-collector-sync config.json --url http://127.0.0.1:8792
sitewatch-collector-sync config.json --url … --collect
```

L'outil charge la configuration (attribue et **persiste** les UUID manquants),
découvre ou joint le collecteur, pousse le manifeste et les secrets, puis affiche
les sources. Idempotent : relancé sans changement, il n'envoie rien.
