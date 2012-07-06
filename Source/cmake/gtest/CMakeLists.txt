# This is an alternative CMakeLists.txt to the experimental one
# shipped with the original package. It builds only the essential
# for our tests. The WebKit version of gtest needs access to some
# headers from WTF and JSC for configuration.

INCLUDE_DIRECTORIES(${THIRDPARTY_DIR}/gtest
    ${THIRDPARTY_DIR}/gtest/include
    ${JAVASCRIPTCORE_DIR}
    ${WTF_DIR}
)

SET(GTEST_DIR "${THIRDPARTY_DIR}/gtest")

ADD_LIBRARY(gtest
    ${GTEST_DIR}/src/gtest.cc
    ${GTEST_DIR}/src/gtest-death-test.cc
    ${GTEST_DIR}/src/gtest-filepath.cc
    ${GTEST_DIR}/src/gtest_main.cc
    ${GTEST_DIR}/src/gtest-port.cc
    ${GTEST_DIR}/src/gtest-test-part.cc
    ${GTEST_DIR}/src/gtest-typed-test.cc
)
