# FetchQt.cmake - Fetch Qt6 if not found via system
function(fetch_qt6_if_needed)
    # Check if Qt6 is already found
    find_package(Qt6 6.5 COMPONENTS Core Widgets REQUIRED QUIET)
    if (Qt6_FOUND)
        return()
    endif()

    # Fetch Qt6 via FetchContent
    include(FetchContent)
    FetchContent_Declare(
        qt6
        GIT_REPOSITORY https://github.com/qt/qt5.git
        GIT_TAG 6.5.2
        SOURCE_DIR ${CMAKE_BINARY_DIR}/qt6
    )

    # Configure Qt with minimal components
    set(QT_QMAKE_EXECUTABLE "" CACHE FILEPATH "Qt qmake executable" FORCE)
    set(QT_CORE_LIB "" CACHE FILEPATH "Qt Core library" FORCE)
    set(QT_WIDGETS_LIB "" CACHE FILEPATH "Qt Widgets library" FORCE)

    # Fetch and configure Qt
    FetchContent_MakeAvailable(qt6)

    # Manually set Qt6 variables (simplified)
    set(Qt6_DIR "${CMAKE_BINARY_DIR}/qt6" CACHE PATH "Qt6 directory" FORCE)
    find_package(Qt6 6.5 COMPONENTS Core Widgets REQUIRED)
endfunction()
