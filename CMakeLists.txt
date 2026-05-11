cmake_minimum_required(VERSION 3.20)
project(Dangerous-Dave LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- SDL3 (static) ---
set(SDL_STATIC ON  CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST  OFF  CACHE BOOL "" FORCE)
add_subdirectory(lib/SDL)

# --- SDL3_image (static, stb_image backend only) ---
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SAMPLES     OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TESTS       OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_VENDORED    OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BACKEND_STB ON  CACHE BOOL "" FORCE)
add_subdirectory(lib/SDL_image)

# --- Box2D ---
set(BOX2D_SAMPLES   OFF CACHE BOOL "" FORCE)
set(BOX2D_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(BOX2D_DOCS      OFF CACHE BOOL "" FORCE)
set(BOX2D_PROFILE   OFF CACHE BOOL "" FORCE)
set(BOX2D_VALIDATE  OFF CACHE BOOL "" FORCE)
set(BOX2D_UNIT_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/box2d)

add_executable(Dangerous_Dave
    main.cpp
    Game.cpp
    Game.h
    bagel.h
)

target_include_directories(Dangerous_Dave PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}" DAVE_PROJECT_SOURCE_DIR)
target_compile_definitions(Dangerous_Dave PRIVATE
    DAVE_PROJECT_SOURCE_DIR="${DAVE_PROJECT_SOURCE_DIR}")

target_link_libraries(Dangerous_Dave PRIVATE
    SDL3-static
    SDL3_image-static
    box2d)

# Statically link the C++ runtime so the exe doesn't need libgcc_s_seh / libstdc++ / libwinpthread DLLs.
if (MINGW)
    target_link_options(Dangerous_Dave PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

# Copy res/ next to the executable so it can run in-place.
add_custom_command(TARGET Dangerous_Dave POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/res"
        "$<TARGET_FILE_DIR:Dangerous_Dave>/res"
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
