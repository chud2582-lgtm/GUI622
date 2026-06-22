cmake_minimum_required(VERSION 3.14)
project(RobotControlUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    workpieceselectionwidget.cpp
    workpieceselectionwidget.h
    robot_sdk/cpp/robot_adapter.cpp
    robot_sdk/cpp/robot_arm_actions.cpp
    robot_sdk/cpp/huichuan_arm_actions.cpp
    robot_sdk/cpp/yinglian_arm_actions.cpp
    robot_sdk/cpp/huichuan_backend.cpp
    robot_sdk/cpp/yinglian_backend.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::Network)

# Force UTF-8 source decoding so Chinese QStringLiteral text is rendered correctly
# in the UI and log area after rebuilding.
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
endif()

set(ROBOT_SDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/robot_sdk")

target_include_directories(${PROJECT_NAME} PRIVATE
    "${ROBOT_SDK_ROOT}"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

if(NOT EXISTS "${ROBOT_SDK_ROOT}")
    message(FATAL_ERROR "robot_sdk directory not found: ${ROBOT_SDK_ROOT}")
endif()

if(WIN32)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The bundled robot SDKs only support 64-bit Windows builds.")
    endif()

    set(HUICHUAN_SDK_WIN_DIR "${ROBOT_SDK_ROOT}/huichuan/x64")
    set(HUICHUAN_SDK_IMPORT_LIB "${HUICHUAN_SDK_WIN_DIR}/IMC100API.lib")
    set(HUICHUAN_SDK_RUNTIME "${HUICHUAN_SDK_WIN_DIR}/IMC100API.dll")
    if(NOT EXISTS "${HUICHUAN_SDK_IMPORT_LIB}")
        message(FATAL_ERROR "Required Huichuan SDK import library not found: ${HUICHUAN_SDK_IMPORT_LIB}")
    endif()
    if(NOT EXISTS "${HUICHUAN_SDK_RUNTIME}")
        message(FATAL_ERROR "Required Huichuan SDK file not found: ${HUICHUAN_SDK_RUNTIME}")
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(YINGLIAN_SDK_WIN_DIR "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_debug_v2.0.4")
    else()
        set(YINGLIAN_SDK_WIN_DIR "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_v2.0.4")
    endif()
    set(YINGLIAN_SDK_RUNTIME "${YINGLIAN_SDK_WIN_DIR}/nrc_host.dll")
    set(YINGLIAN_SDK_IMPORT_LIB "${YINGLIAN_SDK_WIN_DIR}/libnrc_host.dll.a")
    set(YINGLIAN_SDK_RUNTIME_RELEASE "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_v2.0.4/nrc_host.dll")
    set(YINGLIAN_SDK_RUNTIME_DEBUG "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_debug_v2.0.4/nrc_host.dll")
    set(YINGLIAN_SDK_IMPORT_LIB_RELEASE "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_v2.0.4/libnrc_host.dll.a")
    set(YINGLIAN_SDK_IMPORT_LIB_DEBUG "${ROBOT_SDK_ROOT}/yinglian/windows/temp_win_mingw64_debug_v2.0.4/libnrc_host.dll.a")
    if(NOT EXISTS "${YINGLIAN_SDK_RUNTIME}")
        message(FATAL_ERROR "Required Yinglian SDK file not found: ${YINGLIAN_SDK_RUNTIME}")
    endif()
    if(NOT EXISTS "${YINGLIAN_SDK_IMPORT_LIB}")
        message(FATAL_ERROR "Required Yinglian SDK import library not found: ${YINGLIAN_SDK_IMPORT_LIB}")
    endif()

    target_link_libraries(${PROJECT_NAME} PRIVATE "${HUICHUAN_SDK_IMPORT_LIB}" "${YINGLIAN_SDK_IMPORT_LIB}")
else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(YINGLIAN_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/yinglian/linux/arm64")
    elseif(CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 9)
        set(YINGLIAN_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/yinglian/linux/linux_gcc-9_x64_v2.0.4")
    else()
        set(YINGLIAN_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/yinglian/linux/linux_gcc-4_x64_v2.0.4")
    endif()

    set(YINGLIAN_SDK_RUNTIME "${YINGLIAN_SDK_LINUX_DIR}/libnrc_host.so")
    if(NOT EXISTS "${YINGLIAN_SDK_RUNTIME}")
        message(FATAL_ERROR "Required Yinglian Linux SDK file not found: ${YINGLIAN_SDK_RUNTIME}")
    endif()
    if(UNIX)
        target_link_libraries(${PROJECT_NAME} PRIVATE "${YINGLIAN_SDK_RUNTIME}")
    endif()
endif()

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${HUICHUAN_SDK_RUNTIME}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/IMC100API.dll"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/windows/temp_win_mingw64_v2.0.4"
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/windows/temp_win_mingw64_debug_v2.0.4"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${YINGLIAN_SDK_RUNTIME_RELEASE}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/windows/temp_win_mingw64_v2.0.4/nrc_host.dll"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${YINGLIAN_SDK_RUNTIME_DEBUG}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/windows/temp_win_mingw64_debug_v2.0.4/nrc_host.dll"
        VERBATIM
    )
else()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/linux/linux_gcc-9_x64_v2.0.4"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${YINGLIAN_SDK_RUNTIME}"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_sdk/yinglian/linux/linux_gcc-9_x64_v2.0.4/libnrc_host.so"
        VERBATIM
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/robot_connection_huichuan.ini")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/robot_connection_huichuan.ini"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_connection_huichuan.ini"
        VERBATIM
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/robot_connection_yinglian.ini")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/robot_connection_yinglian.ini"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/robot_connection_yinglian.ini"
        VERBATIM
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/model")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/model"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/model"
        VERBATIM
    )
endif()
