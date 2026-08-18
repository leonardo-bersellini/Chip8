cmake_minimum_required(VERSION 3.16)
project(Chip8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_PREFIX_PATH "C:/Users/Utente/Desktop/C++/utilities/sfml/")
find_package(SFML 2.6.2 COMPONENTS graphics window system REQUIRED)

add_executable(Chip8 
    main.cpp

    ui/renderer.h
)

target_compile_definitions(Chip8 PRIVATE
    $<$<CONFIG:Debug>:DEBUG_MACRO>
    $<$<CONFIG:Release>:RELEASE_MACRO>
)

if(CMAKE_BUILD_TYPE STREQUAL "Release") 
    set(EXE_NAME "Chip8")
else()
    set(EXE_NAME "Chip8_DEBUG")
endif()

set_target_properties(Chip8 PROPERTIES OUTPUT_NAME "${EXE_NAME}")

target_include_directories(Chip8 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(Chip8
    PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
)

add_custom_command(TARGET Chip8 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/dependencies"
            "$<TARGET_FILE_DIR:Chip8>"
)

add_custom_command(TARGET Chip8 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/roms"
            "$<TARGET_FILE_DIR:Chip8>/roms"
)
