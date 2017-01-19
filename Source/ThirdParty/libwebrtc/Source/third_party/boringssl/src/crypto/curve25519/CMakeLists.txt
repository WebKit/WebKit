include_directories(../../include)

if (${ARCH} STREQUAL "arm")
  set(
    CURVE25519_ARCH_SOURCES

    asm/x25519-asm-arm.S
  )
endif()

if (${ARCH} STREQUAL "x86_64")
  set(
    CURVE25519_ARCH_SOURCES

    asm/x25519-asm-x86_64.S
  )
endif()

add_library(
  curve25519

  OBJECT

  curve25519.c
  spake25519.c
  x25519-x86_64.c

  ${CURVE25519_ARCH_SOURCES}
)

add_executable(
  ed25519_test

  ed25519_test.cc
  $<TARGET_OBJECTS:test_support>
)

target_link_libraries(ed25519_test crypto)
add_dependencies(all_tests ed25519_test)

add_executable(
  x25519_test

  x25519_test.cc
)

target_link_libraries(x25519_test crypto)
add_dependencies(all_tests x25519_test)

add_executable(
  spake25519_test

  spake25519_test.cc
)

target_link_libraries(spake25519_test crypto)
add_dependencies(all_tests spake25519_test)
