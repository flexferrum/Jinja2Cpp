cmake_minimum_required(VERSION 3.0.1)
project(Jinja2Cpp VERSION 1.1.0)

if (${CMAKE_VERSION} VERSION_GREATER "3.12")
    cmake_policy(SET CMP0074 OLD)
endif ()

if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    set(JINJA2CPP_IS_MAIN_PROJECT TRUE)
else()
    set(JINJA2CPP_IS_MAIN_PROJECT FALSE)
endif()

# Options
set(JINJA2CPP_SANITIZERS address+undefined memory)
set(JINJA2CPP_WITH_SANITIZERS none CACHE STRING "Build with sanitizer")
set_property(CACHE JINJA2CPP_WITH_SANITIZERS PROPERTY STRINGS ${JINJA2CPP_SANITIZERS})
set (JINJA2CPP_DEPS_MODE "internal" CACHE STRING "Jinja2Cpp dependency management mode (internal | external | external-boost | conan-build). See documentation for details. 'interal' is default.")
option(JINJA2CPP_BUILD_TESTS "Build Jinja2Cpp unit tests" ${JINJA2CPP_IS_MAIN_PROJECT})
option(JINJA2CPP_STRICT_WARNINGS "Enable additional warnings and treat them as errors" ON)
option(JINJA2CPP_BUILD_SHARED "Build shared linkage version of Jinja2Cpp" OFF)
option(JINJA2CPP_PIC "Control -fPIC option for library build" OFF)
option(JINJA2CPP_VERBOSE "Add extra debug output to the build scripts" OFF)

if (DEFINED BUILD_SHARED_LIBS)
    set(JINJA2CPP_BUILD_SHARED ${BUILD_SHARED_LIBS})
endif ()

if (JINJA2CPP_BUILD_SHARED)
    set(JINJA2CPP_PIC ON)
    set(JINJA2CPP_MSVC_RUNTIME_TYPE "/MD")
endif ()

if (NOT JINJA2CPP_DEPS_MODE)
    set(JINJA2CPP_DEPS_MODE "internal")
endif ()

if (JINJA2CPP_IS_MAIN_PROJECT OR NOT CMAKE_CXX_STANDARD)
    set(JINJA2CPP_CXX_STANDARD 14 CACHE STRING "Jinja2Cpp C++ standard to build with. C++14 is default")
    set(CMAKE_CXX_STANDARD ${JINJA2CPP_CXX_STANDARD})
endif ()

if (NOT JINJA2CPP_CXX_STANDARD)
    set (JINJA2CPP_CXX_STANDARD ${CMAKE_CXX_STANDARD})
endif ()

if (JINJA2CPP_CXX_STANDARD LESS 14)
    message(FATAL_ERROR "Jinja2Cpp is required C++14 or greater standard set. Currently selected standard: ${JINJA2CPP_CXX_STANDARD}")
else ()
    message(STATUS "Jinja2Cpp C++ standard: ${JINJA2CPP_CXX_STANDARD}")
endif ()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
set(CLANG_CXX_FLAGS)
set(GCC_CXX_FLAGS)
set(MSVC_CXX_FLAGS)

if (JINJA2CPP_WITH_COVERAGE)
    message(STATUS "This is DEBUG build with enabled Code Coverage")
    set(CMAKE_BUILD_TYPE Debug)
    set(JINJA2CPP_COVERAGE_TARGET "jinja2cpp_build_coverage")
    include(coverage)
    add_coverage_target("${JINJA2CPP_COVERAGE_TARGET}")
endif ()

if(NOT ${JINJA2CPP_WITH_SANITIZERS} STREQUAL "none")
    message (STATUS "Build with sanitizers enabled: ${JINJA2CPP_WITH_SANITIZERS}")
    set(_chosen_san)
    list(FIND JINJA2CPP_SANITIZERS ${JINJA2CPP_WITH_SANITIZERS} _chosen_san)
    if (${_chosen_san} EQUAL -1)
        message(FATAL_ERROR "Wrong sanitizer type has been chosen, must be one of ${JINJA2CPP_SANITIZERS}")
    endif()

    include("sanitizer.${JINJA2CPP_WITH_SANITIZERS}")
    set (JINJA2CPP_SANITIZE_TARGET "jinja2cpp_build_sanitizers")
    add_sanitizer_target(${JINJA2CPP_SANITIZE_TARGET})
endif()

if (UNIX)
    if (JINJA2CPP_PIC OR CONAN_CMAKE_POSITION_INDEPENDENT_CODE)
        add_compile_options(-fPIC)
    endif ()

    if (DEFINED CONAN_SHARED_LINKER_FLAGS)
        set(GCC_CXX_FLAGS ${GCC_CXX_FLAGS} -Wl,${CONAN_SHARED_LINKER_FLAGS})
        set(CLANG_CXX_FLAGS ${CLANG_CXX_FLAGS} -Wl,${CONAN_SHARED_LINKER_FLAGS})
    endif ()
