set(TEST_APP_NAME "${LIB_NAME}_test")

# Dependencies
find_package(doctest REQUIRED)
find_package(nlohmann_json QUIET)

if(NOT nlohmann_json_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
    URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
  )
  FetchContent_MakeAvailable(nlohmann_json)
endif()

# Test Binary
file(GLOB TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(${TEST_APP_NAME} ${TEST_SOURCES})
target_include_directories(${TEST_APP_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(${TEST_APP_NAME} ${LIB_NAME} doctest::doctest nlohmann_json::nlohmann_json OpenSSL::Crypto)

# Enable CTest
include(doctest)
doctest_discover_tests(${TEST_APP_NAME}
                       WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
