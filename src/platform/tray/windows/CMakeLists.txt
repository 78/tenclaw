add_library(tenbox_tray_windows STATIC
    ${CMAKE_SOURCE_DIR}/src/platform/tray/windows/tray_app.cpp
)

target_include_directories(tenbox_tray_windows
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)