else ()
    set(GCC_CXX_FLAGS ${GCC_CXX_FLAGS} "-Wa,-mbig-obj" -O1)
    if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
        add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    endif ()
    add_definitions(-DBOOST_ALL_NO_LIB)
    set(MSVC_CXX_FLAGS ${MSVC_CXX_FLAGS} /wd4503 /bigobj)

    if (CMAKE_BUILD_TYPE MATCHES "Debug" AND JINJA2CPP_MSVC_RUNTIME_TYPE)
        set(JINJA2CPP_MSVC_RUNTIME_TYPE "${JINJA2CPP_MSVC_RUNTIME_TYPE}d")
    endif ()
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        if (JINJA2CPP_DEPS_MODE MATCHES "conan-build" OR CMAKE_BUILD_TYPE STREQUAL "")
            if (NOT JINJA2CPP_MSVC_RUNTIME_TYPE STREQUAL "")
                set(MSVC_RUNTIME_DEBUG ${JINJA2CPP_MSVC_RUNTIME_TYPE})
                set(MSVC_RUNTIME_RELEASE ${JINJA2CPP_MSVC_RUNTIME_TYPE})
            endif ()
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${MSVC_RUNTIME_DEBUG}")
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${MSVC_RUNTIME_RELEASE}")
            set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${MSVC_RUNTIME_RELEASE}")
            set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/PROFILE")
            set(Boost_USE_DEBUG_RUNTIME OFF)
        elseif (CMAKE_BUILD_TYPE MATCHES "Debug")
            set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} ${JINJA2CPP_MSVC_RUNTIME_TYPE}")
            set(Boost_USE_DEBUG_RUNTIME ON)
        else ()
            set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${JINJA2CPP_MSVC_RUNTIME_TYPE}")
            set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} ${JINJA2CPP_MSVC_RUNTIME_TYPE}")
            set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/PROFILE")
            set(Boost_USE_DEBUG_RUNTIME OFF)
        endif ()
        message(STATUS "Selected MSVC runtime type for Jinja2C++ library: '${JINJA2CPP_MSVC_RUNTIME_TYPE}'")
    endif ()
endif()


if (JINJA2CPP_BUILD_SHARED)
    set(LIB_LINK_TYPE SHARED)
    message(STATUS "Jinja2C++ library type: SHARED")
else()
    set(LIB_LINK_TYPE STATIC)
    message(STATUS "Jinja2C++ library type: STATIC")
endif()

include(collect_sources)

set (LIB_TARGET_NAME jinja2cpp)

CollectSources(Sources Headers ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/src)
CollectSources(PublicSources PublicHeaders ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${LIB_TARGET_NAME} ${LIB_LINK_TYPE}
    ${Sources}
    ${Headers}
    ${PublicHeaders}
)

string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_CFG_NAME)
set(CURRENT_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${BUILD_CFG_NAME}}")

set(JINJA2CPP_EXTRA_LIBS "" CACHE STRING "You can pass some libs that could used during link stage")
set(JINJA2CPP_PUBLIC_LIBS "${JINJA2CPP_EXTRA_LIBS}")
separate_arguments(JINJA2CPP_PUBLIC_LIBS)
if (JINJA2CPP_WITH_COVERAGE)
    target_compile_options(
            ${JINJA2CPP_COVERAGE_TARGET}
            INTERFACE
            -g -O0
    )
    list(APPEND JINJA2CPP_PUBLIC_LIBS ${JINJA2CPP_COVERAGE_TARGET})
endif()
if (NOT JINJA2CPP_WITH_SANITIZERS STREQUAL "none")
    target_compile_options(
            ${JINJA2CPP_SANITIZE_TARGET}
            INTERFACE
            -g -O2
    )
    list(APPEND JINJA2CPP_PUBLIC_LIBS ${JINJA2CPP_SANITIZE_TARGET})
endif()

set(JINJA2CPP_PRIVATE_LIBS "${JINJA2CPP_PRIVATE_LIBS}")
include(thirdparty/CMakeLists.txt)

target_link_libraries(
        ${LIB_TARGET_NAME}
    PUBLIC
        ${JINJA2CPP_PUBLIC_LIBS}
    PRIVATE
        ${JINJA2CPP_PRIVATE_LIBS}
)

