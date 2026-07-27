/*
 * morfCollector
 * Copyright (C) 2026 morfredus
 * SPDX-License-Identifier: GPL-3.0-only
 */

#pragma once
#include <QString>

namespace morfcollector {

// Version, injectee par CMake depuis le fichier VERSION.
#ifndef MORFCOLLECTOR_VERSION
#  define MORFCOLLECTOR_VERSION "dev"
#endif

inline QString version() { return QStringLiteral(MORFCOLLECTOR_VERSION); }

// Version du protocole HTTP/JSON expose. >>> A ADAPTER si l'API change. <<<
inline constexpr const char* kProtocol = "morfcollector/1";

} // namespace morfcollector
