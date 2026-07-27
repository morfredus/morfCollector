/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "morfcollector/SourceRegistry.h"

#include <QSet>

namespace morfcollector {

SourceRegistry::ApplyResult SourceRegistry::apply(const Manifest& manifest) {
    const QString provider = manifest.provider.instance;

    // --- 1. Propriete : un source_id appartient a un seul fournisseur --------
    //     (CONTRAT.md §4.4). On refuse EN BLOC, l'etat courant est preserve.
    for (const ManifestSource& ms : manifest.sources) {
        auto it = m_sources.constFind(ms.sourceId);
        if (it != m_sources.constEnd() && it->providerInstance() != provider) {
            return { Apply::RejectedOwnership, 0, QString(),
                     QStringLiteral("source_id %1 appartient a un autre fournisseur")
                         .arg(ms.sourceId) };
        }
    }

    // --- 2. Decision generation / revision (CONTRAT.md §4.2) -----------------
    const ProviderState prev = m_providers.value(provider);
    const bool knownGeneration = (prev.revision >= 0) && (prev.generation == manifest.generation);
    if (knownGeneration && manifest.revision <= prev.revision) {
        // Rejoue d'un ancien etat ou doublon : ignore.
        return { Apply::IgnoredStale, prev.revision, prev.generation,
                 QStringLiteral("revision %1 <= revision appliquee %2")
                     .arg(manifest.revision).arg(prev.revision) };
    }
    // generation inconnue (nouvelle lignee) => on applique sans comparer la
    // revision : c'est une nouvelle base autoritaire (restauration, reinstall).

    // --- 3. Reconciliation declarative par source_id -------------------------
    QSet<QString> present;
    for (const ManifestSource& ms : manifest.sources) {
        present.insert(ms.sourceId);
        auto it = m_sources.find(ms.sourceId);
        if (it == m_sources.end()) {
            m_sources.insert(ms.sourceId, Source(ms, provider));   // creation
        } else {
            const bool wasRetired = (it->adminState() == AdminState::Retired);
            it->applyDefinition(ms);                                // mise a jour
            if (wasRetired)
                it->unretire();
        }
    }

    // Sources de CE fournisseur absentes du manifeste -> retrait (archives gardees).
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it) {
        if (it->providerInstance() == provider && !present.contains(it->id()))
            it->retire();
    }

    m_providers.insert(provider, { manifest.generation, manifest.revision });

    return { Apply::Applied, manifest.revision, manifest.generation,
             QStringLiteral("%1 source(s) reconciliee(s)").arg(manifest.sources.size()) };
}

bool SourceRegistry::stateOf(const QString& providerInstance,
                             QString& generation, qint64& revision) const {
    auto it = m_providers.constFind(providerInstance);
    if (it == m_providers.constEnd() || it->revision < 0)
        return false;
    generation = it->generation;
    revision   = it->revision;
    return true;
}

Source* SourceRegistry::find(const QString& sourceId) {
    auto it = m_sources.find(sourceId);
    return it == m_sources.end() ? nullptr : &it.value();
}

const Source* SourceRegistry::find(const QString& sourceId) const {
    auto it = m_sources.constFind(sourceId);
    return it == m_sources.constEnd() ? nullptr : &it.value();
}

QList<Source*> SourceRegistry::all() {
    QList<Source*> v;
    v.reserve(m_sources.size());
    for (auto it = m_sources.begin(); it != m_sources.end(); ++it)
        v.push_back(&it.value());
    return v;
}

bool SourceRegistry::remove(const QString& sourceId) {
    return m_sources.remove(sourceId) > 0;
}

QJsonArray SourceRegistry::sourcesJson() const {
    QJsonArray arr;
    for (const Source& s : m_sources)
        arr.append(s.summaryJson());
    return arr;
}

QJsonObject SourceRegistry::metrics() const {
    int active = 0;
    for (const Source& s : m_sources) {
        if (s.adminState() == AdminState::Active)
            ++active;
    }
    QJsonObject m;
    m["sources"]        = m_sources.size();
    m["sources_active"] = active;
    // objets / octets / derniere collecte : ajoutes par le Collector depuis
    // l'ObjectStore, seule source de verite des compteurs.
    return m;
}

} // namespace morfcollector
