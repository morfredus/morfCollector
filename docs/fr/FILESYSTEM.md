# Emplacements des fichiers - morfCollector

morfCollector suit la doctrine du parc morfSystem (référence complète :
`morfTemplateService/docs/fr/FILESYSTEM.md`). Ce document précise sa disposition
propre.

## Les trois zones

| Zone | Contenu | Emplacement (Linux) | Écriture |
|------|---------|---------------------|----------|
| Programme | binaire `morfcollector` | `/opt/morfcollector` | non |
| Config admin | `morfcollector.json` | `/etc/morfsystem/morfcollector` | non |
| État persistant | données collectées + coffre | `/var/lib/morfsystem/morfcollector` | oui |

Sous Windows, l'état se replie sous
`%ProgramData%\morfsystem\morfcollector\state`.

## Détail de l'état persistant

Sous la racine d'état, deux sous-dossiers **séparés** :

```
/var/lib/morfsystem/morfcollector/
  data/              objets collectés + index JSON (la source de vérité)
    objects/
    index.json
  vault/             coffre chiffré des secrets
    vault.key        clé de chiffrement (générée à l'installation, mode 600)
    vault.enc        secrets chiffrés au repos (AES-256-GCM)
```

Le coffre est un sous-dossier distinct des données : une sauvegarde ou une copie
de `data/` n'emporte jamais la clé, ce qui préserve le sens du chiffrement au
repos (CONTRAT.md §1.5).

## Pourquoi l'état n'est ni dans /etc ni dans /opt

Le coffre est de l'**état généré** par le service (une clé, des secrets
chiffrés), pas de la configuration administrateur. Le placer dans
`/etc/morfsystem/morfcollector` (propriété `root`) empêchait le service, qui
tourne sous son propre utilisateur, d'y créer sa clé : l'initialisation du coffre
échouait en silence et toutes les sources restaient en `auth_failed`. Les données
collectées, elles, étaient sous `/opt/morfcollector/data`, ce qui mélangeait
programme et données variables.

Depuis la version 0.4.0, les deux vivent sous `/var/lib`, créé et possédé par
l'utilisateur du service grâce à `StateDirectory=morfsystem/morfcollector` dans
l'unité systemd. Le service résout sa racine d'état via `$STATE_DIRECTORY` (posé
par systemd), avec repli sur `/var/lib/morfsystem/morfcollector`.

## Surcharges

La configuration admin peut fixer explicitement :

- `storage_root` : remplace `<état>/data` ;
- `vault_root` : remplace `<état>/vault`.

Ces surcharges restent utiles pour un stockage sur un autre volume, mais ne sont
plus nécessaires pour contourner un problème de droits : la disposition par
défaut est désormais accessible en écriture.

## Migration depuis une version antérieure

Une installation antérieure à 0.4.0 range l'état ailleurs (`/opt/morfcollector/data`
et, selon les cas, un `vault_root` de contournement). Pour migrer sans perdre les
copies conservées ni le coffre, déplacer les données existantes vers la nouvelle
racine d'état avant de démarrer la version 0.4.0 :

```bash
sudo systemctl stop morfcollector
sudo install -d -o <user> -g <user> /var/lib/morfsystem/morfcollector
sudo mv /opt/morfcollector/data /var/lib/morfsystem/morfcollector/data
# si un vault_root de contournement existait (ex. /opt/morfcollector/vault) :
sudo mv /opt/morfcollector/vault /var/lib/morfsystem/morfcollector/vault
sudo chown -R <user>:<user> /var/lib/morfsystem/morfcollector
# retirer tout "vault_root"/"storage_root" de contournement du fichier de config
sudo systemctl start morfcollector
```
