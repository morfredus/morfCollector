/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>

// -----------------------------------------------------------------------------
// Domain.h : vocabulaire du contrat morfcollect/1 (docs/fr/CONTRAT.md).
//
// Une source porte DEUX axes d'etat orthogonaux (contrat, section 3) :
//   - administratif : ACTIVE / SUSPENDED / RETIRED (qui la commande) ;
//   - operationnel  : IDLE / WAITING / COLLECTING / ERROR / AUTH_FAILED /
//                     UNREACHABLE (ce qui lui arrive), defini uniquement quand
//                     la source est effectivement ACTIVE.
//
// Le triplet de suspension (contrat 4.3.1) se calcule a partir de deux entrees
// conservees separement -- l'etat souhaite par le manifeste et l'eventuel
// override operateur -- pour que la suspension ne soit jamais un etat fantome.
// -----------------------------------------------------------------------------
namespace morfcollector {

// Etat administratif effectif d'une source.
enum class AdminState {
    Active,     // declaree, non suspendue, presente dans le manifeste
    Suspended,  // en pause (manifeste enabled:false OU override operateur)
    Retired,    // absente du dernier manifeste recu ; collecte arretee, archives gardees
};

// Etat operationnel (seulement pertinent quand AdminState == Active).
enum class OperationalState {
    Idle,        // armee, au repos
    Waiting,     // prochaine collecte planifiee, en attente de l'echeance
    Collecting,  // collecte en cours
    Error,       // derniere tentative en echec (erreur generique)
    AuthFailed,  // echec d'authentification (identifiants refuses)
    Unreachable, // source injoignable (reseau, DNS, port)
};

inline const char* toString(AdminState s) {
    switch (s) {
        case AdminState::Active:    return "active";
        case AdminState::Suspended: return "suspended";
        case AdminState::Retired:   return "retired";
    }
    return "active";
}

inline const char* toString(OperationalState s) {
    switch (s) {
        case OperationalState::Idle:        return "idle";
        case OperationalState::Waiting:     return "waiting";
        case OperationalState::Collecting:  return "collecting";
        case OperationalState::Error:       return "error";
        case OperationalState::AuthFailed:  return "auth_failed";
        case OperationalState::Unreachable: return "unreachable";
    }
    return "idle";
}

} // namespace morfcollector
