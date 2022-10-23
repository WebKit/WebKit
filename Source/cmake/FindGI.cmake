# Copyright (C) 2022 Igalia S.L.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1.  Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
# 2.  Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in the
#     documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND ITS CONTRIBUTORS ``AS
# IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR ITS
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#[=======================================================================[.rst:
FindGI
------

Finds the GObject-Introspection tools and adds the :command:`GI_INTROSPECT`
command. The following variables will also be set:

``GI_FOUND``
  True if the GObject-Introspection tools are available.
``GI_VERSION``
  Version of the GObject-Introspection tools.
``GI_SCANNER_EXE``
  Path to the ``g-ir-scanner`` program.
``GI_COMPILER_EXE``
  Path to the ``g-ir-compiler`` program.
``GI_GIRDIR``
  Path where to install ``.gir`` files in the target system.
``GI_TYPELIBDIR``
  Path where to install ``.typelib`` files in the target system.
``GI_HAVE_SOURCES_TOP_DIRS``
  Whether the introspection scannner supports the ``--sources-top-dirs=``
  command line flag.

#]=======================================================================]

# Add a dummy command in case introspection is disabled. This allows
# always use it and automatically have it be a noop in that case, instead
# of having a check next to each invocation.
if (NOT ENABLE_INTROSPECTION)
    function(GI_INTROSPECT)
    endfunction()
    return()
endif ()

find_package(PkgConfig QUIET)

if (PKG_CONFIG_FOUND)
    if (PACKAGE_FIND_VERSION_COUNT GREATER 0)
        set(_gi_version_cmp ">=${PACKAGE_FIND_VERSION}")
    endif ()
    pkg_check_modules(PC_GI gobject-introspection-1.0${_gi_version_cmp})
    if (PC_GI_FOUND)
        pkg_get_variable(_GI_SCANNER_EXE gobject-introspection-1.0 g_ir_scanner)
        pkg_get_variable(_GI_COMPILER_EXE gobject-introspection-1.0 g_ir_compiler)
        pkg_get_variable(_GI_GIRDIR gobject-introspection-1.0 girdir)
        pkg_get_variable(_GI_TYPELIBDIR gobject-introspection-1.0 typelibdir)
        pkg_get_variable(_GI_PREFIX gobject-introspection-1.0 prefix)
        set(GI_VERSION ${PC_GI_VERSION})
    endif ()
endif ()

find_program(GI_SCANNER_EXE NAMES ${_GI_SCANNER_EXE} g-ir-scanner)
find_program(GI_COMPILER_EXE NAMES ${_GI_COMPILER_EXE} g-ir-compiler)

include(GNUInstallDirs)
if (_GI_GIRDIR AND _GI_PREFIX)
    string(FIND "${_GI_GIRDIR}" "${_GI_PREFIX}" _idx)
    if (_idx EQUAL 0)
        string(LENGTH "${_GI_PREFIX}" _idx)
        string(SUBSTRING "${_GI_GIRDIR}" ${_idx} -1 _GI_GIRDIR)
        set(_GI_GIRDIR "${CMAKE_INSTALL_PREFIX}/${_GI_GIRDIR}")
    else ()
        unset(_GI_GIRDIR)
    endif ()
endif ()

if (NOT _GI_GIRDIR)
    set(_GI_GIRDIR "${CMAKE_INSTALL_DATADIR}/gir-1.0")
endif ()

if (_GI_TYPELIBDIR AND _GI_PREFIX)
    string(FIND "${_GI_TYPELIBDIR}" "${_GI_PREFIX}" _idx)
    if (_idx EQUAL 0)
        string(LENGTH "${_GI_PREFIX}" _idx)
        string(SUBSTRING "${_GI_TYPELIBDIR}" ${_idx} -1 _GI_TYPELIBDIR)
        set(_GI_TYPELIBDIR "${CMAKE_INSTALL_PREFIX}/${_GI_TYPELIBDIR}")
    else ()
        unset(_GI_TYPELIBDIR)
    endif ()
endif ()

if (NOT _GI_TYPELIBDIR)
    set(_GI_TYPELIBDIR "${CMAKE_INSTALL_LIBDIR}/girepository-1.0")
endif ()

set(GI_GIRDIR "${_GI_GIRDIR}" CACHE PATH "Path to installed .gir files")
set(GI_TYPELIBDIR "${_GI_TYPELIBDIR}" CACHE PATH "Path to installed .typelib files")

