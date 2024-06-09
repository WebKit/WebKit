set(CMAKE_CROSSCOMPILING TRUE)
set(CMAKE_SYSTEM_NAME "Linux")
set(CMAKE_SYSTEM_PROCESSOR "riscv64")

option(USE_RVV "Enable riscv vector or not." ON)
option(USE_AUTO_VECTORIZER "Enable riscv auto vectorizer or not." OFF)

# Avoid to use system path for cross-compile
set(CMAKE_FIND_USE_CMAKE_SYSTEM_PATH FALSE)

set(TOOLCHAIN_PATH "" CACHE STRING "The toolcahin path.")
if(NOT TOOLCHAIN_PATH)
  set(TOOLCHAIN_PATH ${CMAKE_SOURCE_DIR}/build-toolchain-qemu/riscv-clang)
endif()

set(TOOLCHAIN_PREFIX "riscv64-unknown-linux-gnu-" CACHE STRING "The toolcahin prefix.")

# toolchain setting
set(CMAKE_C_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}clang")
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/${TOOLCHAIN_PREFIX}clang++")

# CMake will just use the host-side tools for the following tools, so we setup them here.
set(CMAKE_C_COMPILER_AR "${TOOLCHAIN_PATH}/bin/llvm-ar")
set(CMAKE_CXX_COMPILER_AR "${TOOLCHAIN_PATH}/bin/llvm-ar")
set(CMAKE_C_COMPILER_RANLIB "${TOOLCHAIN_PATH}/bin/llvm-ranlib")
set(CMAKE_CXX_COMPILER_RANLIB "${TOOLCHAIN_PATH}/bin/llvm-ranlib")
set(CMAKE_OBJDUMP "${TOOLCHAIN_PATH}/bin/llvm-objdump")
set(CMAKE_OBJCOPY "${TOOLCHAIN_PATH}/bin/llvm-objcopy")

# compile options
set(RISCV_COMPILER_FLAGS "" CACHE STRING "Compile flags")
# if user provides RISCV_COMPILER_FLAGS, appeding compile flags is avoided.
if(RISCV_COMPILER_FLAGS STREQUAL "")
  message(STATUS "USE_RVV: ${USE_RVV}")
  message(STATUS "USE_AUTO_VECTORIZER: ${USE_AUTO_VECTORIZER}")
  if(USE_RVV)
    list(APPEND RISCV_COMPILER_FLAGS "-march=rv64gcv")
    if(NOT USE_AUTO_VECTORIZER)
      # Disable auto-vectorizer
      add_compile_options(-fno-vectorize -fno-slp-vectorize)
    endif()
  else()
    list(APPEND RISCV_COMPILER_FLAGS "-march=rv64gc")
  endif()
endif()
message(STATUS "RISCV_COMPILER_FLAGS: ${RISCV_COMPILER_FLAGS}")

set(CMAKE_C_FLAGS             "${RISCV_COMPILER_FLAGS} ${CMAKE_C_FLAGS}")
set(CMAKE_CXX_FLAGS           "${RISCV_COMPILER_FLAGS} ${CMAKE_CXX_FLAGS}")

set(RISCV_LINKER_FLAGS "-lstdc++ -lpthread -lm -ldl")
set(RISCV_LINKER_FLAGS_EXE)
set(CMAKE_SHARED_LINKER_FLAGS "${RISCV_LINKER_FLAGS} ${CMAKE_SHARED_LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${RISCV_LINKER_FLAGS} ${CMAKE_MODULE_LINKER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS    "${RISCV_LINKER_FLAGS} ${RISCV_LINKER_FLAGS_EXE} ${CMAKE_EXE_LINKER_FLAGS}")
