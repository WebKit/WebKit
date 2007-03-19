
project(kjs-unity)

MESSAGE(STATUS "Missing CMakeFiles.txt into wtf directory")
add_subdirectory( wtf )
add_subdirectory( pcre )

# Configuration checks
check_library_exists(pthread pthread_attr_get_np "" HAVE_PTHREAD_ATTR_GET_NP)
check_library_exists(pthread pthread_getattr_np "" HAVE_PTHREAD_GETATTR_NP)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/pcre
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings
    ${CMAKE_CURRENT_SOURCE_DIR}/bindings/c
    ${CMAKE_CURRENT_BINARY_DIR}/kjs
    ${CMAKE_CURRENT_SOURCE_DIR}/kjs
    ${CMAKE_CURRENT_SOURCE_DIR}/wtf

    ${QT_INCLUDES}
)

if(WIN32)
  include_directories( ${KDEWIN32_INCLUDES} )
endif(WIN32)

set(CREATE_HASH_TABLE ${CMAKE_CURRENT_SOURCE_DIR}/kjs/create_hash_table )

macro(CREATE_LUT _srcs_LIST _in_FILE _out_FILE _dep_FILE)

   add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      COMMAND ${PERL_EXECUTABLE} ${CREATE_HASH_TABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} -i > ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${_in_FILE} ${CMAKE_CURRENT_SOURCE_DIR}/${_dep_FILE} )
   set( ${_srcs_LIST}  ${${_srcs_LIST}} ${CMAKE_CURRENT_BINARY_DIR}/${_out_FILE})
endmacro(CREATE_LUT)

create_lut(kjs-unity_LIB_SRCS kjs/date_object.cpp kjs/date_object.lut.h kjs/date_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/number_object.cpp kjs/number_object.lut.h kjs/number_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/string_object.cpp kjs/string_object.lut.h kjs/string_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/array_object.cpp kjs/array_object.lut.h kjs/array_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/math_object.cpp kjs/math_object.lut.h kjs/math_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/regexp_object.cpp kjs/regexp_object.lut.h kjs/regexp_object.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/keywords.table kjs/lexer.lut.h kjs/lexer.cpp)
create_lut(kjs-unity_LIB_SRCS kjs/keywords.table kjs/lexer.lut.h kjs/lexer.cpp)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/kjs/grammar.cpp ${CMAKE_CURRENT_BINARY_DIR}/kjs/grammar.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}

  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/kjs
  COMMAND ${BISON_EXECUTABLE} -d -p kjsyy ${CMAKE_CURRENT_SOURCE_DIR}/kjs/grammar.y
  COMMAND mv grammar.tab.c ${CMAKE_CURRENT_BINARY_DIR}/kjs/grammar.cpp
  COMMAND mv grammar.tab.h ${CMAKE_CURRENT_BINARY_DIR}/kjs/grammar.h
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/kjs/grammar.y
)

########### next target ###############

IF (WEBKIT_USE_KDE_SUPPORT)
    kde4_add_library(wtf-unity SHARED
        wtf/TCSystemAlloc.cpp
        wtf/Assertions.cpp
        wtf/HashTable.cpp
        wtf/FastMalloc.cpp
    )
ELSE (WEBKIT_USE_KDE_SUPPORT)
    add_library(wtf-unity SHARED
        wtf/TCSystemAlloc.cpp
        wtf/Assertions.cpp
        wtf/HashTable.cpp
        wtf/FastMalloc.cpp
    )
ENDIF (WEBKIT_USE_KDE_SUPPORT)

set(kjs-unity_LIB_SRCS
    ${kjs-unity_LIB_SRCS}
    bindings/NP_jsobject.cpp
    bindings/npruntime.cpp
    bindings/runtime_array.cpp
    bindings/runtime.cpp
    bindings/runtime_method.cpp
    bindings/runtime_object.cpp
    bindings/runtime_root.cpp
    bindings/c/c_class.cpp
    bindings/c/c_instance.cpp
    bindings/c/c_runtime.cpp
    bindings/c/c_utility.cpp
    kjs/DateMath.cpp
    kjs/JSWrapperObject.cpp
    kjs/PropertyNameArray.cpp
    kjs/array_object.cpp
    kjs/bool_object.cpp
    kjs/collector.cpp
    kjs/CommonIdentifiers.cpp
    kjs/Context.cpp
    kjs/date_object.cpp
    kjs/debugger.cpp
    kjs/dtoa.cpp
    kjs/error_object.cpp
    kjs/fpconst.cpp
    kjs/function.cpp
    kjs/function_object.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/kjs/grammar.cpp
    kjs/identifier.cpp
    kjs/internal.cpp
    kjs/interpreter.cpp
    kjs/JSImmediate.cpp
    kjs/JSLock.cpp 
    kjs/lexer.cpp
    kjs/list.cpp
    kjs/lookup.cpp
    kjs/math_object.cpp
    kjs/nodes.cpp
    kjs/nodes2string.cpp
    kjs/number_object.cpp
    kjs/object.cpp
    kjs/object_object.cpp
    kjs/operations.cpp
    kjs/Parser.cpp
    kjs/property_map.cpp
    kjs/property_slot.cpp
    kjs/regexp.cpp
    kjs/regexp_object.cpp
    kjs/scope_chain.cpp
    kjs/string_object.cpp
    kjs/ustring.cpp
    kjs/value.cpp
    kjs/ExecState.cpp
)


IF (WEBKIT_USE_KDE_SUPPORT)
if(UNIX)
   kde4_add_library(kjs-unity STATIC ${kjs-unity_LIB_SRCS})
   target_link_libraries(kjs-unity  ${KDE4_KDECORE_LIBS} m )
else(UNIX)
   target_link_libraries(kjs-unity  ${KDE4_KDECORE_LIBS})
endif(UNIX)
ELSE (WEBKIT_USE_KDE_SUPPORT)
   add_library(kjs-unity STATIC ${kjs-unity_LIB_SRCS})
ENDIF (WEBKIT_USE_KDE_SUPPORT)

target_link_libraries(kjs-unity wtf-unity)
set_target_properties(kjs-unity PROPERTIES VERSION ${GENERIC_LIB_VERSION} SOVERSION ${GENERIC_LIB_SOVERSION} )

IF (WEBKIT_USE_KDE_SUPPORT)
    install (TARGETS wtf-unity DESTINATION ${LIB_INSTALL_DIR})
    install (TARGETS kjs-unity DESTINATION ${LIB_INSTALL_DIR})
ENDIF (WEBKIT_USE_KDE_SUPPORT)

# testkjs
IF (WEBKIT_USE_KDE_SUPPORT)
    set(testkjs_SRCS kjs/testkjs.cpp)
    kde4_add_executable(testkjs RUN_UNINSTALLED ${testkjs_SRCS})
    target_link_libraries(testkjs ${KDE4_KDECORE_LIBS} kjs-unity pcre-unity icuuc)
ENDIF (WEBKIT_USE_KDE_SUPPORT)
