# binary_dir_relative_path sets outvar to
# ${CMAKE_CURRENT_BINARY_DIR}/${cur_bin_dir_relative}, but expressed relative to
# ${CMAKE_BINARY_DIR}.
#
# TODO(davidben): When we require CMake 3.20 or later, this can be replaced with
# the built-in cmake_path(RELATIVE_PATH) function.
function(binary_dir_relative_path cur_bin_dir_relative outvar)
  string(LENGTH "${CMAKE_BINARY_DIR}/" root_dir_length)
  string(SUBSTRING "${CMAKE_CURRENT_BINARY_DIR}/${cur_bin_dir_relative}" ${root_dir_length} -1 result)
  set(${outvar} ${result} PARENT_SCOPE)
endfunction()
