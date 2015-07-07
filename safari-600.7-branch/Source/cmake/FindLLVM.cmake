#
# Check if the llvm-config gives us the path for the llvm libs.
#
# The following variables are set:
#  LLVM_CONFIG_EXE
#  LLVM_VERSION
#  LLVM_INCLUDE_DIRS - include directories for the llvm headers.
#  LLVM_STATIC_LIBRARIES - list of paths for the static llvm libraries.

find_program(LLVM_CONFIG_EXE NAMES "llvm-config")

execute_process(COMMAND ${LLVM_CONFIG_EXE} --version OUTPUT_VARIABLE LLVM_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG_EXE} --includedir OUTPUT_VARIABLE LLVM_INCLUDE_DIRS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND ${LLVM_CONFIG_EXE} --libfiles OUTPUT_VARIABLE LLVM_STATIC_LIBRARIES OUTPUT_STRIP_TRAILING_WHITESPACE)

# convert the list of paths into a cmake list
separate_arguments(LLVM_STATIC_LIBRARIES)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LLVM DEFAULT_MSG
                                  LLVM_VERSION LLVM_INCLUDE_DIRS LLVM_STATIC_LIBRARIES)

mark_as_advanced(LLVM_VERSION LLVM_INCLUDE_DIRS LLVM_STATIC_LIBRARIES)
