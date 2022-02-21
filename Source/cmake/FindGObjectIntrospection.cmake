# - Try to find gobject-introspection
# Once done, this will define
#
#  INTROSPECTION_FOUND - system has gobject-introspection
#  INTROSPECTION_SCANNER - the gobject-introspection scanner, g-ir-scanner
#  INTROSPECTION_COMPILER - the gobject-introspection compiler, g-ir-compiler
#  INTROSPECTION_GENERATE - the gobject-introspection generate, g-ir-generate
#  INTROSPECTION_GIRDIR
#  INTROSPECTION_TYPELIBDIR
#  INTROSPECTION_CFLAGS
#  INTROSPECTION_LIBS
#
# Copyright (C) 2010, Pino Toscano, <pino@kde.org>
# Copyright (C) 2014 Igalia S.L.
#
# Redistribution and use is allowed according to the terms of the BSD license.



find_package(PkgConfig QUIET)
if (PKG_CONFIG_FOUND)
    if (PACKAGE_FIND_VERSION_COUNT GREATER 0)
        set(_gir_version_cmp ">=${PACKAGE_FIND_VERSION}")
    endif ()
    pkg_check_modules(_pc_gir gobject-introspection-1.0${_gir_version_cmp})
    if (_pc_gir_FOUND)
        set(INTROSPECTION_FOUND TRUE)
        pkg_get_variable(INTROSPECTION_SCANNER gobject-introspection-1.0 g_ir_scanner)
        pkg_get_variable(INTROSPECTION_COMPILER gobject-introspection-1.0 g_ir_compiler)
        pkg_get_variable(INTROSPECTION_GENERATE gobject-introspection-1.0 g_ir_generate)
        pkg_get_variable(INTROSPECTION_GIRDIR gobject-introspection-1.0 girdir)
        pkg_get_variable(INTROSPECTION_TYPELIBDIR gobject-introspection-1.0 typelibdir)
        set(INTROSPECTION_VERSION "${_pc_gir_VERSION}")
        if (${INTROSPECTION_VERSION} VERSION_GREATER_EQUAL "1.59.1")
            set(INTROSPECTION_HAVE_SOURCES_TOP_DIRS YES)
        endif ()
        set(INTROSPECTION_CFLAGS "${_pc_gir_CFLAGS}")
        set(INTROSPECTION_LIBS "${_pc_gir_LIBS}")
    endif ()
endif ()

mark_as_advanced(
    INTROSPECTION_SCANNER
    INTROSPECTION_COMPILER
    INTROSPECTION_GENERATE
    INTROSPECTION_GIRDIR
    INTROSPECTION_TYPELIBDIR
    INTROSPECTION_CFLAGS
    INTROSPECTION_LIBS
    INTROSPECTION_VERSION
    INTROSPECTION_HAVE_SOURCES_TOP_DIRS
)
