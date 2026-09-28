# This is an alternative CMakeLists.txt to the one shipped with the
# original package. It builds only the essential for our tests.

set(GMOCK_DIR "${THIRDPARTY_DIR}/gmock")

set(GMOCK_INCLUDE_DIRECTORIES
    "${GMOCK_DIR}"
    "${GMOCK_DIR}/include"
)

set(GMOCK_SOURCES
    ${GMOCK_DIR}/src/gmock-all.cc
)

# Nothing in the default build uses gmock; linking it builds it.
add_library(gmock STATIC EXCLUDE_FROM_ALL ${GMOCK_SOURCES})

target_include_directories(gmock PRIVATE ${GMOCK_INCLUDE_DIRECTORIES})
target_link_libraries(gmock PUBLIC WebKit::gtest)

WEBKIT_ADD_TARGET_CXX_FLAGS(gmock -Wno-undef)

add_library(gmockFramework INTERFACE)
target_link_libraries(gmockFramework INTERFACE gmock)
target_include_directories(gmockFramework INTERFACE "${GMOCK_DIR}/include")

add_library(WebKit::gmock ALIAS gmockFramework)
