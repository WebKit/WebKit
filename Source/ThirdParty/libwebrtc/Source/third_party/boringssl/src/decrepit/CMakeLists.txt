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
