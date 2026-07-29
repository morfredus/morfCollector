# Roadmap - morfCollector

morfCollector est le service de **collecte et de conservation locale** du parc.
Ce n'est pas un dépôt template : il implémente le contrat **`morfcollect/1`**
(voir [`docs/fr/CONTRAT.md`](docs/fr/CONTRAT.md)), gelé avant l'implémentation
comme `morfbeacon/1`.

Deux vérités le cadrent, et rien de ce qui suit ne doit les enfreindre :

- **morfCollector est un exécutant.** Il ne décide **jamais** quoi collecter. Un
  *fournisseur de configuration* (SiteWatch aujourd'hui, un autre demain) détient
  la politique et lui envoie un manifeste de sources ; morfCollector se contente
  de l'exécuter et de garantir la conservation. Il ne découvre rien, ne scanne
  rien, ne cherche pas de serveurs.
- **Il conserve les originaux sans jamais les interpréter.** Les objets collectés
  sont gardés tels quels ; le sens leur appartient, pas au collecteur.

## Déjà en place (socle du contrat, v0.4.x)

Le cœur du contrat est implémenté et livré :

- **Modèle de source à deux axes d'état** (contrat §3) : administratif
  (`active` / `suspended` / `retired`) et opérationnel (`idle` / `waiting` /
  `collecting` / `error` / `auth_failed` / `unreachable`).
- **Moteur `Collector`** : plan de contrôle (`SourceRegistry`, manifeste, états)
  et plan de données (connecteurs, pipeline de collecte asynchrone, `ObjectStore`
  avec index, rétention), plus un **ordonnanceur autonome** qui déclenche les
  collectes planifiées même fournisseur fermé (contrat §4.5).
- **Connecteurs** `fs` et `sftp` (interface `IConnector`).
- **Coffre de secrets chiffré** (`Vault`, AES-256-GCM/OpenSSL, clé propre à
  l'installation, jamais relue), remise des identifiants par `POST /credentials`
  (contrat §1.5).
- **Ingestion de manifeste** (§4), **API d'administration** (§6.3), **capacités
  annoncées** (§7.2), `/status` compatible morfBeacon.
- **Doctrine fichiers** : état sous `/var/lib/morfsystem/morfcollector` (`data/`
  et `vault/` séparés pour qu'une sauvegarde des données n'emporte jamais la clé),
  config admin sous `/etc/morfsystem/morfcollector`, programme sous `/opt`.

## Intégration fournisseur : faite

Le premier fournisseur, **SiteWatch**, est branché de bout en bout et vérifié :
`source_id` = UUID persisté par SiteWatch, composant `CollectorClient` /
`CollectorSync` (découverte par capacité `collection`, `pushManifest`,
`pushCredentials`, `getSources`, `download`), outil console
`sitewatch-collector-sync` et intégration GUI (bouton « Tout synchroniser »,
onglet « Copies locales » avec administration). Génération + révision du manifeste
persistées, réconciliation par `source_id`, retrait ≠ suppression. La boucle
fournisseur → collecteur → conservation tourne.

## Ce qui reste

- **Validation en conditions réelles** (le vrai reste) : SFTP contre un hôte et des
  identifiants réels (le connecteur n'est pas testable hors ligne) ; la GUI
  SiteWatch complète en interactif ; la découverte morfBeacon **inter-hôtes**
  (collecteur sur le Pi, SiteWatch sur le PC) validée en broadcast réel.
- **Nouveaux connecteurs, à la demande seulement.** Le contrat est agnostique au
  transport ; `fs` et `sftp` existent. HTTP(S), IMAP/courriel, S3… ne s'ajoutent
  que lorsqu'un fournisseur en a réellement besoin - jamais « au cas où », pour
  ne pas trahir le rôle d'exécutant.
- **Tests de contrat + CI** : la machine à états d'une source, le pipeline
  collecte → commit dans l'`ObjectStore`, la rétention, le coffre (indisponible
  → `503`, jamais de fuite de secret) ; build multi-plateforme.
- **Index SQLite** (optionnel, futur) : si le volume d'objets rend l'index JSON
  actuel lent.
- **Réalignement du README** : sa présentation décrit encore un « squelette sans
  code métier » (héritage du template dont morfCollector est issu), ce qui ne
  correspond plus à la réalité du service.

## Non-objectifs (garde-fous du contrat)

- **Ne jamais décider quoi collecter** : pas de découverte, pas de scan, pas de
  « qu'est-ce que je pourrais sauvegarder ? ». La politique appartient au
  fournisseur.
- **Ne jamais interpréter le contenu** collecté : conserver les originaux, pas en
  extraire du sens (c'est le rôle d'autres services, comme morfAnalytics sur ses
  propres données).
- **Pas de dépendance externe au-delà de Qt et d'OpenSSL** (morfBeacon reste
  vendoré).
