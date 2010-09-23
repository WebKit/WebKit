# Sets extra compile flags for a target, depending on the compiler being used.
# Currently, only GCC is supported.
MACRO(WEBKIT_SET_EXTRA_COMPILER_FLAGS _target)
  IF (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    GET_TARGET_PROPERTY(OLD_COMPILE_FLAGS ${_target} COMPILE_FLAGS)
    IF (${OLD_COMPILE_FLAGS} STREQUAL "OLD_COMPILE_FLAGS-NOTFOUND")
        SET(OLD_COMPILE_FLAGS "")
    ENDIF ()

    # Disable some optimizations on buggy compiler versions
    # GCC 4.5.1 does not implement -ftree-sra correctly
    EXEC_PROGRAM(${CMAKE_CXX_COMPILER} ARGS -dumpversion OUTPUT_VARIABLE COMPILER_VERSION)
    IF (${COMPILER_VERSION} STREQUAL "4.5.1")
        SET(OLD_COMPILE_FLAGS "${OLD_COMPILE_FLAGS} -fno-tree-sra")
    ENDIF ()

    IF (NOT SHARED_CORE)
        SET(OLD_COMPILE_FLAGS "-fPIC -fvisibility=hidden ${OLD_COMPILE_FLAGS}")
    ENDIF ()

    SET(OLD_COMPILE_FLAGS "-fno-exceptions -fno-strict-aliasing ${OLD_COMPILE_FLAGS}")

    SET_TARGET_PROPERTIES (${_target} PROPERTIES
	COMPILE_FLAGS "${OLD_COMPILE_FLAGS}")

    UNSET(OLD_COMPILE_FLAGS)
  ENDIF ()
ENDMACRO()


# Append the given flag to the target property.
# Builds on top of GET_TARGET_PROPERTY() and SET_TARGET_PROPERTIES()
MACRO (ADD_TARGET_PROPERTIES _target _property _flags)
  GET_TARGET_PROPERTY (_tmp ${_target} ${_property})
  IF (NOT _tmp)
    SET (_tmp "")
  ENDIF (NOT _tmp)

  FOREACH (f ${_flags})
    SET (_tmp "${_tmp} ${f}")
  ENDFOREACH (f ${_flags})

  SET_TARGET_PROPERTIES (${_target} PROPERTIES ${_property} ${_tmp})
  UNSET (_tmp)
ENDMACRO (ADD_TARGET_PROPERTIES _target _property _flags)


# Append the given dependencies to the source file
MACRO(ADD_SOURCE_DEPENDENCIES _source _deps)
  GET_SOURCE_FILE_PROPERTY(_tmp ${_source} OBJECT_DEPENDS)
  IF (NOT _tmp)
    SET (_tmp "")
  ENDIF ()

  FOREACH (f ${_deps})
    LIST(APPEND _tmp "${f}")
  ENDFOREACH ()

  SET_SOURCE_FILES_PROPERTIES(${_source} PROPERTIES OBJECT_DEPENDS "${_tmp}")
  UNSET(_tmp)
ENDMACRO()


# Append the given dependencies to the source file
# This one consider the given dependencies are in ${DERIVED_SOURCES_DIR}
# and prepends this to every member of dependencies list
MACRO(ADD_SOURCE_DERIVED_DEPENDENCIES _source _deps)
  SET(_tmp "")
  FOREACH (f ${_deps})
    LIST(APPEND _tmp "${DERIVED_SOURCES_DIR}/${f}")
  ENDFOREACH ()

  ADD_SOURCE_DEPENDENCIES(${_source} ${_tmp})
  UNSET(_tmp)
ENDMACRO()

