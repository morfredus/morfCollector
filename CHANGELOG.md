# Journal des versions - morfCollector

Le format s'inspire de [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/)
et du [versionnage sémantique](https://semver.org/lang/fr/).

## [0.8.0] - 2026-08-20

### Ajouté

- **Connecteur `github-traffic/1`.** Collecte quotidienne d'un instantané brut
  GitHub (vues, clones, pages, référents, releases et `download_count`) sans
  interprétation. Objet partiel conservé si un endpoint échoue. Le PAT reste
  dans le coffre. Sous-contrat : `docs/fr/CONNECTEUR-GITHUB-TRAFFIC.md`.
- Le heartbeat annonce aussi les capacités des connecteurs réellement
  compilés (`sftp`, `github-traffic`, …) en plus de `collection`.

### Corrigé

- Compilation MinGW du connecteur GitHub : boucle sur les endpoints sans
  `initializer_list` mixant pointeurs const et non-const.

## [0.7.0] - 2026-08-20

### Corrigé

- Le packaging Windows détecte la racine OpenSSL de la toolchain MinGW active,
  sans chemin d'installation figé.

## [0.6.0] - 2026-08-20

### Ajouté

- Mise à jour de la copie vendorée de morfDeploy 0.14.0 : le service peut
  désormais produire des paquets dont la provenance est vérifiée.

## [0.5.1] - 2026-08-19

### Corrigé

- **Lecture des objets indépendante du manifeste courant** (contrat §6.2).
  `GET /sources/{id}/objects` et `/periods` répondaient `404` dès que la source
  n'était pas chargée dans le manifeste - or les sources ne sont pas persistées :
  après un redémarrage, les objets pourtant archivés devenaient invisibles tant
  que le fournisseur n'avait pas repoussé sa configuration. L'**index** (objets
  conservés) est désormais l'autorité d'existence pour la lecture, pas le
  manifeste (sources à collecter) : source dans l'index → `200` + objets ; source
  du manifeste sans objet → `200` + liste vide ; source totalement inconnue →
  `404`. Ce n'était pas une frontière d'autorisation (confiance LAN, §7.3) mais
  une dépendance d'implémentation ; `deleteSourceObjects` appliquait déjà cette
  autorité de l'index. Permet un diagnostic de cohérence en lecture seule côté
  fournisseur sans avoir à pousser un manifeste au préalable. Additif et
  rétrocompatible. `GET /sources/{id}` (détail de la source, qui expose les
  `params` du manifeste) reste, lui, en `404` hors manifeste.

## [0.5.0] - 2026-08-19

### Modifié

- **Collecte incrémentale par la taille** (contrat §5.6). Un fichier surveillé
  n'est plus figé à sa première capture : morfCollector compare la taille distante
  (obtenue au listage, sans transfert) à celle du dernier état conservé, et
  re-télécharge dès qu'elle diffère. Indispensable pour les logs o2switch, qui
  sont un `.gz` **par mois** se complétant au fil de l'eau : avant, un mois
  collecté tôt restait tronqué jusqu'à sa clôture.
- **Upsert par (source, nom d'origine)** dans l'`ObjectStore` : recollecter un nom
  déjà connu **remplace** l'objet (même `object_id`, dernier état) au lieu d'en
  créer un second. Sans ça, la collecte quotidienne d'un fichier mutable empilait
  une copie par nuit (duplication). La référence reste stable pour un client qui a
  déjà lu l'objet.

### Détails

- Détection : taille supérieure = nouvelles données ; inférieure = fichier
  tronqué, réinitialisé ou remplacé (rotation) ; égale = inchangé ; taille
  distante inconnue = récupération. Un fichier disparu du listage n'est pas
  supprimé (son état conservé reste disponible). Le `hash` reste une métadonnée
  d'intégrité, jamais un critère de collecte (il imposerait de retransférer le
  fichier). Contrat : fichiers surveillés **append-only**.

## [0.4.5] - 2026-08-18

### Ajouté

- **Déclaration de dépendances de build** dans `service.json` :
  `build_dependencies` = `openssl` (coffre chiffré) et `libssh2` (SFTP), toutes
  deux requises. morfDeploy 0.9.0 les résout **avant** le build (Debian :
  `libssl-dev`, `libssh2-1-dev` ; sur une toolchain sans gestionnaire, annonce le
  besoin). Sur l'Asus (Qt officielle, sans MSYS2), le build de morfCollector
  échouait sur `find_package(OpenSSL)` : le besoin est désormais annoncé
  clairement avant l'erreur CMake. Aucun changement de code du service.

## [0.4.4] - 2026-08-17

### Ajouté

- **Déclaration de purge** dans `service.json` : catégories `vault` (le coffre
  chiffré : clé + secrets) et `data` (les collectes conservées), toutes deux
  destructrices. Chacune utilise `from_config` (`vault_root` / `storage_root`)
  pour que morfdeploy lise le vrai emplacement dans la config déployée quand
  l'admin l'a relocalisé, avec repli sur `state/vault` et `state/data`. Effaçables
  par `service.py purge vault|data` et `morf purge morfCollector …` (avec
  `--dry-run`, confirmation à jeton, et refus si le service tourne). Aucun
  changement de code du service : capacité déclarée, exécutée par le socle.

## [0.4.3] - 2026-08-14

### Corrigé

- **Troncature des grandes réponses HTTP** dans `HttpServer`. Resynchronisation du
  correctif issu du patron `morfTemplateService`, appliqué aux deux chemins d'écriture :
  la réponse générique `reply()` et surtout le **téléchargement d'objets** (`GET
  /objects/<id>`), dont le corps binaire peut peser bien plus que le tampon socket
  (~20 Ko). La connexion était fermée sans drainer le tampon d'écriture, ce qui
  tronquait les gros objets. On attend désormais que `bytesToWrite()` retombe à zéro
  avant `disconnectFromHost()`.
- Resynchronisation de la copie vendorée de **morfBeacon** (`third_party/morf/beacon`)
  en 0.6.1 : même classe de bug corrigée dans son `StatusServer` (grande réponse
  `/status` coupée faute de drainage du tampon d'écriture).

## [0.4.2] - 2026-08-14

### Corrigé

- Description de l'unité systemd : remplacement du tiret cadratin par un tiret
  simple, conformément à la règle de ponctuation du parc.

## [0.4.1] - 2026-08-14

### Modifié

- Resynchronisation de la copie vendorée de **morfBeacon**
  (`third_party/morf/beacon`) en 0.6.0, alignée sur le dépôt source
  (`IMetricsProvider.h`, `StatusServer.cpp`). Aucun changement de comportement.
- Ajout du marqueur de version manquant à la copie vendorée de **morfdeploy**
  (`third_party/morf/morfdeploy/VERSION` = 0.1.0) ; le code Python était déjà à
  jour. `morf doctor` de nouveau vert sur les copies vendorées.

## [0.4.0] - 2026-07-28

### Modifié

- **État persistant déplacé sous `/var/lib/morfsystem/morfcollector`** (doctrine
  du parc, voir `docs/fr/FILESYSTEM.md`). Les données collectées (`data/`) et le
  coffre chiffré (`vault/`) ne vivent plus sous `/opt/<service>/data` ni sous
  `/etc/morfsystem/<service>` : ils sont désormais de l'**état**, distinct de la
  config admin (`/etc`, lecture seule) et du programme (`/opt`). L'unité systemd
  déclare `StateDirectory=morfsystem/morfcollector` : systemd crée le dossier
  possédé par l'utilisateur du service et l'expose via `$STATE_DIRECTORY`. Le
  coffre et les données restent des sous-dossiers séparés (`vault/` et `data/`)
  pour qu'une sauvegarde de `data/` n'emporte jamais la clé.

### Corrigé

- **Coffre inécrivable non signalé.** Quand le dossier du coffre n'était pas
  accessible en écriture (cas d'un `/etc/morfsystem/<service>` appartenant à
  `root` alors que le service tourne sous son propre utilisateur), `POST
  /credentials` répondait un `400` trompeur (« 'ref' et 'secret' requis ») alors
  que le vrai problème était l'échec silencieux de l'initialisation du coffre.
  Désormais : `POST /credentials` répond **`503`** avec un message clair quand le
  coffre est indisponible, distinct du `400` d'une requête mal formée, et le
  service **journalise un avertissement au démarrage** si le coffre n'est pas
  opérationnel. La racine du problème est traitée par le déplacement de l'état
  ci-dessus.

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
