cmake_minimum_required(VERSION 3.16...3.30)

# ── Project ────────────────────────────────────────────────────────────────────
project(midcircuitcdn-obs-plugin VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Plugin Metadata ────────────────────────────────────────────────────────────
set(PLUGIN_AUTHOR "MidcircuitCDN")
set(PLUGIN_WEBSITE "https://midcircuitcdn.com")

# ── OBS SDK (pre-extracted) ────────────────────────────────────────────────────
# The OBS SDK headers are pre-extracted into deps/obs-sdk/.
# This avoids FetchContent's file RENAME issues on NTFS/WSL mounts.
# To update the SDK version, re-download and extract into deps/obs-sdk/:
#   curl -sL https://github.com/obsproject/obs-studio/archive/refs/tags/<VERSION>.tar.gz | tar -xz
#   mv obs-studio-<VERSION> deps/obs-sdk

set(OBS_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/deps/obs-sdk")

if(NOT EXISTS "${OBS_SDK_DIR}/libobs/obs-module.h")
    message(FATAL_ERROR
        "OBS SDK not found at ${OBS_SDK_DIR}.\n"
        "Run: curl -sL https://github.com/obsproject/obs-studio/archive/refs/tags/30.2.3.tar.gz | tar -xz -C deps/ && mv deps/obs-studio-30.2.3 deps/obs-sdk"
    )
endif()

# Point to the OBS header directories
set(OBS_INCLUDE_DIRS
    "${OBS_SDK_DIR}/libobs"
    "${OBS_SDK_DIR}/UI/obs-frontend-api"
)


# ── Qt (for UI dialogs & native dock panels) ──────────────────────────────────
# OBS 30+ uses Qt6. When Qt is found, we enable native dock widgets.
find_package(Qt6 COMPONENTS Core Widgets Network QUIET)
if(Qt6_FOUND)
    set(QT_LIBS Qt6::Core Qt6::Widgets Qt6::Network)
    set(HAVE_QT ON)
    message(STATUS "Found Qt6 — native dock panels enabled")
else()
    find_package(Qt5 5.15 COMPONENTS Core Widgets Network QUIET)
    if(Qt5_FOUND)
        set(QT_LIBS Qt5::Core Qt5::Widgets Qt5::Network)
        set(HAVE_QT ON)
        message(STATUS "Found Qt5 — native dock panels enabled")
    else()
        message(STATUS "Qt not found — native dock panels disabled, using Tools menu fallback")
        set(QT_LIBS "")
        set(HAVE_QT OFF)
    endif()
endif()

if(HAVE_QT)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)
endif()

# ── Source Files ──────────────────────────────────────────────────────────────
set(PLUGIN_SOURCES
    src/plugin-main.cpp
    src/auth/plugin_oauth.cpp
    src/auth/plugin_store.cpp
    src/ui/settings_dialog.cpp
    src/ui/dashboard_dock.cpp
    src/obs_integration/stream_config.cpp
    src/obs_integration/encoder_config.cpp
    src/multistream/multistream_store.cpp
    src/multistream/multistream_output.cpp
)

# Add Qt-dependent sources only when Qt is available
if(HAVE_QT)
    list(APPEND PLUGIN_SOURCES
        src/ui/control_dock.cpp
        src/ui/multistream_dialog.cpp
        src/update_checker.cpp
    )
endif()

# ── Build Target ──────────────────────────────────────────────────────────────
# MODULE = shared lib loaded at runtime by OBS (no implicit link to libobs)
add_library(${PROJECT_NAME} MODULE ${PLUGIN_SOURCES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${OBS_INCLUDE_DIRS}
)

# Define HAVE_QT so source files can conditionally compile Qt code
if(HAVE_QT)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_QT)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${QT_LIBS})
endif()

# ── Windows Linking ───────────────────────────────────────────────────────────
if(WIN32)
    set(STUBS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stubs")

    if(MSVC)
        # ── MSVC Native Build ─────────────────────────────────────────────
        # Link against MSVC import libraries generated from .def files.
        # Run build_win.bat to generate these before building.
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${STUBS_DIR}/obs.lib"
            "${STUBS_DIR}/obs-frontend-api.lib"
            ws2_32    # Winsock2 (for OAuth loopback server)
            shell32   # ShellExecuteA (for opening browser)
        )
    else()
        # ── MinGW Cross-Compilation ───────────────────────────────────────
        # Link against MinGW .a import libraries generated from .def files
        # via dlltool. Used for WSL cross-compilation.
        target_link_libraries(${PROJECT_NAME} PRIVATE
            "${STUBS_DIR}/libobs.a"
            "${STUBS_DIR}/libobs-frontend-api.a"
            ws2_32
            shell32
        )
        # Statically link MinGW runtimes so the user doesn't need extra DLLs
        target_link_options(${PROJECT_NAME} PRIVATE
            "-static-libgcc"
            "-static-libstdc++"
        )
    endif()
endif()


# Suppress the "lib" prefix on Linux (OBS expects just the bare name)
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")

# ── Install Rules ─────────────────────────────────────────────────────────────
if(WIN32)
    set(OBS_PLUGIN_DEST "obs-plugins/64bit")
else()
    set(OBS_PLUGIN_DEST "lib/obs-plugins")
endif()

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${OBS_PLUGIN_DEST})
install(DIRECTORY data/ DESTINATION "data/obs-plugins/${PROJECT_NAME}")
