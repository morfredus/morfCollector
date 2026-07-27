find_package(Qt6 REQUIRED COMPONENTS Core)

add_executable(morfcollector_demo main.cpp)
target_link_libraries(morfcollector_demo PRIVATE morfCollector::morfCollector Qt6::Core)

if(MSVC)
    target_compile_options(morfcollector_demo PRIVATE /utf-8)
endif()