if (NOT GI_VERSION AND GI_SCANNER_EXE)
    execute_process(
        COMMAND "${GI_SCANNER_EXE}" --version
        OUTPUT_VARIABLE GI_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        COMMAND_ERROR_IS_FATAL ANY
        ERROR_QUIET
    )
    if (GI_VERSION MATCHES "^g-ir-scanner[[:space:]]+([0-9.]+)")
        set(GI_VERSION ${CMAKE_MATCH_1})
    else ()
        unset(GI_VERSION)
    endif ()
endif ()

if (GI_VERSION VERSION_GREATER_EQUAL 1.59.1)
    set(GI_HAVE_SOURCES_TOP_DIRS TRUE)
else ()
    set(GI_HAVE_SOURCES_TOP_DIRS FALSE)
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GI
    REQUIRED_VARS GI_SCANNER_EXE GI_COMPILER_EXE GI_GIRDIR GI_TYPELIBDIR
    VERSION_VAR GI_VERSION
)

if (NOT GI_FOUND)
    return()
endif ()

define_property(TARGET
    PROPERTY GI_GIR_PATH
    BRIEF_DOCS "Path to .gir file"
    FULL_DOCS "Path to .gir file generated by the target"
)
define_property(TARGET
    PROPERTY GI_PACKAGE
    BRIEF_DOCS "Exported package"
    FULL_DOCS "Name of the pkg-config package for the target"
)


#[=======================================================================[.rst:

