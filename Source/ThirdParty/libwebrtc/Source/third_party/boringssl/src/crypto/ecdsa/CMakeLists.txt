include_directories(../../include)

add_library(
  ecdsa

  OBJECT

  ecdsa.c
  ecdsa_asn1.c
)


add_executable(
  ecdsa_test

  ecdsa_test.cc

  $<TARGET_OBJECTS:test_support>
)

add_executable(
  ecdsa_sign_test

  ecdsa_sign_test.cc

  $<TARGET_OBJECTS:test_support>
)

add_executable(
  ecdsa_verify_test

  ecdsa_verify_test.cc

  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(ecdsa_test crypto)
target_link_libraries(ecdsa_sign_test crypto)
target_link_libraries(ecdsa_verify_test crypto)
add_dependencies(all_tests ecdsa_test ecdsa_sign_test ecdsa_verify_test)
