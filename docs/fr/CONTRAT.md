# Contrat `morfcollect/1` - fournisseur de configuration ↔ morfCollector

Retour à l'[index de la documentation](README.md).

---

Ce document spécifie le **contrat fonctionnel** entre un *fournisseur de
configuration* (SiteWatch aujourd'hui, une autre application demain) et
morfCollector. Il est rédigé **avant** toute implémentation : comme pour
`morfbeacon/1`, la solidité du service vient de ce qu'on gèle le protocole
d'abord et qu'on code un comportement seulement ensuite.

Le contrat ne décrit **aucune classe C++**. Il décrit des **messages**, des
**états** et des **règles**. Les concepts métier (`Source`, `Connector`,
`CollectionTask`, `CollectedObject`, `RetentionPolicy`) implémenteront ce
contrat ; ils n'y figurent que comme vocabulaire.

## Quatre vérités fondatrices

1. **morfCollector est un exécutant.** Il ne décide **jamais** quoi collecter.
   Une application cliente reste seule responsable de définir les ressources à
   conserver. morfCollector ne fait qu'exécuter cette politique et garantir la
   conservation locale des données obtenues. Il ne découvre pas Internet, ne
   cherche pas de serveurs, ne scanne pas un NAS, ne se demande jamais « qu'est-ce
   que je pourrais sauvegarder ? ». Voir le principe en [§8](#8-les-principes-darchitecture)
   ; c'est le garde-fou le plus important du contrat.
2. **Le fournisseur est l'unique propriétaire de la configuration.** morfCollector
   n'a pas d'interface de déclaration des sources. Il détient une copie
   *opérationnelle* dérivée d'un manifeste reçu.
3. **morfCollector conserve les originaux, sans jamais les interpréter.** La
   collecte et l'analyse sont strictement séparées.
4. **Une donnée n'est jamais détruite par un effet de bord.** Retirer une source
   arrête la collecte ; seule une action explicite supprime des octets.

> ### État de gel
> **`morfcollect/1` est gelé** (27/07/2026). Tous les arbitrages sont tranchés :
> l'implémentation peut désormais remplacer les `ExampleModule` par `Source`,
> `Connector`, `CollectionTask`, `CollectedObject` et `RetentionPolicy`, en
> respectant ce contrat. Toute évolution ultérieure se fait **par ajout de clés
> optionnelles** (voir §1.6) ; un changement incompatible incrémenterait `proto`.

---

## 1. Le manifeste de collecte

### 1.1 Rôle

Le manifeste est le **seul** message par lequel un fournisseur décrit à
morfCollector *ce qu'il faut collecter*. Ce n'est pas une copie de la
configuration interne du fournisseur : il ne contient que le nécessaire à la
collecte. Le protocole est générique - toute application peut produire le même
format.

### 1.2 Format

Document JSON unique, poussé par le fournisseur (voir [§6](#6-lapi-http)).

```jsonc
{
  "proto": "morfcollect/1",

  "provider": {
    "app": "SiteWatch",
    "instance": "SiteWatch@fredpc"
  },

  "manifest_generation": "9d2e7f10-1c3b-4a55-8e02-77aa1b6c4d90",
  "revision": 7,

  "sources": [
    {
      "source_id": "b3f1c2a4-5e6d-4f80-9a1b-2c3d4e5f6071",
      "label": "monsite.fr",
      "enabled": true,

      "connector": { "name": "sftp", "version": 1 },
      "params": {
        "host": "serveur.hebergeur.net",
        "remote_dir": "/home/monutilisateur/logs",
        "match": "",
        "match_mode": "o2switch"
      },

      "credentials_ref": "sw-monsite-fr",

      "schedule": { "every_minutes": 360 },

      "retention": { "mode": "keep_forever" }
    }
  ]
}
```

| Champ | Type | Description |
|---|---|---|
| `proto` | string | Version du **format**. Toujours `morfcollect/<n>`. Un manifeste dont le préfixe n'est pas `morfcollect/` est **rejeté**. |
| `provider.app` | string | Nom logique du fournisseur (affichage). Modifiable. |
| `provider.instance` | string | Identité stable `app@host` du fournisseur. Sert de **domaine de propriété** (voir §4.4). |
| `manifest_generation` | string (UUID) | Identifiant de la **lignée** de configuration (voir §1.4). |
| `revision` | number | Entier **strictement croissant à l'intérieur d'une génération**. Clé de réconciliation (voir §4.2). |
| `sources[]` | array | Déclaration de l'**état désiré complet** des sources de ce fournisseur. |
| `source_id` | string (UUID) | Identité **stable** de la source (voir §1.3). |
| `label` | string | Nom lisible (affichage). Modifiable, **jamais** utilisé comme identité. |
| `enabled` | bool | `false` = source déclarée mais en pause côté fournisseur (suspension). |
| `connector` | object | `{ name, version }` du connecteur. Détermine l'interprétation de `params` et des secrets (voir [§2](#2-le-contrat-de-connecteur)). |
| `params` | object | Paramètres **propres au connecteur**. Opaques pour le cœur générique. |
| `credentials_ref` | string | Référence vers les secrets stockés dans le coffre de morfCollector (voir §1.5). |
| `schedule` | object | Cadence de collecte souhaitée. |
| `retention` | object | Politique de conservation souhaitée (voir §5.4). |

**Règle de tolérance.** Un lecteur ignore toute clé qu'il ne connaît pas ; un
producteur qui ne renseigne pas une clé optionnelle ne l'émet pas. C'est la base
de l'extensibilité (voir §1.6).

### 1.3 Identifiants stables

Chaque source porte un `source_id` **stable pour toute sa vie** :

- un **UUID** assigné par le fournisseur **à la création** de la source ;
- **jamais réutilisé**, jamais dérivé d'une donnée modifiable.

Une source peut changer **d'hébergement, de nom de domaine, de chemin, de
protocole** : elle reste la **même source**. Donc :

```
source_id = identité
label     = affichage
```

C'est la transposition directe de la règle morfBeacon « chercher une capacité,
afficher un nom », et le même raisonnement que la migration int→UUID de
ComponentHub (voir [[componenthub-uuid-migration]]). Conséquence concrète côté
fournisseur : **SiteWatch doit persister un UUID par site**, indépendant de son
nom.

### 1.4 Génération et révision

Le couple `(manifest_generation, revision)` versionne le **contenu** d'un
fournisseur. Un simple entier ne suffit pas : après une **restauration de
sauvegarde**, une **réinstallation** ou un **retour à une ancienne
configuration**, un fournisseur pourrait réémettre une « révision 12 » qui n'est
pas celle que morfCollector a déjà vue.

- `manifest_generation` (UUID) identifie une **lignée** de configuration. Le
  fournisseur en **frappe une nouvelle** uniquement lorsqu'il repart d'une
  nouvelle base : première initialisation, restauration, réinstallation, retour
  arrière. Tant qu'on édite normalement, la génération **ne change pas**.
- `revision` croît **strictement** à l'intérieur d'une génération.

morfCollector compare donc d'abord la génération, ensuite la révision (voir §4.2
pour la logique complète). Deux révisions de générations différentes ne sont
**jamais** comparées entre elles.

### 1.5 Secrets d'accès (coffre local)

Les secrets (mot de passe, clé privée, jeton de pare-feu) ne circulent **pas** de
façon répétée et ne sont **pas** dupliqués en deux configurations indépendantes.
Le modèle retenu est un **envoi unique vers un coffre local chiffré** :

```
SiteWatch  ──(validation de la config)──►  POST /credentials  ──►  morfCollector
                                                                         │
                                                                         ▼
                                                             coffre local chiffré
```

- SiteWatch reste l'**interface** de configuration.
- À la **validation** de la configuration, il transmet les secrets **une seule
  fois** (endpoint dédié, voir §6.2).
- morfCollector les range dans son **propre coffre chiffré**, au repos.
- Ensuite, le manifeste ne porte plus qu'une `credentials_ref` ; SiteWatch **n'en
  reparle plus**.

Ce n'est ni un partage permanent, ni deux trousseaux indépendants : c'est une
**remise unique** suivie d'une conservation locale. Cohérent avec
[[r5-modele-confiance-acces-distant]] : arbitrer la confiance **avant** de
remettre un secret.

Règle **non négociable** : un secret n'est **jamais** présent dans `/status`,
jamais dans un heartbeat, jamais renvoyé par une route de lecture (voir §7.3).

### 1.6 Évolution du protocole

Comme `morfbeacon/1`, le contrat est conçu pour **grossir sans casser** :

1. **Toute évolution est une nouvelle clé optionnelle.** Nouveau champ de source,
   nouveau type de `schedule`, nouvelle politique de rétention : ajout, jamais
   remplacement.
2. **Ajouter une clé est rétrocompatible.** Un morfCollector ancien ignore une
   clé inconnue ; un fournisseur ancien ne l'émet pas.
3. On **n'incrémente `proto`** (`morfcollect/2`) que si la forme du manifeste
   change de façon incompatible - ce que cette discipline rend rare. Un connecteur
   évolue via sa **propre** version, sans toucher à `proto` (voir §2).

### 1.7 Suppression d'une source (dans le manifeste)

Le manifeste déclare l'**état désiré complet** des sources d'un fournisseur. Une
source **présente précédemment** puis **absente** d'un nouveau manifeste est
considérée **retirée** : collecte arrêtée, secrets oubliés, **archives
conservées**. C'est un **retrait**, pas une suppression (voir §3 et la règle
capitale du fournisseur hors ligne en §4.5).

---

## 2. Le contrat de connecteur

Un connecteur mérite son **propre contrat** : demain, à côté de `sftp`,
viendront `http`, `https`, `ftp`, `webdav`, `smb`, `rest`... On fige donc **dès
maintenant** la notion, pour que le cœur n'ait jamais à connaître le détail d'un
transport.

Un connecteur est une brique **nommée et versionnée** exposant cinq facettes :

| Facette | Rôle |
|---|---|
| **nom** | Identifiant stable (`sftp`, `http`, ...). Choisi dans le manifeste (`connector.name`). |
| **version** | Version du **sous-contrat** du connecteur (`connector.version`). Indépendante de `proto` : un connecteur peut évoluer sans casser le manifeste. |
| **paramètres** | Schéma des `params`, **propriété du connecteur**. Le cœur les transporte sans les lire. |
| **validation** | Le connecteur valide `params` + secrets **avant** d'armer une tâche. Un paramétrage invalide place la source en état opérationnel `ERROR` / `AUTH_FAILED` (voir §3.2), jamais un plantage. |
| **capacité** | Un identifiant stable annoncé dans `/status.capabilities` (voir §7.2), pour qu'un futur client sache **immédiatement** si son type de collecte est supporté. |

Le cœur générique ne connaît d'un connecteur que : son `name`/`version`, des
`params` opaques et une `credentials_ref`. Il lui **délègue** la validation, la
connexion, l'énumération des ressources distantes et la récupération. C'est ce
qui garantit que morfCollector **ne connaît jamais la logique métier** d'un
client (voir §8).

> Chaque connecteur concret fera l'objet d'un **document de sous-contrat** dédié
> (schéma de `params`, forme des secrets attendus, capacité annoncée). Le présent
> contrat fige seulement la **forme** commune ci-dessus.

---

## 3. Le cycle de vie d'une source

Une source porte **deux axes d'état orthogonaux** : un état **administratif**
(qui la commande) et, quand elle est active, un état **opérationnel** (ce qui lui
arrive). Les deux sont exposés ; SiteWatch les affiche.

### 3.1 États administratifs

```
   (absente)
       │  apparaît dans un manifeste
       ▼
   ┌────────┐  enabled:false / suspend(admin)   ┌───────────┐
   │ ACTIVE │ ─────────────────────────────────►│ SUSPENDED │
   │        │ ◄─────────────────────────────────│           │
   └───┬────┘  enabled:true  / resume(admin)    └───────────┘
       │
       │  absente d'un nouveau manifeste
       ▼
   ┌─────────┐  suppression explicite (admin)   ┌───────────┐
   │ RETIRED │ ────────────────────────────────►│ (détruite)│
   └─────────┘                                   └───────────┘
     collecte arrêtée, archives conservées
```

| Transition | Déclencheur | Effet |
|---|---|---|
| **création** | source (nouveau `source_id`) dans un manifeste | `ACTIVE`, planification armée |
| **mise à jour** | `source_id` connu, champs modifiés | paramètres/planning/rétention adaptés, historique conservé |
| **suspension** | `enabled:false` **ou** `POST /sources/{id}/suspend` | collecte en pause, configuration et archives conservées |
| **reprise** | `enabled:true` **ou** `POST /sources/{id}/resume` | collecte réarmée |
| **retrait** | source absente d'un manifeste reçu | `RETIRED` : collecte arrêtée, secrets oubliés, **archives conservées** |
| **suppression définitive** | `DELETE` d'administration **uniquement** | destruction des octets ; jamais déclenchée par un manifeste |

Rigueur : **suspension ≠ retrait** (l'une réversible par le fournisseur, l'autre
ne revient que par réapparition dans un manifeste avec le **même** `source_id`) ;
**retrait ≠ suppression** (le retrait ne perd aucune donnée).

### 3.2 États opérationnels

Définis **uniquement** quand la source est `ACTIVE`. Ce sont eux que SiteWatch
affiche au quotidien.

| État | Signification |
|---|---|
| `IDLE` | armée, au repos, aucune collecte imminente |
| `WAITING` | prochaine collecte planifiée, en attente de l'échéance |
| `COLLECTING` | collecte en cours |
| `ERROR` | dernière tentative échouée (erreur générique : énumération, écriture disque...) |
| `AUTH_FAILED` | échec d'authentification (identifiants refusés par la source) |
| `UNREACHABLE` | source injoignable (réseau, DNS, port) |

`ERROR` / `AUTH_FAILED` / `UNREACHABLE` décrivent le **résultat de la dernière
tentative** ; la tâche reste armée et retentera selon son `schedule`. Une source
`SUSPENDED` ou `RETIRED` n'a **pas** d'état opérationnel.

---

## 4. Le modèle de synchronisation

### 4.1 Propriété de la configuration

Le **fournisseur** possède la configuration des sources ; il en est la seule
source de vérité. morfCollector détient une **copie opérationnelle** dérivée du
dernier manifeste appliqué. L'utilisateur ne configure **jamais** une source dans
morfCollector : une seule saisie, dans SiteWatch.

### 4.2 Réconciliation

À chaque démarrage du fournisseur (et à la demande) :

1. **découverte** de morfCollector via morfBeacon (aucune IP configurée) ;
2. **vérification de compatibilité** : lecture du bloc `collect` de `/status`
   (voir §7.2) et d'un `proto` commun ;
3. **comparaison** : lecture de `GET /manifest/state` (génération + révision
   appliquées) ; si identiques aux siennes, **rien à faire** ; sinon, il **pousse**
   son manifeste (`POST /manifest`).

À la réception, morfCollector décide selon le couple `(génération, révision)` :

| Situation | Décision |
|---|---|
| **génération connue**, révision **supérieure** | applique la réconciliation |
| **génération connue**, révision **inférieure ou égale** | **ignore** (rejoue/doublon), `409` |
| **génération inconnue** (nouvelle lignée) | applique **sans** comparer la révision : c'est une nouvelle base autoritaire |

La réconciliation est **déclarative**, indexée par `source_id` :

| Cas | Action |
|---|---|
| `source_id` présent, inconnu | **création** |
| `source_id` présent, connu | **mise à jour** (diff des champs) |
| `source_id` connu, absent du manifeste | **retrait** (→ `RETIRED`) |

Parce que les `source_id` sont stables, un changement de génération (restauration,
réinstallation) **raccroche** les sources à leur historique existant au lieu de
tout recollecter. La réconciliation est **idempotente**.

### 4.3 Gestion des conflits

| Conflit | Règle |
|---|---|
| Révision inférieure/égale dans une génération connue | **ignoré**, `409` |
| Manifeste mal formé ou `proto` inconnu | **rejeté** en bloc, état courant préservé, `422` |
| Manifeste touchant une source d'**un autre fournisseur** | **rejeté**, `409` (voir §4.4) |
| Suspension **opérateur** (admin) vs `enabled:true` du manifeste | l'override opérateur **est prioritaire** (voir §4.3.1) |

#### 4.3.1 Override opérateur (règle gelée)

Une suspension demandée par l'opérateur est **prioritaire sur le manifeste**.
Elle **survit aux réconciliations successives** et ne disparaît qu'après un appel
explicite à `POST /sources/{id}/resume`. Un manifeste `enabled:true` exprime
l'état *souhaité par le fournisseur*, mais **n'annule jamais** une décision
d'exploitation prise volontairement par l'utilisateur.

morfCollector conserve donc **trois valeurs séparées** par source, toutes
exposées dans `/sources/{id}` pour que la suspension ne devienne jamais un état
fantôme incompréhensible depuis SiteWatch :

```json
{
  "manifest_enabled": true,
  "operator_override": "suspended",
  "effective_state": "suspended"
}
```

- `manifest_enabled` : ce que déclare le dernier manifeste appliqué.
- `operator_override` : décision opérateur explicite (`suspended`, ou absent si
  aucune). Posée par `suspend`, levée par `resume`.
- `effective_state` : l'état réellement appliqué. Il vaut `suspended` dès que
  l'override **ou** le manifeste le demande ; il ne redevient `active` que si le
  manifeste l'autorise **et** qu'aucun override ne subsiste.

### 4.4 Propriété et isolation entre fournisseurs

Chaque source appartient au fournisseur qui l'a créée (`provider.instance`).
morfCollector **refuse** un manifeste qui modifierait ou retirerait la source
d'un autre fournisseur. Un `source_id` vit dans le **domaine** d'un seul
fournisseur.

### 4.5 Fournisseur absent depuis longtemps

Règle **capitale**, au cœur de la raison d'être du service :

> L'absence **du fournisseur** ne retire **jamais** une source. Le retrait
> n'advient que par un **manifeste reçu** qui omet la source.

Si SiteWatch reste fermé des semaines, morfCollector **continue** de collecter
selon les `schedule`. Un fournisseur injoignable n'est pas un signal de
suppression : c'est le fonctionnement normal.

---

## 5. Le modèle de stockage

### 5.1 Objet collecté (`CollectedObject`)

L'unité conservée n'est pas seulement une « archive » de journaux : c'est un
**objet collecté** générique. Aujourd'hui `access.log.gz` ; demain `backup.sql.gz`,
`rapport.pdf`, `export.csv`, `image.zip`. L'archive de journaux n'est qu'un **cas
particulier**. Le concept du cœur est donc `CollectedObject` (l'interface
utilisateur peut continuer d'afficher « Archives »).

Un `CollectedObject` est **conservé tel quel** : jamais modifié, fusionné,
décompressé ni interprété. La même donnée pourra servir demain à plusieurs
applications - ce que seule la conservation à l'identique garantit. Il porte un
`object_id` stable et appartient à un `source_id`.

### 5.2 Index

morfCollector tient un **index** (métadonnées, pas contenu) exposant par source :

| Métadonnée | Rôle |
|---|---|
| `object_id` | identifiant stable de l'objet |
| `source_id` | source d'appartenance |
| `original_name` | nom d'origine chez le fournisseur distant |
| `size`, `hash` | taille et empreinte (intégrité, déduplication) |
| `collected_at` | date de récupération |
| `period` | période **couverte** par l'objet (voir §5.3) |

L'index alimente « espace occupé », « dernière collecte », « périodes
disponibles », « erreurs ». Il ne contient **aucune** donnée issue du contenu.

### 5.3 Périodes

Chaque objet **couvre une période** (typiquement une journée). Le `period` est
déduit du nom ou fourni par le fournisseur ; morfCollector ne l'infère **pas** en
lisant le contenu. La détection d'un trou relève du client, pas de morfCollector.

### 5.4 Politiques de conservation

La `retention` décrit ce que morfCollector fait de **sa propre copie** :

| `mode` | Effet |
|---|---|
| `keep_forever` | **défaut**. Aucune purge automatique. C'est la vocation du service. |
| `keep_days` | conserve les objets dont la période est plus récente que `days`. |
| `keep_size` | plafonne l'espace par source ; purge les plus anciens au-delà. |

Garde-fous : une purge ne touche **que** les copies locales, jamais la source
distante ; **le retrait d'une source n'applique aucune rétention** (un `RETIRED`
garde tout son historique jusqu'à décision explicite).

### 5.5 Suppression des données

Seules des actions **explicites** d'administration (voir §6.3) détruisent des
octets. Aucune synchronisation, aucun retrait, aucun manifeste ne supprime jamais
de données.

---

## 6. L'API HTTP

API HTTP/1.1, réponses `application/json`. Trois familles : **cadre** (compatible
morfBeacon), **lecture**, **administration**.

### 6.1 Endpoints de cadre (publics)

| Méthode | Chemin | Rôle |
|---|---|---|
| `GET` | `/status` | Détail morfBeacon : état, métriques agrégées, blocs `collect` et `capabilities`, liste d'API (voir §7.2). |
| `GET` | `/healthz` | Sonde de vie : `{"status":"ok"}`. |

### 6.2 Endpoints de configuration et de lecture

| Méthode | Chemin | Rôle | Codes |
|---|---|---|---|
| `GET` | `/manifest/state` | Génération + révision appliquées pour un fournisseur (`instance`). Permet le « rien à faire ». | `200`, `404` |
| `POST` | `/manifest` | Le fournisseur pousse son manifeste (voir §4.2). | `200`, `409`, `422` |
| `POST` | `/credentials` | Remise **unique** des secrets vers le coffre chiffré (voir §1.5). Jamais relu. | `204`, `400` |
| `GET` | `/sources` | Liste : état admin, état opérationnel, dernière collecte, nombre d'objets, espace. | `200` |
| `GET` | `/sources/{id}` | Détail d'une source, dont le triplet `manifest_enabled` / `operator_override` / `effective_state` (voir §4.3.1). | `200`, `404` |
| `GET` | `/sources/{id}/periods` | Périodes disponibles. | `200`, `404` |
| `GET` | `/sources/{id}/objects` | Liste des objets collectés (métadonnées d'index). | `200`, `404` |
| `GET` | `/objects/{object_id}` | **Récupère l'original** conservé. C'est par là que le client lit la copie locale. | `200`, `404` |

### 6.3 Endpoints d'administration

Exécutent les actions de la vision. **Toutes** sont réalisées par morfCollector ;
le client ne modifie jamais un fichier directement.

| Méthode | Chemin | Rôle | Codes |
|---|---|---|---|
| `POST` | `/sources/{id}/collect` | Collecte immédiate (asynchrone). | `202`, `404`, `409` |
| `POST` | `/sources/{id}/suspend` | Suspend (override opérateur). | `200`, `404` |
| `POST` | `/sources/{id}/resume` | Reprend. | `200`, `404` |
| `DELETE` | `/objects/{object_id}` | Supprime **un** objet. Irréversible. | `200`, `404` |
| `DELETE` | `/sources/{id}/objects` | Supprime **toutes** les copies d'une source. Irréversible. | `200`, `404` |
| `DELETE` | `/sources/{id}` | Supprime définitivement une source `RETIRED` et son historique. | `200`, `404`, `409` |

### 6.4 Codes de retour

| Code | Signification |
|---|---|
| `200` | Succès (lecture, ou action synchrone appliquée). |
| `202` | Action **asynchrone acceptée** (collecte lancée ; résultat via `/status` et l'index). |
| `204` | Succès sans corps (remise de secrets). |
| `400` | Requête syntaxiquement invalide. |
| `404` | Ressource inconnue. |
| `405` | Méthode non autorisée sur la route. |
| `409` | Conflit : révision périmée, source d'un autre fournisseur, suppression d'une source non retirée. |
| `422` | Manifeste bien formé mais **non applicable** (`proto` inconnu, champ requis absent). |

### 6.5 Modèle de confiance

morfCollector **n'implémente pas d'authentification par service**. Il s'appuie
sur la confiance du LAN, comme le reste de l'écosystème. Tout accès **hors LAN**
relève d'un composant dédié (voir [[r5-modele-confiance-acces-distant]]), pas
d'une authentification ajoutée ici.

---

## 7. Les annonces morfBeacon

morfCollector suit `morfbeacon/1` : heartbeat UDP maigre pour la **présence**,
`/status` HTTP pour le **détail**.

### 7.1 Heartbeat (présence)

Datagramme standard `morfbeacon/1`, enrichi de la capacité **`collection`** pour
que les fournisseurs le trouvent **par ce qu'il sait faire**, pas par son nom.

morfCollector annonce la capacité `collection`, qui signifie :

> le service sait recevoir un contrat `morfcollect`, exécuter des collectes
> planifiées et conserver localement les objets récupérés.

La capacité `storage` serait trop large : morfCollector ne fournit pas seulement
un espace de stockage, il **exécute des missions planifiées** et conserve les
objets obtenus. `collection` est ajoutée au **registre morfBeacon de façon
additive**, sans changer la version `morfbeacon/1`.

**Règle de découverte** (impérative) : une application cliente découvre le service
par sa **capacité** (`capability == collection`), puis affiche son **nom réel** à
l'utilisateur. Le nom `morfCollector` ne doit **jamais** servir de clé technique
de découverte - il est modifiable, la capacité ne l'est pas.

### 7.2 Ce que morfCollector annonce

`GET /status` porte, outre les champs standard morfBeacon (`app`, `host`,
`version`, `state`, `uptime_s`, `ts`, `api`) :

- `metrics` : objet **agrégé et non sensible** -
  `{ "sources": 12, "sources_active": 10, "objects": 4210, "bytes": 5368709120,
  "last_collect_ts": 1753600000, "errors": 0 }`.
- `collect` **et** `capabilities` : nouvelles clés optionnelles de premier niveau.
  `collect` décrit le contrat supporté ; `capabilities` liste ce que ce
  morfCollector sait faire, pour qu'un client sache **immédiatement** si son type
  de collecte est géré :

```json
{
  "collect": {
    "proto": ["morfcollect/1"],
    "manifest_endpoint": "/manifest",
    "state_endpoint": "/manifest/state"
  },
  "capabilities": ["sftp", "http", "zip", "delete", "retention"]
}
```

Ajouter ces clés est **rétrocompatible** au sens de `morfbeacon/1` : un
observateur qui les ignore n'est pas affecté.

### 7.3 Ce que morfCollector ne doit JAMAIS annoncer

`/status` et le heartbeat sont **publics** sur le LAN. En sont **exclus sans
exception** :

- tout **secret d'accès** : mot de passe, clé privée, jeton de pare-feu ;
- toute **coordonnée de source distante** : hôte, chemin, utilisateur d'hébergement ;
- le **contenu** du manifeste (liste des sources d'un fournisseur, leurs `params`) ;
- le **contenu** des objets collectés ;
- les données d'**un autre fournisseur** croisées entre elles.

Seules des **métriques agrégées** et le **contrat technique** sont publics. Le
détail par source se lit par l'API de lecture (§6.2), sous confiance LAN, pas par
l'annonce diffusée.

---

## 8. Les principes d'architecture

Ces principes sont des **clauses du contrat**. Toute implémentation qui les viole
le rompt, même si elle « marche ».

| Principe | Clause contraignante |
|---|---|
| **Exécutant, jamais décideur** | morfCollector ne collecte **jamais** de sa propre initiative. Il ne découvre pas, ne scanne pas, ne cherche pas quoi sauvegarder. Il n'exécute que des missions confiées par une application propriétaire, qui reste seule responsable de définir les ressources à conserver. Ce garde-fou l'empêche de devenir un « super service » mêlant découverte, collecte, analyse et stockage. |
| **Une seule configuration utilisateur** | Les sources se déclarent **exclusivement** chez le fournisseur. Aucune interface de création/édition de sources dans morfCollector. |
| **Les applications restent propriétaires de leur métier** | morfCollector conserve des originaux et ne produit **aucune** analyse, statistique, détection ni graphique. |
| **morfCollector ignore la logique métier des clients** | Il ne connaît ni WordPress, ni Apache, ni SiteWatch. `params` et secrets sont **opaques** au cœur, interprétés par le seul connecteur (voir §2). |
| **Fonctionnement autonome des applications** | Sans morfCollector, le fournisseur fonctionne **exactement** comme avant. L'écosystème est une amélioration, jamais une obligation. |
| **Fonctionnement autonome de morfCollector** | Une fois configuré, il poursuit ses collectes planifiées, fournisseur fermé (voir §4.5). |
| **Découverte automatique** | Présence via morfBeacon uniquement. **Aucune IP** configurée. |
| **Aucune dépendance forte** | Le lien est opportuniste : présent, il est privilégié ; absent, il ne manque à personne. |

---

## Ce que ce contrat fige

- Le **format** du manifeste (`morfcollect/1`), le couple
  `(manifest_generation, revision)` et la règle d'extensibilité.
- La **forme** commune d'un connecteur (nom, version, paramètres, validation,
  capacité) et le fait que le cœur en ignore le détail.
- Les **deux axes d'état** d'une source (administratif, opérationnel) et leurs
  transitions.
- La **réconciliation** déclarative par `source_id` et les règles de conflit.
- La séparation **retrait / suppression** et l'absence d'effet de bord destructeur.
- Le **coffre local chiffré** et la remise unique des secrets.
- La liste et la **sémantique** des endpoints et les codes de retour.
- Ce que morfCollector **annonce** et **n'annonce jamais**.

Ce que ce contrat **ne fige pas** : le langage et les classes, le format de
l'index (SQLite, JSON...), l'ordonnanceur, la disposition sur le disque, et le
détail interne de chaque connecteur (objet de son propre sous-contrat).

> `morfcollect/1` est **gelé**. L'implémentation peut remplacer les
> `ExampleModule` par `Source`, `Connector`, `CollectionTask`, `CollectedObject`
> et `RetentionPolicy`, en respectant ce contrat. Toute évolution se fait par
> ajout de clés optionnelles ; seul un changement incompatible incrémenterait
> `proto`.
