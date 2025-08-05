cmake_minimum_required(VERSION 3.12)

set(PATCH_VERSION "1" CACHE INTERNAL "Patch version")
set(PROJECT_VERSION 0.0.${PATCH_VERSION})

project(bulk_server VERSION ${PROJECT_VERSION})

option(WITH_BOOST_TEST "Whether to build Boost test" ON)
option(WITH_GTEST "Whether to build Google test" ON)

configure_file(version.h.in version.h)

add_definitions(-D USE_PRETTY)

add_executable(async main_async.cpp)
add_executable(bulk_server main_bulk_server.cpp bulkserver_utils.cpp)
add_library(libbulk SHARED vers.cpp bulk.cpp bulk_utils.cpp)
add_library(libasync SHARED async.cpp async_internal.cpp async_utils.cpp)

#target_compile_definitions(async PUBLIC -DUSE_DBG_TRACE)
#target_compile_definitions(bulk_server PUBLIC -DUSE_DBG_TRACE)
#target_compile_definitions(libbulk PUBLIC -DUSE_DBG_TRACE)
#target_compile_definitions(libasync PUBLIC -DUSE_DBG_TRACE)


set_target_properties(async PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(bulk_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(libbulk PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

set_target_properties(libasync PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

find_package(Boost REQUIRED COMPONENTS program_options system)
if( Boost_FOUND )
    message(status "** Boost Include: ${Boost_INCLUDE_DIR}")
    message(status "** Boost Libraries: ${Boost_LIBRARY_DIRS}")
    message(status "** Boost Libraries: ${Boost_LIBRARIES}")

    set_target_properties(libbulk PROPERTIES
        COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

    set_target_properties(libasync PROPERTIES
        COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

    set_target_properties(async PROPERTIES
        COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

    set_target_properties(bulk_server PROPERTIES
        COMPILE_DEFINITIONS BOOST_ALL_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

    target_link_libraries(libbulk PRIVATE
        ${Boost_LIBRARIES}
    )

    target_link_libraries(libasync PRIVATE
        ${Boost_LIBRARIES}
    )

    target_link_libraries(async PRIVATE
        ${Boost_LIBRARIES}
    )

    target_link_libraries(bulk_server PRIVATE
        ${Boost_LIBRARIES}
    )
endif()

target_include_directories(libbulk
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_include_directories(async
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_include_directories(bulk_server
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_include_directories(libasync
    PRIVATE "${CMAKE_BINARY_DIR}"
)

target_link_libraries(async PRIVATE
    $<$<CONFIG:Debug>:asan>
    libbulk
    libasync    
)

target_link_libraries(bulk_server PRIVATE
    $<$<CONFIG:Debug>:asan>
    libbulk
    libasync    
)

target_link_libraries(libbulk PRIVATE
    $<$<CONFIG:Debug>:asan>
)

target_link_libraries(libasync PRIVATE
    $<$<CONFIG:Debug>:asan>
    libbulk
)

if(WITH_BOOST_TEST)
    #if(WIN32)
        set (Boost_ROOT "C:/local/boost_1_87_0/") # Путь к библиотеке Boost
    #endif()

    find_package(Boost COMPONENTS unit_test_framework REQUIRED)
    add_executable(test_version test_version.cpp)

    set_target_properties(test_version PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    set_target_properties(test_version PROPERTIES
        COMPILE_DEFINITIONS BOOST_TEST_DYN_LINK
        INCLUDE_DIRECTORIES ${Boost_INCLUDE_DIR}
    )

    target_link_libraries(test_version
        $<$<CONFIG:Debug>:asan>
        ${Boost_LIBRARIES}
        libbulk
    )
endif()

if(WITH_GTEST)
    find_package(GTest  REQUIRED)
    add_executable(test_versiong test_versiong.cpp)
    add_executable(test_bulk test_bulk.cpp)
    add_executable(test_async test_async.cpp)

    target_compile_definitions(test_bulk PUBLIC -DUSE_DBG_TRACE)
    target_compile_definitions(test_async PUBLIC -DUSE_DBG_TRACE)

    set_target_properties(test_versiong PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    set_target_properties(test_bulk PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    set_target_properties(test_async PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )

    target_link_libraries(test_versiong
        $<$<CONFIG:Debug>:asan>
        gtest
        libbulk
    )

    target_link_libraries(test_bulk
        $<$<CONFIG:Debug>:asan>
        gtest
        libbulk
    )

    target_link_libraries(test_async
        $<$<CONFIG:Debug>:asan>
        gtest
        libbulk
        libasync
    )

endif()

if (MSVC)
    target_compile_options(libbulk PRIVATE
        /W4
    )
    target_compile_options(async PRIVATE
        /W4
    )
    target_compile_options(libasync PRIVATE
        /W4
    )
    if(WITH_BOOST_TEST)
        target_compile_options(test_version PRIVATE
            /W4
        )
    endif()
    if(WITH_GTEST)
        target_compile_options(test_versiong PRIVATE
            /W4
        )
        target_compile_options(test_bulk PRIVATE
            /W4
        )
        target_compile_options(test_async PRIVATE
            /W4
        )
    endif()
else ()
    target_compile_options(libbulk PRIVATE
        -Wall -Wextra -pedantic -Werror $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
    )
    target_compile_options(async PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
        -Wall -Wextra -pedantic -Werror
    )
    target_compile_options(bulk_server PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
        -Wall -Wextra -pedantic -Werror
    )
    target_compile_options(libasync PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
        -Wall -Wextra -pedantic -Werror
    )
    if(WITH_BOOST_TEST)
        target_compile_options(test_version PRIVATE
            -Wall -Wextra -pedantic -Werror
        )
    endif()
    if(WITH_GTEST)
        target_compile_options(test_versiong PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
            -Wall -Wextra -pedantic -Werror
        )
        target_compile_options(test_bulk PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
            -Wall -Wextra -pedantic -Werror
        )
        target_compile_options(test_async PRIVATE $<$<CONFIG:Debug>:-fsanitize=address -fsanitize=leak>
            -Wall -Wextra -pedantic -Werror
        )
    endif()
endif()

install(TARGETS async RUNTIME DESTINATION bin)
install(TARGETS bulk_server RUNTIME DESTINATION bin)
install(TARGETS libbulk LIBRARY DESTINATION lib)
install(TARGETS libasync LIBRARY DESTINATION lib)

set(CPACK_GENERATOR DEB)
set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")
set(CPACK_PACKAGE_CONTACT maxf1312@yandex.ru)
include(CPack)

if(WITH_BOOST_TEST)
    enable_testing()
    add_test(test_version test_version)
endif()

if(WITH_GTEST)
    #include(GoogleTest)
    enable_testing()
    add_test(test_versiong test_versiong)
    add_test(test_bulk test_bulk)
    add_test(test_async test_async)
endif()
