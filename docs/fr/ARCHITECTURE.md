# Architecture - morfCollector

Retour à l'[index de la documentation](README.md).

Voir d'abord le [contrat `morfcollect/1`](CONTRAT.md) : l'architecture n'est qu'une
implémentation de ce contrat.

---

Service Qt (Core + Network), sans interface. morfCollector n'a pas de « modules »
enfichables : il a **un moteur** unique qui pilote des sources décrites par un
manifeste. On distingue deux plans :

- le **plan de contrôle** (implémenté) : manifeste, états, synchronisation, API ;
- le **plan de données** (à venir) : connecteurs concrets, récupération des
  objets, index sur disque, coffre chiffré, rétention.

## Les pièces

```
Service (facade : cable tout a partir d'une ServiceConfig)
├── Collector            -> LE moteur ; implemente morfbeacon::IMetricsProvider
│     └── SourceRegistry -> reconciliation declarative (manifeste -> sources)
│            └── Source   -> etat runtime d'une source (triplet de suspension)
├── HttpServer           -> API du contrat (GET/POST/DELETE)
└── morfbeacon::Heartbeat -> annonce LAN, capacite "collection"
        ▲ IMetricsProvider
        └── Collector expose metrics() + state()
```

### `ServiceConfig`

Réglages **de la machine** uniquement (port, bind, annonce, racine de stockage).
Elle **ne déclare pas les sources** : celles-ci n'arrivent que par le manifeste
(« une seule configuration utilisateur », voir CONTRAT.md).

### `Manifest` (+ `Manifest::parse`)

Miroir passif du JSON reçu (`ProviderRef`, `ManifestSource`, `ConnectorRef`).
`parse()` valide la **forme** (proto, champs requis) et renvoie un 422 sinon.
Aucune logique de réconciliation ici.

### `SourceRegistry` - la réconciliation

Logique **pure** (aucune E/S), donc testable sans réseau. Détient les sources
indexées par `source_id`, suit la `(génération, révision)` appliquée par
fournisseur, et décide d'appliquer / d'ignorer (409 périmé) / de rejeter (409
propriété). La réconciliation est déclarative : création / mise à jour / retrait
par `source_id` (CONTRAT.md §4.2).

### `Source` - le double axe d'état

Combine la déclaration du manifeste et le **triplet de suspension** conservé
séparément (`manifest_enabled`, `operator_override`, état effectif dérivé). Porte
l'état administratif (ACTIVE/SUSPENDED/RETIRED) et l'état opérationnel
(IDLE/WAITING/...). L'override opérateur est prioritaire et persiste (§4.3.1).

### `Collector` - le moteur

Possède le `SourceRegistry`, implémente `morfbeacon::IMetricsProvider`
(`metrics()`/`state()` pour `/status` et le heartbeat), et expose les opérations
appelées par l'API : appliquer un manifeste, lire les sources, suspendre /
reprendre, collecter, remettre des secrets. Les capacités annoncées viennent des
connecteurs enregistrés (aucun pour l'instant : liste honnêtement vide).

### `HttpServer`

Serveur HTTP/1.1 minimal (GET/POST/DELETE, corps via `Content-Length`), routant
les endpoints du contrat (CONTRAT.md §6) vers le `Collector`. `/status` ajoute
les blocs `collect` et `capabilities` et le bloc `api` (point unique
`SelfDescription`).

### `Service` (façade)

L'unique objet manipulé par le démon : construit le `Collector`, démarre le
serveur HTTP puis le heartbeat morfBeacon (capacité `collection`).

## Fil d'exécution

Tout tourne sur **le thread principal Qt**. Le plan de contrôle est synchrone et
non bloquant. Le plan de données (collectes réseau) devra rester **asynchrone** et
ne publier qu'un instantané via l'état des sources, sans jamais bloquer le serveur.

## Plan de données

Le connecteur est une interface (`IConnector`) nommée et versionnée (CONTRAT.md
§2) : `validate` / `list` / `fetch`, plus une capacité annoncée.

**Collecte asynchrone.** `Collector::startCollection` prend un instantané sur le
thread principal (params, secrets, noms déjà collectés), puis exécute le travail
réseau (`list` → `fetch`) sur un **thread de travail** (`QtConcurrent`). Le
**commit** (écriture dans l'`ObjectStore`, mise à jour de l'état) se fait au retour
sur le thread principal. Une garde par source empêche deux collectes simultanées.
Un ordonnanceur (`QTimer`) déclenche les collectes planifiées même fournisseur
fermé (autonomie, §4.5). Cette répartition est **vérifiée via le connecteur `fs`**
(le réseau, lui, n'est pas testable en local).

- `CollectedObject` + `ObjectStore` : conservation **à l'identique** sur disque,
  index JSON persisté, déduplication par nom, périodes, totaux, suppressions
  explicites, rétention (`keep_forever` / `keep_days` / `keep_size`).
- `Vault` : coffre **chiffré** des secrets au repos (AES-256-GCM, OpenSSL), clé
  par installation ; les secrets survivent au redémarrage, ne sont jamais relus
  par l'API ni annoncés (§1.5, §7.3).
- Connecteur **`fs`** (dossier local, testable hors ligne) et connecteur **`sftp`**
  (SSH/SFTP via libssh2, dépendance optionnelle, adapté de SiteWatch ; **non
  testable en local**).

## Dépendance morfBeacon (embarquée)

morfBeacon est vendoré dans `third_party/morf/beacon` (lié statiquement) : build
autonome. Resynchroniser avec `scripts/sync-morf.(sh|ps1)`.

## Portabilité

Aucun code spécifique à une plateforme. Comportement identique Windows / Linux
x64 / Raspberry Pi (ARM64). Installation en service via `service.py` (systemd /
Planificateur de tâches Windows).
