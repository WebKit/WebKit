## Utility CMake functions.

# ----------------------------------------------------------------------------
## Convert boolean value to 0 or 1
macro (bool_to_int VAR)
  if (${VAR})
    set (${VAR} 1)
  else ()
    set (${VAR} 0)
  endif ()
endmacro ()

# ----------------------------------------------------------------------------
## Extract version numbers from version string.
function (version_numbers version major minor patch)
  if (version MATCHES "([0-9]+)(\\.[0-9]+)?(\\.[0-9]+)?(rc[1-9][0-9]*|[a-z]+)?")
    if (CMAKE_MATCH_1)
      set (_major ${CMAKE_MATCH_1})
    else ()
      set (_major 0)
    endif ()
    if (CMAKE_MATCH_2)
      set (_minor ${CMAKE_MATCH_2})
      string (REGEX REPLACE "^\\." "" _minor "${_minor}")
    else ()
      set (_minor 0)
    endif ()
    if (CMAKE_MATCH_3)
      set (_patch ${CMAKE_MATCH_3})
      string (REGEX REPLACE "^\\." "" _patch "${_patch}")
    else ()
      set (_patch 0)
    endif ()
  else ()
    set (_major 0)
    set (_minor 0)
    set (_patch 0)
  endif ()
  set ("${major}" "${_major}" PARENT_SCOPE)
  set ("${minor}" "${_minor}" PARENT_SCOPE)
  set ("${patch}" "${_patch}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Configure public header files
function (configure_headers out)
  set (tmp)
  foreach (src IN LISTS ARGN)
    if (IS_ABSOLUTE "${src}")
      list (APPEND tmp "${src}")
    elseif (EXISTS "${PROJECT_SOURCE_DIR}/src/${src}.in")
      configure_file ("${PROJECT_SOURCE_DIR}/src/${src}.in" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" @ONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    else ()
	    configure_file ("${PROJECT_SOURCE_DIR}/src/${src}" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" COPYONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    endif ()
  endforeach ()
  set (${out} "${tmp}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Configure source files with .in suffix
function (configure_sources out)
  set (tmp)
  foreach (src IN LISTS ARGN)
    if (src MATCHES ".h$" AND EXISTS "${PROJECT_SOURCE_DIR}/src/${src}.in")
      configure_file ("${PROJECT_SOURCE_DIR}/src/${src}.in" "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}" @ONLY)
      list (APPEND tmp "${PROJECT_BINARY_DIR}/include/${GFLAGS_INCLUDE_DIR}/${src}")
    else ()
      list (APPEND tmp "${PROJECT_SOURCE_DIR}/src/${src}")
    endif ()
  endforeach ()
  set (${out} "${tmp}" PARENT_SCOPE)
endfunction ()

# ----------------------------------------------------------------------------
## Add usage test
#
# Using PASS_REGULAR_EXPRESSION and FAIL_REGULAR_EXPRESSION would
# do as well, but CMake/CTest does not allow us to specify an
# expected exit status. Moreover, the execute_test.cmake script
# sets environment variables needed by the --fromenv/--tryfromenv tests.
macro (add_gflags_test name expected_rc expected_output unexpected_output cmd)
  set (args "--test_tmpdir=${PROJECT_BINARY_DIR}/Testing/Temporary"
            "--srcdir=${PROJECT_SOURCE_DIR}/test")
  add_test (
    NAME    ${name}
    COMMAND "${CMAKE_COMMAND}" "-DCOMMAND:STRING=$<TARGET_FILE:${cmd}>;${args};${ARGN}"
                               "-DEXPECTED_RC:STRING=${expected_rc}"
                               "-DEXPECTED_OUTPUT:STRING=${expected_output}"
                               "-DUNEXPECTED_OUTPUT:STRING=${unexpected_output}"
                               -P "${PROJECT_SOURCE_DIR}/cmake/execute_test.cmake"
    WORKING_DIRECTORY "${GFLAGS_FLAGFILES_DIR}"
  )
endmacro ()
