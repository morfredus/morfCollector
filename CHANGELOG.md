# Journal des versions - morfCollector

Le format s'inspire de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/)
et du [versionnage sémantique](https://semver.org/lang/fr/).

## [0.3.0] - 2026-07-28

### Modifié

- **Configuration regroupée sous `/etc/morfsystem/<service>`.** Tout le parc
  partage désormais un point d'entrée UNIQUE dans `/etc` (`/etc/morfsystem/`),
  qui contient le fichier partagé `morfsystem.json` et un sous-dossier par
  service, au lieu d'un `/etc/<service>` par service à la racine de `/etc`. Sous
  Windows : `%ProgramData%\morfsystem\<service>`. Les données restent sous
  `/opt/<service>`. L'ancien `/etc/<service>` est adopté à l'installation
  (`migrate_from`).
- Le **coffre de secrets** (`vault.enc` + `vault.key`) suit la config :
  `/etc/morfsystem/morfcollector`, toujours séparé des données (`storage_root`).
- **Rangement des données conforme à la convention morfSystem** (`docs/FILESYSTEM.md`).
  Les objets collectés et l'index vivent désormais sous `<app_dir>/data`
  (`/opt/morfcollector/data` sous Linux) au lieu d'un dossier applicatif à plat.
  Le **coffre de secrets** (`vault.enc` + `vault.key`) est déplacé dans le dossier
  de configuration (`/etc/morfsystem/morfcollector`), **séparé des données** : copier ou
  sauvegarder `data/` n'emporte plus jamais la clé, et le chiffrement au repos
  garde son sens. Deux réglages facultatifs : `storage_root` (données) et
  `vault_root` (coffre). La disposition disque restait un détail d'implémentation
  (hors contrat) : `morfcollect/1` est inchangé, les consommateurs ne voient
  aucune différence (accès par l'API HTTP).

### Ajouté

- **Rendez-vous de collecte quotidien (`schedule.daily_at`).** Le manifeste peut
  demander une collecte **une fois par jour à une heure locale** (`"HH:MM"`, ex.
  `"02:00"`) en plus de l'intervalle `every_minutes` existant. L'ordonnanceur
  déclenche la collecte à l'heure dite ou, si la machine était éteinte, au
  démarrage suivant (rattrapage, une seule fois par jour). Ajout additif : `proto`
  reste `morfcollect/1`.

- **Contrat `morfcollect/1` gelé** (`docs/fr/CONTRAT.md`) : le protocole fonctionnel
  fournisseur ↔ morfCollector, à respecter avant toute implémentation métier.
  Fige le manifeste (`manifest_generation` + `revision`, `source_id` UUID stable),
  le contrat de connecteur (nom/version/paramètres/validation/capacité), le double
  axe d'état d'une source (administratif + opérationnel), la réconciliation
  déclarative, le coffre de secrets à remise unique, l'API HTTP, les annonces
  morfBeacon (capacité `collection`), et le principe fondateur « exécutant, jamais
  décideur ». Capacité `collection` ajoutée au registre morfBeacon (additif).
- **Plan de contrôle du contrat implémenté.** L'ancien système de « modules » du
  template est remplacé par le moteur `Collector` + `SourceRegistry` + `Source` :
  parsing/validation du manifeste, réconciliation déclarative par `source_id`,
  logique génération/révision (rejeu périmé, nouvelle lignée), isolation par
  fournisseur, triplet de suspension avec précédence de l'override opérateur,
  états administratif et opérationnel. API HTTP réelle (`POST /manifest`,
  `GET /manifest/state`, `POST /credentials`, `GET /sources[/{id}[/periods|objects]]`,
  `POST /sources/{id}/{collect|suspend|resume}`), et `/status` enrichi de `collect`
  + `capabilities`. Le plan de données (connecteurs, index disque, coffre chiffré,
  rétention) reste à venir, derrière interfaces.
- **Plan de données : fondations et premier connecteur.** Interface `IConnector`
  (contrat de connecteur : nom/version/capacité/validation/list/fetch), modèle
  `CollectedObject` et `ObjectStore` (conservation à l'identique sur disque +
  index JSON persisté, déduplication par nom, totaux). Premier connecteur concret
  **`fs`** (collecte depuis un dossier local, testable hors ligne), pipeline de
  collecte complet (`list` → `fetch` → dédup → `put`), extraction de période
  depuis le nom, états opérationnels réels (waiting/error/auth_failed/unreachable),
  politiques de rétention (`keep_forever`/`keep_days`/`keep_size`), suppressions
  explicites (`DELETE /objects/{id}`, `/sources/{id}/objects`, `/sources/{id}`
  refusée si non retirée), téléchargement binaire de l'original
  (`GET /objects/{id}`), et un ordonnanceur déclenchant les collectes planifiées
  même fournisseur fermé. `capabilities` reflète les connecteurs réellement
  disponibles.
- **Collecte asynchrone + connecteur `sftp`.** La collecte s'exécute désormais sur
  un thread de travail (`QtConcurrent`) : le connecteur (bloquant pour le réseau)
  ne bloque jamais le serveur HTTP, et le commit dans l'`ObjectStore` se fait sur
  le thread principal à la fin du travail. Garde anti-collecte-concurrente par
  source. `POST /collect` répond `202` immédiatement ; l'état suit via
  `GET /sources/{id}`. Connecteur **`sftp`** (SSH/SFTP via libssh2, dépendance
  optionnelle) adapté du code éprouvé de SiteWatch : authentification clé (fichier
  local ou matière en mémoire) ou mot de passe, listing `.gz`, correspondance
  o2switch (préfixe) ou générique (sous-chaîne), récupération en mémoire. **Non
  testable en local** (réseau + hébergeur) ; le harnais asynchrone, lui, est
  vérifié via le connecteur `fs`.
- **Coffre chiffré des secrets au repos.** Les secrets remis par `POST /credentials`
  sont désormais chiffrés (AES-256-GCM via OpenSSL) et persistés (`vault.enc`),
  avec une clé propre à l'installation générée au premier démarrage
  (`vault.key`, permissions restreintes au propriétaire). Ils survivent au
  redémarrage (autonomie), ne sont jamais journalisés ni renvoyés par aucune
  route. Modèle de menace assumé : la clé vit sur la même machine que le coffre
  (service headless, sans phrase de passe) ; le chiffrement protège une copie du
  fichier de coffre, pas un attaquant ayant déjà tous les droits sur la machine.

## [0.1.0] - 2026-07-27

### Ajouté

- **Initialisation du projet morfCollector.** Service de collecte et de
  conservation locale de ressources distantes temporaires pour l'écosystème
  morfSystem. Amorcé depuis morfTemplateService : squelette Qt/C++ (API HTTP,
  configuration JSON, annonce morfBeacon, installation en service multiplateforme
  via `service.py`), sans code métier propre. Port de service **8792** réservé
  dans `morfTools/ecosystem.json`.
