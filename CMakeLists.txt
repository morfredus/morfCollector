cmake_minimum_required(VERSION 3.21)

# =============================================================================
#  morfCollector — squelette reutilisable de service morfSystem.
#
#  POUR CREER UN NOUVEAU SERVICE : le plus simple est le script de clonage
#      scripts/new-service.sh   morfmonservice MorfMonService   (Linux/macOS)
#      scripts/new-service.ps1  morfmonservice MorfMonService   (Windows)
#  qui copie ce projet en remplacant les noms. Sinon, remplacer a la main :
#      morfCollector  (nom du projet / bibliotheque)
#      morfcollector         (namespace, dossier include, binaire, unite systemd)
#      MORFCOLLECTOR         (prefixe de macro)
#
#  Ensuite : coder le metier dans src/ExampleModule.* (ou de nouveaux modules),
#  adapter la fabrique (src/ModuleFactory.cpp) et la liste de sources ci-dessous.
# =============================================================================

# Version pilotee par le fichier VERSION (convention morfSystem).
set(MORFCOLLECTOR_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MORFCOLLECTOR_VERSION_FILE}")
file(STRINGS "${MORFCOLLECTOR_VERSION_FILE}" MORFCOLLECTOR_VERSION_FROM_FILE LIMIT_COUNT 1)
string(STRIP "${MORFCOLLECTOR_VERSION_FROM_FILE}" MORFCOLLECTOR_VERSION_FROM_FILE)

project(morfCollector VERSION "${MORFCOLLECTOR_VERSION_FROM_FILE}" LANGUAGES CXX)

# --- Standard C++ ---------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt : generation automatique du moc (classes QObject) -----------------
set(CMAKE_AUTOMOC ON)

# Core + Network + Concurrent : serveur HTTP, JSON, et collectes reseau deportees
# sur un thread de travail (QtConcurrent).
find_package(Qt6 REQUIRED COMPONENTS Core Network Concurrent)

# OpenSSL (libcrypto) : coffre chiffre des secrets (AES-256-GCM).
find_package(OpenSSL REQUIRED)

# --- libssh2 (connecteur sftp) : dependance OPTIONNELLE --------------------
# Le nom du package/cible varie selon la chaine (calque sur SiteWatch). Absente,
# le connecteur 'sftp' n'est simplement pas compile ; le service fonctionne avec
# les autres connecteurs.
find_package(libssh2 CONFIG QUIET)
if(NOT TARGET libssh2::libssh2 AND NOT TARGET Libssh2::libssh2)
    find_package(Libssh2 CONFIG QUIET)
endif()
set(MORFCOLLECTOR_SFTP_TARGET "")
if(TARGET libssh2::libssh2)
    set(MORFCOLLECTOR_SFTP_TARGET libssh2::libssh2)
elseif(TARGET Libssh2::libssh2)
    set(MORFCOLLECTOR_SFTP_TARGET Libssh2::libssh2)
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(LIBSSH2 QUIET IMPORTED_TARGET libssh2)
        if(TARGET PkgConfig::LIBSSH2)
            set(MORFCOLLECTOR_SFTP_TARGET PkgConfig::LIBSSH2)
        endif()
    endif()
endif()

# --- Dependance morfBeacon (annonce de presence sur le LAN) ---------------
# morfBeacon est EMBARQUE (copie vendoree) dans third_party/morf/beacon, lie
# statiquement : le projet se compile sans dependre d'aucun depot externe, a
# l'identique sous Windows, Linux x64 et Raspberry Pi (ARM64).
# Ne pas editer third_party/ ; resynchroniser avec scripts/sync-morf.(sh|ps1).
if(NOT TARGET morfBeacon::morfBeacon)
    add_subdirectory(third_party/morf/beacon)
endif()

# --- Bibliotheque ---------------------------------------------------------
# Plan de CONTROLE du contrat morfcollect/1 (manifeste, etats, API). Le plan de
# DONNEES (connecteurs concrets, index sur disque, coffre chiffre, retention)
# s'ajoutera ici, derriere des interfaces (voir docs/fr/CONTRAT.md).
add_library(morfCollector STATIC
    src/Manifest.cpp
    src/Source.cpp
    src/SourceRegistry.cpp
    src/ObjectStore.cpp
    src/Vault.cpp
    src/FsConnector.cpp
    src/GitHubTrafficConnector.cpp
    src/Collector.cpp
    src/HttpServer.cpp
    src/Service.cpp
    include/morfcollector/Domain.h
    include/morfcollector/Manifest.h
    include/morfcollector/Source.h
    include/morfcollector/SourceRegistry.h
    include/morfcollector/IConnector.h
    include/morfcollector/CollectedObject.h
    include/morfcollector/ObjectStore.h
    include/morfcollector/Vault.h
    include/morfcollector/FsConnector.h
    include/morfcollector/GitHubTrafficConnector.h
    include/morfcollector/Collector.h
    include/morfcollector/ServiceConfig.h
    include/morfcollector/SelfDescription.h
    include/morfcollector/HttpServer.h
    include/morfcollector/Service.h
    include/morfcollector/Version.h
)
add_library(morfCollector::morfCollector ALIAS morfCollector)

target_include_directories(morfCollector PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(morfCollector
    PUBLIC  Qt6::Core Qt6::Network
    PRIVATE Qt6::Concurrent morfBeacon::morfBeacon OpenSSL::Crypto)

# --- Connecteur sftp (si libssh2 disponible) ------------------------------
if(MORFCOLLECTOR_SFTP_TARGET)
    target_sources(morfCollector PRIVATE
        src/SftpConnector.cpp
        include/morfcollector/SftpConnector.h)
    target_compile_definitions(morfCollector PRIVATE MORFCOLLECTOR_HAVE_SFTP)
    target_link_libraries(morfCollector PRIVATE ${MORFCOLLECTOR_SFTP_TARGET})
    if(WIN32)
        target_link_libraries(morfCollector PRIVATE ws2_32)   # Winsock, requis par libssh2
    endif()
    message(STATUS "morfCollector : connecteur sftp ACTIVE (${MORFCOLLECTOR_SFTP_TARGET})")
else()
    message(STATUS "morfCollector : libssh2 introuvable, connecteur sftp DESACTIVE")
endif()

# Numero de version accessible depuis le code.
target_compile_definitions(morfCollector PUBLIC MORFCOLLECTOR_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(morfCollector PRIVATE /utf-8)
endif()

# --- Cible service (le demon autonome) ------------------------------------
add_subdirectory(service)

# --- Exemple (compile seulement en build autonome, pas via add_subdirectory) -
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(_morfcollector_standalone ON)
else()
    set(_morfcollector_standalone OFF)
endif()
option(MORFCOLLECTOR_BUILD_EXAMPLES "Compiler l'exemple de demonstration" ${_morfcollector_standalone})

if(MORFCOLLECTOR_BUILD_EXAMPLES)
    add_subdirectory(examples/minimal)
endif()
