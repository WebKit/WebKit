include_directories(../include)

add_subdirectory(bio)
add_subdirectory(blowfish)
add_subdirectory(cast)
add_subdirectory(des)
add_subdirectory(dh)
add_subdirectory(dsa)
add_subdirectory(evp)
add_subdirectory(obj)
add_subdirectory(rc4)
add_subdirectory(ripemd)
add_subdirectory(rsa)
add_subdirectory(ssl)
add_subdirectory(x509)
add_subdirectory(xts)

add_library(
  decrepit

  $<TARGET_OBJECTS:bio_decrepit>
  $<TARGET_OBJECTS:blowfish>
  $<TARGET_OBJECTS:cast>
  $<TARGET_OBJECTS:des_decrepit>
  $<TARGET_OBJECTS:dh_decrepit>
  $<TARGET_OBJECTS:dsa_decrepit>
  $<TARGET_OBJECTS:evp_decrepit>
  $<TARGET_OBJECTS:obj_decrepit>
  $<TARGET_OBJECTS:rc4_decrepit>
  $<TARGET_OBJECTS:ripemd_decrepit>
  $<TARGET_OBJECTS:rsa_decrepit>
  $<TARGET_OBJECTS:ssl_decrepit>
  $<TARGET_OBJECTS:x509_decrepit>
  $<TARGET_OBJECTS:xts>
)

target_link_libraries(decrepit crypto ssl)

add_executable(
  decrepit_test

  ripemd/ripemd_test.cc

  $<TARGET_OBJECTS:gtest_main>
  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(decrepit_test crypto decrepit gtest)
if (WIN32)
  target_link_libraries(decrepit_test ws2_32)
endif()
add_dependencies(all_tests decrepit_test)
