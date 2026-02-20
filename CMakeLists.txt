cmake_minimum_required(VERSION 3.20)
project(NotationalVelocity VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(FetchQt)

# Fetch Qt if needed
fetch_qt6_if_needed()

# Find required Qt packages
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Concurrent LinguistTools Network)

# Add resources
qt_add_resources(RESOURCES resources/nv.qrc)

# Add translations
#set(TS_FILES
##    resources/translations/nv_en.ts
#    resources/translations/nv_de.ts
#)
#qt_add_translations(TR_FILES ${TS_FILES})

# Core library
add_library(nv_core STATIC
    src/core/include/nv/note_model.h
    src/core/src/note_model.cpp
    src/core/include/nv/search_index.h
    src/core/src/search_index.cpp
    src/core/include/nv/note_store.h
    src/core/src/note_store.cpp
    src/core/include/nv/storage.h
    src/core/src/storage.cpp
    src/core/include/nv/app_state.h
    src/core/src/app_state.cpp
    src/core/include/nv/result.h
    src/core/include/nv/interfaces.h
)
target_include_directories(nv_core PUBLIC src/core/include)
target_link_libraries(nv_core PUBLIC Qt6::Core Qt6::Network)

# UI library
qt_wrap_cpp(nv_ui_ui_moc
    src/ui/include/nv/search_field.h
    src/ui/include/nv/note_list.h
    src/ui/include/nv/note_editor.h
    src/ui/include/nv/main_window.h
    src/ui/include/nv/application_controller.h
    src/ui/include/nv/webdav_config_dialog.h
    src/ui/include/nv/checkbox_widget.h
    src/core/include/nv/webdav_sync_manager.h
)

add_library(nv_ui STATIC
    ${nv_ui_ui_moc}
    src/ui/src/search_field.cpp
    src/ui/src/note_list.cpp
    src/ui/src/note_editor.cpp
    src/ui/src/checkbox_widget.cpp
    src/ui/src/main_window.cpp
    src/ui/src/application_controller.cpp
    src/ui/src/webdav_config_dialog.cpp
    src/core/src/webdav_sync_manager.cpp
)

target_include_directories(nv_ui PUBLIC src/ui/include src/core/include)
target_link_libraries(nv_ui PUBLIC nv_core Qt6::Widgets Qt6::Concurrent Qt6::Network)

# Platform-specific menu bars
if(APPLE)
    set(MACOS_SOURCES
        src/app/platform/macos/MenuBar.mm
        src/app/platform/macos/Dock.mm
    )
elseif(WIN32)
    set(WINDOWS_SOURCES
        src/app/platform/windows/MenuBar.cpp
        src/app/platform/windows/Taskbar.cpp
    )
elseif(UNIX)
    set(LINUX_SOURCES
        src/app/platform/linux/MenuBar.cpp
    )
endif()

# Application executable
add_executable(nv
    src/app/main.cpp
    ${MACOS_SOURCES}
    ${WINDOWS_SOURCES}
    ${LINUX_SOURCES}
    ${RESOURCES}
    ${TR_FILES}
)

target_link_libraries(nv PRIVATE nv_ui)

# Set macOS bundle
if(APPLE)
    set_target_properties(nv PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/MacOSBundleInfo.plist.in"
    )
endif()

# Install target
if(NV_INSTALL)
    install(TARGETS nv
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )
endif()
