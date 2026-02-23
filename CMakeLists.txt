cmake_minimum_required(VERSION 3.24)

set(SFML_2048_VERSION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")
if (NOT EXISTS "${SFML_2048_VERSION_FILE}")
    message(FATAL_ERROR "VERSION file not found: ${SFML_2048_VERSION_FILE}")
endif()

file(READ "${SFML_2048_VERSION_FILE}" SFML_2048_VERSION_RAW)
string(STRIP "${SFML_2048_VERSION_RAW}" SFML_2048_PROJECT_VERSION)

if (NOT SFML_2048_PROJECT_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "VERSION must follow SemVer format X.Y.Z, got: ${SFML_2048_PROJECT_VERSION}")
endif()

project(sfml_2048 VERSION ${SFML_2048_PROJECT_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SFML_2048_ENABLE_WARNINGS "Enable strict compiler warnings" ON)
option(SFML_2048_ENABLE_SANITIZERS "Enable AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
option(SFML_2048_ENABLE_COVERAGE "Enable coverage instrumentation (GCC/Clang)" OFF)

set(SFML_2048_USING_VCPKG OFF)
if (DEFINED CMAKE_TOOLCHAIN_FILE AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg")
    set(SFML_2048_USING_VCPKG ON)
endif()

function(enable_project_warnings target_name)
    if (NOT SFML_2048_ENABLE_WARNINGS)
        return()
    endif()

    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

function(enable_project_sanitizers target_name)
    if (NOT SFML_2048_ENABLE_SANITIZERS)
        return()
    endif()

    if (MSVC)
        message(WARNING "Sanitizers are not enabled for MSVC in this project.")
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(
            ${target_name}
            PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
        )
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

function(enable_project_coverage target_name)
    if (NOT SFML_2048_ENABLE_COVERAGE)
        return()
    endif()

    if (MSVC)
        message(WARNING "Coverage instrumentation is not enabled for MSVC in this project.")
        return()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
        target_link_options(${target_name} PRIVATE --coverage)
    endif()
endfunction()

find_package(SFML 2.5 COMPONENTS graphics window system audio CONFIG QUIET)
if (NOT SFML_FOUND)
    find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)
endif()

find_package(nlohmann_json CONFIG QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

if (SFML_2048_USING_VCPKG)
    find_package(fmt CONFIG REQUIRED)
else()
    find_package(fmt CONFIG QUIET)
endif()
find_package(OpenAL QUIET)

add_library(game_core
    src/core/Game.cpp
    src/core/ScoreManager.cpp
)

target_include_directories(game_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(game_core PUBLIC nlohmann_json::nlohmann_json)

enable_project_warnings(game_core)
enable_project_sanitizers(game_core)
enable_project_coverage(game_core)

add_executable(sfml_2048
    src/app/main.cpp
    src/app/AssetResolver.cpp
    src/app/SoundManager.cpp
    src/app/App.cpp
)

target_link_libraries(sfml_2048 PRIVATE game_core)

if (TARGET SFML::Graphics)
    target_link_libraries(sfml_2048 PRIVATE SFML::Graphics SFML::Window SFML::System SFML::Audio)
elseif (TARGET sfml-graphics)
    target_link_libraries(sfml_2048 PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)
else()
    target_link_libraries(sfml_2048 PRIVATE ${SFML_LIBRARIES})
endif()

if (TARGET OpenAL::OpenAL)
    target_link_libraries(sfml_2048 PRIVATE OpenAL::OpenAL)
elseif (OPENAL_LIBRARY)
    target_link_libraries(sfml_2048 PRIVATE ${OPENAL_LIBRARY})
endif()

if (APPLE)
    find_library(AUDIOTOOLBOX_FRAMEWORK AudioToolbox)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIOUNIT_FRAMEWORK AudioUnit)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(FOUNDATION_FRAMEWORK Foundation)

    foreach(_fw IN ITEMS AUDIOTOOLBOX_FRAMEWORK COREAUDIO_FRAMEWORK AUDIOUNIT_FRAMEWORK
                        COREFOUNDATION_FRAMEWORK FOUNDATION_FRAMEWORK)
        if (${_fw})
            target_link_libraries(sfml_2048 PRIVATE ${${_fw}})
        endif()
    endforeach()
endif()

if (TARGET fmt::fmt)
    target_link_libraries(sfml_2048 PRIVATE fmt::fmt)
elseif (TARGET fmt::fmt-header-only)
    target_link_libraries(sfml_2048 PRIVATE fmt::fmt-header-only)
endif()

enable_project_warnings(sfml_2048)
enable_project_sanitizers(sfml_2048)
enable_project_coverage(sfml_2048)

# Keep runtime assets available next to build outputs.
add_custom_command(TARGET sfml_2048 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:sfml_2048>/assets
)

include(GNUInstallDirs)

install(
    TARGETS sfml_2048
    RUNTIME_DEPENDENCY_SET sfml_2048_runtime_deps
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

install(
    TARGETS game_core
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(
    RUNTIME_DEPENDENCY_SET sfml_2048_runtime_deps
    DESTINATION .
    PRE_EXCLUDE_REGEXES "api-ms-" "ext-ms-"
    POST_EXCLUDE_REGEXES ".*/system32/.*\\.dll" ".*/usr/lib/.*" ".*/System/Library/.*"
)

install(DIRECTORY assets DESTINATION .)
install(FILES README.md LICENSE CHANGELOG.md VERSION DESTINATION .)

include(CTest)
if (BUILD_TESTING)
    find_package(Catch2 3 CONFIG QUIET)
    if (NOT Catch2_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
    endif()

    add_executable(core_unit_tests
        tests/core_unit_tests.cpp
    )

    target_link_libraries(core_unit_tests PRIVATE game_core Catch2::Catch2WithMain)
    enable_project_warnings(core_unit_tests)
    enable_project_sanitizers(core_unit_tests)
    enable_project_coverage(core_unit_tests)

    add_test(
        NAME core_unit_tests
        COMMAND core_unit_tests
    )
endif()

set(CPACK_PACKAGE_NAME "sfml_2048")
set(CPACK_PACKAGE_VENDOR "2048 Project Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "2048 desktop game in C++ with SFML")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_GENERATOR "ZIP")
string(TOLOWER "${CMAKE_SYSTEM_NAME}" SFML_2048_SYSTEM_NAME)
set(
    CPACK_PACKAGE_FILE_NAME
    "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${SFML_2048_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)
set(CPACK_SOURCE_IGNORE_FILES "/build/;/\\.git/;\\.DS_Store")

include(CPack)
