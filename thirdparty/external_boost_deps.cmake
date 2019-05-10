find_package(boost_filesystem QUIET)
find_package(boost_algorithm  QUIET)
find_package(boost_variant    QUIET)
find_package(boost_optional   QUIET)

if(boost_filesystem_FOUND AND
   boost_algorithm_FOUND  AND
   boost_variant_FOUND    AND
   boost_optional_FOUND)
   imported_target_alias(boost_filesystem ALIAS boost_filesystem::boost_filesystem)
   imported_target_alias(boost_algorithm  ALIAS boost_algorithm::boost_algorithm)
   imported_target_alias(boost_variant    ALIAS boost_variant::boost_variant)
   imported_target_alias(boost_optional   ALIAS boost_optional::boost_optional)
   
   
else()
    if (MSVC)
        if (NOT DEFINED Boost_USE_STATIC_LIBS)
            if (THIRDPARTY_RUNTIME_TYPE STREQUAL "/MD" OR THIRDPARTY_RUNTIME_TYPE STREQUAL "/MDd")
                set (Boost_USE_STATIC_LIBS OFF)
                set (Boost_USE_STATIC_RUNTIME OFF)
            else ()
                set (Boost_USE_STATIC_LIBS ON)
                set (Boost_USE_STATIC_RUNTIME ON)
            endif ()
        endif ()
    endif ()

    find_package(Boost COMPONENTS system filesystem QUIET REQUIRED)

    if (Boost_FOUND)
        imported_target_alias(boost_filesystem ALIAS Boost::filesystem)
        imported_target_alias(boost_algorithm  ALIAS Boost::boost)
        imported_target_alias(boost_variant    ALIAS Boost::boost)
        imported_target_alias(boost_optional   ALIAS Boost::boost)
    endif ()
endif ()

install(TARGETS boost_filesystem boost_algorithm boost_variant boost_optional
        EXPORT InstallTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}/static
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/boost
        )