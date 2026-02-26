add_library(tenbox_ipc STATIC
    ${CMAKE_SOURCE_DIR}/src/ipc/protocol_v1.cpp
)

target_include_directories(tenbox_ipc
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
)