.. command:: GI_INTROSPECT

  .. code-block:: cmake

    GI_INTROSPECT(<namespace> <nsversion> <header>
                  [TARGET <target>]
                  [SYMBOL_PREFIX <string>]
                  [IDENTIFIER_PREFIX <string>]
                  [PACKAGE <pkgname>]
                  [DEPENDENCIES <dependency>...]
                  [SOURCES <file>...
                  [NO_IMPLICIT_SOURCES])

  Enables generating introspection data for a library ``<target>``, which
  will make the introspected API available in the ``<namespace>-<nsversion>``
  module. Both ``.gir`` and ``.typelib`` will be built and configured for
  installation.

  The ``<header>`` argument indicates how to include the top-level *public*
  API header for the library, for example ``gtk/gtk.h``.

  ``TARGET`` specifies the name of the CMake used to build the library to scan
  for introspection data. If not specified, the default value is the same as
  the ``<namespace>``.

  ``SYMBOL_PREFIX`` specifies the prefix of symbols (functions) to scan for.
  If not specified, the default value is the ``<namespace>`` converted to
  lowercase.

  ``IDENTIFIER_PREFIX`` specifies the prefix of identifiers (types) to scan
  for. If not specified, the default value is the ``<namespace>`` converted
  to uppercase.

  ``PACKAGE`` indicates the ``pkg-config`` package exported by the generated
  ``.gir``. If not specified, the default value is the ``<namespace>``
  converted to lowercase.

  ``DEPENDENCIES`` specifies an optional list of dependencies of the library
  being scanned for introspection data. Each dependecy can be one of:

  * A GObject-Introspection module name, e.g. ``GObject-2.0``.
  * A GObject-Introspection module name, a colon, and the name of its
    ``pkg-config`` package, e.g. ``Gtk-4.0:gtk4``. This is useful for
    those modules where both names don't match.
  * Another ``<namespace>`` from a previous usage of the command. This
    will add its ``.gir`` as an uninstalled dependency and ensure that
    dependencies are built beforehand.

  By default the sources scanned for introspection documentation comments
  are those used to build the ``<target>`` library, plus those specified
  with ``SOURCES``. Adding the ``NO_IMPLICIT_SOURCES`` flag uses only the
  latter.

  The command creates a target named ``gir-<namespace>`` to build the
  ``.gir`` file, with two properties: ``GI_GIR_PATH`` contains the path
  to the generated (uninstalled) file, and ``GI_PACKAGE`` containing the
  string ``<pkgname>-<nsversion>``.

  A target named ``typelib-<namespace>`` is created as well to build the
  ``.typelib`` file.

  Targets ``gir-all`` and ``typelib-all`` can be used to build all the
  ``.gir`` and ``.typelib`` files for a project.

#]=======================================================================]

function(GI_INTROSPECT namespace nsversion header)
    cmake_parse_arguments(PARSE_ARGV 2 opt
        "NO_IMPLICIT_SOURCES"
        "IDENTIFIER_PREFIX;PACKAGE;SYMBOL_PREFIX;TARGET"
        "DEPENDENCIES;SOURCES;OPTIONS"
    )
    if (NOT opt_PACKAGE)
        string(TOLOWER "${namespace}" opt_PACKAGE)
    endif ()
    if (NOT opt_SYMBOL_PREFIX)
        string(TOLOWER "${namespace}" opt_SYMBOL_PREFIX)
    endif ()
    if (NOT opt_IDENTIFIER_PREFIX)
        string(TOUPPER "${opt_SYMBOL_PREFIX}" opt_IDENTIFIER_PREFIX)
    endif ()
    if (NOT opt_TARGET)
        set(opt_TARGET "${namespace}")
    endif ()

    if (NOT TARGET ${opt_TARGET})
        message(FATAL_ERROR "Target '${opt_TARGET}' was not defined")
    endif ()

    set(gir_deps)
    set(gir_name "${namespace}-${nsversion}")
    set(gir_path "${CMAKE_BINARY_DIR}/${gir_name}.gir")
    set(typ_path "${CMAKE_BINARY_DIR}/${gir_name}.typelib")

    set(scanner_flags)
    if (GI_HAVE_SOURCES_TOP_DIRS)
        list(APPEND scanner_flags "--sources-top-dirs=${CMAKE_SOURCE_DIR}")
    endif ()

    # Each dependency can be:
    #   * GI include, i.e. "GObject-2.0", implies --include=GObject-2.0, --pkg=gobject-2.0
    #   * GI include ":" pkgconfig module, i.e. "Gtk-4.0:gtk4", implies --include=Gtk-4.0, --pkg=gtk4
    #   * CMake target, i.e. "JavaScriptCore", implies --include-uninstalled=<girfile>. The target
    #     must have been previously used with GI_INTROSPECT(), and for each use on the target the
    #     corresponding <girfile> will be picked automatically.
    foreach (dep IN LISTS opt_DEPENDENCIES)
        if (TARGET "gir-${dep}")
            get_property(dep_gir_path TARGET "gir-${dep}" PROPERTY GI_GIR_PATH)
            get_property(dep_gir_lib TARGET "gir-${dep}" PROPERTY GI_GIR_LIBRARY)
            if (dep_gir_path)
                list(APPEND scanner_flags "--include-uninstalled=${dep_gir_path}")
                list(APPEND gir_deps "${dep_gir_path}")
            else ()
                message(AUTHOR_WARNING
                    "Target '${dep}' listed as a dependency but it has not "
                    "been previously configured with GI_INTROSPECT()"
                )
            endif ()
            if (dep_gir_lib)
                list(APPEND scanner_flags "--library=${dep_gir_lib}")
            endif ()
        elseif (dep MATCHES "^([a-zA-Z0-9._-]+):([a-z0-9._\\+-]+)$")
            list(APPEND scanner_flags
                "--include=${CMAKE_MATCH_1}"
                "--pkg=${CMAKE_MATCH_2}"
            )
        else ()
            string(TOLOWER "${dep}" dep_pkg)
            list(APPEND scanner_flags
                "--include=${dep}"
                "--pkg=${dep_pkg}"
            )
        endif ()
    endforeach ()

    get_property(target_srcdir TARGET ${opt_TARGET} PROPERTY SOURCE_DIR)
    foreach (incdir IN LISTS ${opt_TARGET}_INTERFACE_INCLUDE_DIRECTORIES)
        if (NOT IS_ABSOLUTE "${incdir}")
            get_filename_component(incdir "${incdir}" REALPATH BASE_DIR "${target_srcdir}")
        endif ()
        list(APPEND scanner_flags "-I${incdir}")
    endforeach ()

    set(gir_srcs "")
    foreach (src IN LISTS opt_SOURCES)
        if (NOT IS_ABSOLUTE "${src}")
            get_filename_component(src "${src}" REALPATH BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
        endif ()
        if (IS_DIRECTORY "${src}")
            if (EXISTS "${src}")
                file(GLOB src_files LIST_DIRECTORIES FALSE CONFIGURE_DEPENDS "${src}/*.c" "${src}/*.cpp")
                if (src_files)
                    list(APPEND gir_srcs ${src_files})
                else ()
                    message(AUTHOR_WARNING "Directory '${src}' specified as source, but contains no source files")
                endif ()
            else ()
                message(AUTHOR_WARNING "Directory '${src}' specified as source, but it does not exist")
            endif ()
        else ()
            list(APPEND gir_srcs "${src}")
        endif ()
    endforeach ()

    if (NOT opt_NO_IMPLICIT_SOURCES)
        foreach (src IN LISTS ${opt_TARGET}_INSTALLED_HEADERS ${opt_TARGET}_SOURCES)
            if (NOT IS_ABSOLUTE "${src}")
                get_filename_component(src "${src}" REALPATH BASE_DIR "${target_srcdir}")
            endif ()
            list(APPEND gir_srcs "${src}")
        endforeach ()
    endif ()

    if (NOT gir_srcs)
        message(FATAL_ERROR "No sources to scan specified")
    endif ()

    # Generate .gir
    set(target_def "$<TARGET_PROPERTY:${opt_TARGET},COMPILE_DEFINITIONS>")
    set(target_inc "$<TARGET_PROPERTY:${opt_TARGET},INTERFACE_INCLUDE_DIRECTORIES>")
    add_custom_command(
        OUTPUT "${gir_path}"
        COMMENT "Generating ${gir_name}.gir"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS ${gir_deps} ${gir_srcs}
        VERBATIM
        COMMAND_EXPAND_LISTS
        COMMAND ${CMAKE_COMMAND} -E env "CC=${CMAKE_C_COMPILER}"
            "${GI_SCANNER_EXE}" --quiet --warn-all --warn-error --no-libtool
            "--output=${gir_path}"
            "--library=$<TARGET_FILE_BASE_NAME:${opt_TARGET}>"
            "--library-path=$<TARGET_FILE_DIR:${opt_TARGET}>"
            "--namespace=${namespace}"
            "--nsversion=${nsversion}"
            "--c-include=${header}"
            "--identifier-prefix=${opt_IDENTIFIER_PREFIX}"
            "--symbol-prefix=${opt_SYMBOL_PREFIX}"
            "--pkg-export=${opt_PACKAGE}-${nsversion}"
            "$<$<BOOL:${target_def}>:-D$<JOIN:${target_def},;-D>>"
            "$<$<BOOL:${target_inc}>:-I$<JOIN:${target_inc},;-I>>"
            ${scanner_flags}
            ${opt_OPTIONS}
            ${gir_srcs}
    )

    add_custom_target("gir-${namespace}" DEPENDS "${gir_path}")

    if (NOT TARGET gir-all)
        add_custom_target(gir-all COMMENT "All GI .gir targets")
    endif ()
    add_dependencies(gir-all "gir-${namespace}")

    install(
        FILES "${gir_path}"
        DESTINATION "${GI_GIRDIR}"
        COMPONENT runtime
    )

    # Generate .typelib
    add_custom_command(
        OUTPUT "${typ_path}"
        COMMENT "Generating ${gir_name}.typelib"
        DEPENDS "${gir_path}"
        VERBATIM
        COMMAND "${GI_COMPILER_EXE}"
            "--includedir=${CMAKE_BINARY_DIR}"
            "--output=${typ_path}"
            "${gir_path}"
    )

    add_custom_target("typelib-${namespace}" DEPENDS "${typ_path}")

    if (NOT TARGET typelib-all)
        add_custom_target(typelib-all ALL COMMENT "All GI .typelib targets")
    endif ()
    add_dependencies(typelib-all "typelib-${namespace}")

    install(
        FILES "${typ_path}"
        DESTINATION "${GI_TYPELIBDIR}"
        COMPONENT runtime
    )

    # Record in targets to use later on e.g. with gi-docgen.
    set_property(TARGET "gir-${namespace}" PROPERTY GI_GIR_PATH "${gir_path}")
    set_property(TARGET "gir-${namespace}" PROPERTY GI_GIR_LIBRARY "$<TARGET_FILE_BASE_NAME:${opt_TARGET}>")
    set_property(TARGET "gir-${namespace}" PROPERTY GI_PACKAGE "${opt_PACKAGE}-${nsversion}")
endfunction()
