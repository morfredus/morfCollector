# Demon de service morfCollector : execute la bibliotheque en tache de fond.
# Le binaire s'appelle « morfcollector » (comme l'unite systemd et /opt/morfcollector).
add_executable(morfcollector main.cpp)

target_link_libraries(morfcollector PRIVATE morfCollector::morfCollector Qt6::Core)

if(MSVC)
    target_compile_options(morfcollector PRIVATE /utf-8)
endif()

install(TARGETS morfcollector RUNTIME DESTINATION bin)
