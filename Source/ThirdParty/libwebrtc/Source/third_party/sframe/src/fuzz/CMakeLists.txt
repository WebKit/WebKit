set(FUZZ_TARGETS fuzz_header fuzz_unprotect)

foreach(target ${FUZZ_TARGETS})
  add_executable(${target} ${target}.cpp)
  target_link_libraries(${target} PRIVATE ${LIB_NAME} ${CRYPTO_LIB})
  target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
  target_compile_options(${target} PRIVATE -fsanitize=fuzzer)
  target_link_options(${target} PRIVATE -fsanitize=fuzzer)
endforeach()
