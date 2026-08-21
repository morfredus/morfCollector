# Connecteur `github-traffic/1`

Retour à l'[index de la documentation](README.md).

Sous-contrat du connecteur `github-traffic`, conforme à [CONTRAT.md §2](CONTRAT.md).
Le cœur de morfCollector transporte `params` et `credentials_ref` sans les lire.

## Rôle

Exécuter une collecte quotidienne des métriques GitHub d'un dépôt **déjà choisi**
par SiteWatch. morfCollector conserve un **instantané brut daté**. Il n'additionne
pas les vues, ne calcule pas de deltas de téléchargements, n'invente pas les jours
perdus au-delà de la fenêtre de 14 jours de GitHub.

## Identité

| Facette | Valeur |
|---|---|
| `connector.name` | `github-traffic` |
| `connector.version` | `1` |
| capacité `/status` | `github-traffic` |

## Paramètres (`params`)

```json
{
  "owner": "morfredus",
  "repository": "morfCollector"
}
```

Aucun jeton ici.

## Secrets (`credentials_ref`)

Le coffre attend `{ "token": "…" }` : un fine-grained PAT avec **Administration :
Read-only** sur les dépôts suivis. Le manifeste ne porte que la référence. Le
jeton n'apparaît ni dans `/status`, ni dans les objets collectés, ni dans les
journaux.

## Objet conservé

Un fichier JSON par jour UTC, nommé `github-traffic-YYYY-MM-DD.json`.

Contrat de l'objet : `github-traffic/1`. Champs principaux :

- `owner`, `repository`, `full_name`
- `collected_at` (ISO-8601 UTC)
- `period` : fenêtre glissante de 14 jours annoncée par GitHub
- `endpoints` : statut HTTP de chaque appel (`repo`, `views`, `clones`, `paths`,
  `referrers`, `releases`)
- `partial` : vrai si au moins un endpoint a échoué
- `rate_limit`
- `data` : payloads GitHub tels quels
- `diagnostics` : messages **sans secret**

Une erreur sur les référents ne fait pas perdre les vues déjà reçues : l'objet
est alors `partial`.

## Particularités GitHub (rappel pour le consommateur)

Ces règles sont **l'affaire de morfAnalytics**, pas du connecteur :

- les journées de vues/clones se répètent dans la fenêtre glissante : upsert par
  `(dépôt, métrique, date UTC)` ;
- la somme des uniques quotidiens n'est pas un unique de période ;
- pages et référents sont un classement glissant de 14 jours, pas un historique
  quotidien ;
- `download_count` est cumulatif : historiser les snapshots puis calculer un
  delta, jamais un téléchargement négatif.

Si la collecte s'arrête plus de 14 jours, GitHub ne rend plus le trafic ancien.
Le trou est signalé à l'import ; il n'est pas comblé.
