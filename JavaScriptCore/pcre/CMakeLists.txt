
project( pcre-unity )

include_directories( 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../wtf
    )
#compile dftables.c -> dftables
#run dftables to generate chartables.c

add_executable(dftables dftables.c)

get_target_property(dftables_executable dftables LOCATION)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/chartables.c
   COMMAND ${dftables_executable} ${CMAKE_CURRENT_BINARY_DIR}/chartables.c
   DEPENDS dftables )

set(pcre-unity_LIB_SRCS
   ${pcre-unity_LIB_SRCS}
   ${CMAKE_CURRENT_BINARY_DIR}/chartables.c
   pcre_get.c
   pcre_refcount.c
   pcre_ucp_findchar.c
   pcre_compile.c
   pcre_globals.c
   pcre_config.c
   pcre_version.c
   pcre_info.c
   pcre_study.c
   pcre_exec.c
   pcre_xclass.c
   pcre_tables.c
   pcre_maketables.c
   pcre_try_flipped.c
   pcre_ord2utf8.c
   pcre_fullinfo.c
   )

IF (WEBKIT_USE_KDE_SUPPORT)
    kde4_add_library(pcre-unity STATIC ${pcre-unity_LIB_SRCS})
ELSE (WEBKIT_USE_KDE_SUPPORT)
    add_library(pcre-unity STATIC ${pcre-unity_LIB_SRCS})
ENDIF (WEBKIT_USE_KDE_SUPPORT)


