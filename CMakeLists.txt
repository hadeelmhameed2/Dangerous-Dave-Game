cmake_minimum_required(VERSION 3.20)
project(Dangerous-Dave-2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(Dangerous_Dave
    main.cpp
    Game.cpp
    sdl_compat.cpp
    Game.h
    bagel.h
    sdl_compat.h
)

target_include_directories(Dangerous_Dave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DAVE_PROJECT_SOURCE_DIR)
target_compile_definitions(Dangerous_Dave PRIVATE DAVE_PROJECT_SOURCE_DIR="${DAVE_PROJECT_SOURCE_DIR}")

# Copy res/ and SDL DLLs next to the executable so it can run in-place.
add_custom_command(TARGET Dangerous_Dave POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:Dangerous_Dave>/res"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL3.dll"
        "$<TARGET_FILE_DIR:Dangerous_Dave>/SDL3.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/SDL3_image.dll"
        "$<TARGET_FILE_DIR:Dangerous_Dave>/SDL3_image.dll"
)

find_package(Doxygen QUIET)
if (DOXYGEN_FOUND)
    add_custom_target(docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating Doxygen HTML documentation"
        VERBATIM
    )
endif()
