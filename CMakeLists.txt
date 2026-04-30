cmake_minimum_required(VERSION 3.14)
project(RobotControlUI VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    robotpreviewwidget.cpp
    robotpreviewwidget.h
    autocalibrationwidget.cpp
    autocalibrationwidget.h
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

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
set(ROBOT_SDK_CPP_ROOT "${ROBOT_SDK_ROOT}/cpp")

target_include_directories(${PROJECT_NAME} PRIVATE
    "${ROBOT_SDK_ROOT}"
    "${ROBOT_SDK_CPP_ROOT}"
    "${ROBOT_SDK_CPP_ROOT}/interface"
    "${ROBOT_SDK_CPP_ROOT}/parameter"
)

if(NOT EXISTS "${ROBOT_SDK_ROOT}")
    message(FATAL_ERROR "robot_sdk directory not found: ${ROBOT_SDK_ROOT}")
endif()

add_library(robot_sdk SHARED IMPORTED GLOBAL)

if(WIN32)
    if(NOT MINGW)
        message(FATAL_ERROR "The Windows SDK in robot_sdk/windows only provides MinGW import libraries. Please configure CMake with a MinGW toolchain.")
    endif()

    set(ROBOT_SDK_RELEASE_DIR "${ROBOT_SDK_ROOT}/windows/temp_win_mingw64_v2.0.4")
    set(ROBOT_SDK_DEBUG_DIR "${ROBOT_SDK_ROOT}/windows/temp_win_mingw64_debug_v2.0.4")

    set(ROBOT_SDK_RELEASE_IMPLIB "${ROBOT_SDK_RELEASE_DIR}/libnrc_host.dll.a")
    set(ROBOT_SDK_RELEASE_DLL "${ROBOT_SDK_RELEASE_DIR}/nrc_host.dll")
    set(ROBOT_SDK_DEBUG_IMPLIB "${ROBOT_SDK_DEBUG_DIR}/libnrc_host.dll.a")
    set(ROBOT_SDK_DEBUG_DLL "${ROBOT_SDK_DEBUG_DIR}/nrc_host.dll")

    foreach(required_file
            "${ROBOT_SDK_RELEASE_IMPLIB}"
            "${ROBOT_SDK_RELEASE_DLL}"
            "${ROBOT_SDK_DEBUG_IMPLIB}"
            "${ROBOT_SDK_DEBUG_DLL}")
        if(NOT EXISTS "${required_file}")
            message(FATAL_ERROR "Required Windows SDK file not found: ${required_file}")
        endif()
    endforeach()

    set_target_properties(robot_sdk PROPERTIES
        IMPORTED_CONFIGURATIONS "Debug;Release"
        IMPORTED_IMPLIB_DEBUG "${ROBOT_SDK_DEBUG_IMPLIB}"
        IMPORTED_LOCATION_DEBUG "${ROBOT_SDK_DEBUG_DLL}"
        IMPORTED_IMPLIB_RELEASE "${ROBOT_SDK_RELEASE_IMPLIB}"
        IMPORTED_LOCATION_RELEASE "${ROBOT_SDK_RELEASE_DLL}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        MAP_IMPORTED_CONFIG_MINSIZEREL Release
    )

    set(ROBOT_SDK_RUNTIME "$<IF:$<CONFIG:Debug>,${ROBOT_SDK_DEBUG_DLL},${ROBOT_SDK_RELEASE_DLL}>")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
        set(ROBOT_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/linux/arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64)$")
        if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
            set(ROBOT_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/linux/linux_gcc-4_x64_v2.0.4")
        else()
            set(ROBOT_SDK_LINUX_DIR "${ROBOT_SDK_ROOT}/linux/linux_gcc-9_x64_v2.0.4")
        endif()
    else()
        message(FATAL_ERROR "Unsupported Linux architecture for robot SDK: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    set(ROBOT_SDK_RUNTIME "${ROBOT_SDK_LINUX_DIR}/libnrc_host.so")

    if(NOT EXISTS "${ROBOT_SDK_RUNTIME}")
        message(FATAL_ERROR "Required Linux SDK file not found: ${ROBOT_SDK_RUNTIME}")
    endif()

    set_target_properties(robot_sdk PROPERTIES
        IMPORTED_LOCATION "${ROBOT_SDK_RUNTIME}"
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "\$ORIGIN"
    )
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE robot_sdk)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${ROBOT_SDK_RUNTIME}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>"
    VERBATIM
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/calib_scripts")
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/calib_scripts"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/calib_scripts"
        VERBATIM
    )
endif()