target_include_directories(${LIB_TARGET_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )

if (JINJA2CPP_STRICT_WARNINGS)
    if (UNIX)
        set(GCC_CXX_FLAGS ${GCC_CXX_FLAGS} -Wall -Werror)
        set(CLANG_CXX_FLAGS ${CLANG_CXX_FLAGS} -Wall -Werror -Wno-unused-command-line-argument)
        set(MSVC_CXX_FLAGS ${MSVC_CXX_FLAGS} /W4)
    endif ()
endif ()

if (${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
    target_compile_options(${LIB_TARGET_NAME} PRIVATE ${GCC_CXX_FLAGS})
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    target_compile_options(${LIB_TARGET_NAME} PRIVATE ${CLANG_CXX_FLAGS})
elseif (${CMAKE_CXX_COMPILER_ID} MATCHES "MSVC")
    target_compile_options(${LIB_TARGET_NAME} PRIVATE ${MSVC_CXX_FLAGS})
endif ()

target_compile_definitions(${LIB_TARGET_NAME} PUBLIC -DBOOST_SYSTEM_NO_DEPRECATED -DBOOST_ERROR_CODE_HEADER_ONLY)

if (JINJA2CPP_BUILD_SHARED)
    target_compile_definitions(${LIB_TARGET_NAME} PRIVATE -DJINJA2CPP_BUILD_AS_SHARED PUBLIC -DJINJA2CPP_LINK_AS_SHARED)
endif ()

set_target_properties(${LIB_TARGET_NAME} PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION 1
        )

set_target_properties(${LIB_TARGET_NAME} PROPERTIES
        CXX_STANDARD ${JINJA2CPP_CXX_STANDARD}
        CXX_STANDARD_REQUIRED ON
        )

set_property(TARGET ${LIB_TARGET_NAME} PROPERTY PUBLIC_HEADER ${PublicHeaders} ${JINJA2CPP_EXTRA_PUBLIC_HEADERS})

configure_file(jinja2cpp.pc.in jinja2cpp.pc @ONLY)

if (JINJA2CPP_BUILD_TESTS)
    enable_testing()

    CollectSources(TestSources TestHeaders ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/test)
    add_executable(jinja2cpp_tests ${TestSources} ${TestHeaders})
    target_link_libraries(jinja2cpp_tests gtest gtest_main nlohmann_json ${LIB_TARGET_NAME} ${EXTRA_TEST_LIBS} ${JINJA2CPP_PRIVATE_LIBS})

    set_target_properties(jinja2cpp_tests PROPERTIES
            CXX_STANDARD ${JINJA2CPP_CXX_STANDARD}
            CXX_STANDARD_REQUIRED ON)

    if (MSVC)
        target_compile_options(jinja2cpp_tests PRIVATE /bigobj)
    endif ()

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/test_data/simple_template1.j2tpl
            COMMAND ${CMAKE_COMMAND} ARGS -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/test/test_data ${CMAKE_CURRENT_BINARY_DIR}/test_data
            MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test/test_data/simple_template1.j2tpl
            COMMENT "Copy test data to the destination dir"
    )

    add_custom_target(CopyTestData ALL
            DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/test_data/simple_template1.j2tpl
    )

    add_dependencies(jinja2cpp_tests CopyTestData)

    add_test(NAME jinja2cpp_tests COMMAND jinja2cpp_tests)
endif ()

set (JINJA2CPP_INSTALL_CONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/${LIB_TARGET_NAME}")
set (JINJA2CPP_TMP_CONFIG_PATH "cmake/config")


macro (Jinja2CppGetTargetIncludeDir infix target)
    message (STATUS "infix: ${infix} target: ${target}")

    if (TARGET ${target})
        set (_J2CPP_VAR_NAME JINJA2CPP_${infix}_INCLUDE_DIRECTORIES)
        get_target_property(${_J2CPP_VAR_NAME} ${target} INTERFACE_INCLUDE_DIRECTORIES)
    endif ()
endmacro ()

Jinja2CppGetTargetIncludeDir(EXPECTED-LITE expected-lite)
Jinja2CppGetTargetIncludeDir(VARIANT-LITE variant-lite)
Jinja2CppGetTargetIncludeDir(OPTIONAL-LITE optional-lite)
Jinja2CppGetTargetIncludeDir(STRING-VIEW-LITE string-view-lite)

# Workaround for #14444 bug of CMake (https://gitlab.kitware.com/cmake/cmake/issues/14444)
# We can't use EXPORT feature of 'install' as is due to limitation of subproject's targets installation
# So jinja2cpp-config.cmake should be written manually

install(
    TARGETS
        ${LIB_TARGET_NAME}
    EXPORT
        InstallTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jinja2cpp
)

install(
    FILES
        ${CMAKE_BINARY_DIR}/jinja2cpp.pc
    DESTINATION
        ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig
)

install(
    EXPORT
        InstallTargets
    FILE
        jinja2cpp-cfg.cmake
    DESTINATION
        ${JINJA2CPP_INSTALL_CONFIG_DIR}
)

configure_package_config_file(
    cmake/public/jinja2cpp-config.cmake.in
    ${JINJA2CPP_TMP_CONFIG_PATH}/jinja2cpp-config.cmake
    INSTALL_DESTINATION ${JINJA2CPP_TMP_CONFIG_PATH}
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

configure_package_config_file(
    cmake/public/jinja2cpp-config-deps-${JINJA2CPP_DEPS_MODE}.cmake.in
    ${JINJA2CPP_TMP_CONFIG_PATH}/jinja2cpp-config-deps.cmake
    INSTALL_DESTINATION ${JINJA2CPP_TMP_CONFIG_PATH}
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/${JINJA2CPP_TMP_CONFIG_PATH}/${LIB_TARGET_NAME}-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/${JINJA2CPP_TMP_CONFIG_PATH}/${LIB_TARGET_NAME}-config-deps.cmake
    DESTINATION
        ${JINJA2CPP_INSTALL_CONFIG_DIR}
)
