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
FindGIDocgen
------------

Finds the ``gi-docgen`` program and adds the :command:`GI_DOCGEN` command.
The following variables will also be set:

``GIDocgen_FOUND``
  True if the ``gi-docgen`` program has been found.
``GIDocgen_VERSION``
  Version of the ``gi-docgen`` program.
``GIDocgen_EXE``
  Path to the ``gi-docgen`` program.

There are three different ways in which ``gi-docgen`` can be made available:

* Installed system-wide.
* A Python `virtualenv`__ placed at ``${CMAKE_SOURCE_DIR}/gi-docgen``.
* A copy of the ``gi-docgen`` source tree placed at
  ``${CMAKE_SOURCE_DIR}/gi-docgen``.

__ https://virtualenv.pypa.io/en/latest/

In particular, the last two methods allows distributors which do not have
access to a readily available installation of ``gi-docgen`` to provide a
local copy without needing to vendor it in the project source tree.

#]=======================================================================]

# Add a dummy command in case introspection is disabled. This allows
# always use it and automatically have it be a noop in that case, instead
# of having a check next to each invocation.
if (NOT ENABLE_DOCUMENTATION)
    function(GI_DOCGEN)
    endfunction()
    return()
endif ()

find_package(GI REQUIRED)

# Allow dropping a copy of the tool source under gi-docgen/, as it can run
# uninstalled, and also search gi-docgen/bin/ to allow placing a virtualenv
# in that location.
find_program(GIDocgen_EXE
    NAMES gi-docgen gi-docgen.py
    HINTS "${CMAKE_SOURCE_DIR}/gi-docgen/bin"
          "${CMAKE_SOURCE_DIR}/gi-docgen"
    DOC "Path to the gi-docgen program"
)


if (GIDocgen_FIND_REQUIRED)
    set(_GIDocgen_MSGLEVEL FATAL_ERROR)
else ()
    set(_GIDocgen_MSGLEVEL WARNING)
endif ()

if (NOT GIDocgen_EXE)
    message(${_GIDocgen_MSGLEVEL}
        "Could not find gi-docgen. If your system does not provide a package "
        "the following commands can be used to install it inside the WebKit "
        "source tree, where it will be found automatically:\n"
        "  virtualenv gi-docgen ${CMAKE_SOURCE_DIR}/gi-docgen\n"
        "  ${CMAKE_SOURCE_DIR}/gi-docgen/bin/pip install gi-docgen\n"
        "Another option is unpacking a gi-docgen release at the same location "
        "but in this case dependencies will not be automatically installed.\n"
    )
elseif (GIDocgen_EXE)
    execute_process(
        COMMAND ${GIDocgen_EXE} --version
        OUTPUT_VARIABLE GIDocgen_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE _GIDocgen_CMDSTATUS
        ERROR_QUIET
    )
    set(GIDocgen_VERSION "${GIDocgen_VERSION}" CACHE INTERNAL "")
    if (NOT ${_GIDocgen_CMDSTATUS} EQUAL 0)
        message(${_GIDocgen_MSGLEVEL}
            "Could not execute ${GIDocgen_EXE}. This typically signals an "
            "incorrect installation. You may want to run it by hand for "
            "troubleshooting, the re-run CMake again.\n"
        )
    endif ()
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GIDocgen
    FOUND_VAR GIDocgen_FOUND
    REQUIRED_VARS GIDocgen_EXE GIDocgen_VERSION
    VERSION_VAR GIDocgen_VERSION
)

if (NOT GIDocgen_FOUND)
    return()
endif ()

#[=======================================================================[.rst:

.. command:: GI_DOCGEN

  .. code-block:: cmake

    GI_DOCGEN(<namespace> <toml-file>
              [CONTENT_TEMPLATES <file...>])

   Configures generating documentation for a GObject-Introspection
   ``<namespace>``, which needs to have been previously configured using
   command:`GI_INTROSPECT`.

   The ``<toml-file>`` is the path to a `configuration file`__ for
   ``gi-docgen``. The file is treated as a template, and can contain
   expansions of CMake variables using ``@`` as delimiter, e.g.
   ``@PROJECT_VERSION@``.

   __ https://gnome.pages.gitlab.gnome.org/gi-docgen/project-configuration.html

   ``CONTENT_TEMPLATES`` can be used to list files, for each of them a
   template with the ``.in`` suffix will be expanded using the
   command:`configure_file` in ``@ONLY`` mode. The ``.in`` sufix must
   not be present.

   Documentation is generated in the ``Documentation/<pkgname>-<nsversion>/``
   subdirectory by the ``doc-<namespace>`` target added by the command.
   Additionally, a ``doc-check-<namespace>`` target is created as well,
   which allows running ``gi-docgen`` as a linter to report issues with
   the documentation.

   Targets ``doc-all`` and ``doc-check-all`` can be used to build and check
   the documentation modules configured for a project.

#]=======================================================================]
function(GI_DOCGEN namespace toml)
    cmake_parse_arguments(PARSE_ARGV 1 opt
        ""
        ""
        "CONTENT_TEMPLATES"
    )
    if (NOT TARGET "gir-${namespace}")
        message(FATAL_ERROR
            "Introspection not configured for '${namespace}'. "
            "Did you forget a GI_INTROSPECT(${namespace} ..) call?"
        )
    endif ()

    set(contentdir "${CMAKE_BINARY_DIR}/GIDocgenGenerated/${namespace}")

    get_property(gir_path TARGET "gir-${namespace}" PROPERTY GI_GIR_PATH)
    set(toml_path "${contentdir}.toml")
    configure_file("${toml}" "${toml_path}" @ONLY)

    get_filename_component(toml_dir "${toml}" DIRECTORY)
    get_filename_component(toml_dir "${toml_dir}" ABSOLUTE)

    get_property(package TARGET "gir-${namespace}" PROPERTY GI_PACKAGE)
    if (NOT package)
        set(package "${namespace}")
    endif ()
    set(outdir "${CMAKE_BINARY_DIR}/Documentation/${package}")

    set(docdeps "${toml_path};${gir_path}")
    foreach (item IN LISTS opt_CONTENT_TEMPLATES)
        get_filename_component(filename "${item}" NAME)
        configure_file("${item}.in" "${contentdir}/${filename}" @ONLY)
        list(APPEND docdeps "${contentdir}/${filename}")
    endforeach ()

    set(common_flags)
    list(APPEND common_flags
        --quiet
        --config "${toml_path}"
        --add-include-path "${CMAKE_BINARY_DIR}"
    )

    # Documentation generation.
    add_custom_command(
        OUTPUT "${outdir}/index.html"
        COMMENT "Generating documentation: ${namespace}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS ${docdeps}
        DEPFILE ${contentdir}.dep
        VERBATIM
        COMMAND "${GIDocgen_EXE}" gen-deps
            ${common_flags}
            --content-dir "${contentdir}"
            --content-dir "${toml_dir}"
            --config "${toml_path}"
            "${gir_path}"
            "${contentdir}.depfiles"
        COMMAND "${Python_EXECUTABLE}" -c
            "import sys; print(sys.argv[1], ':', ' '.join((l[:-1] for l in sys.stdin.readlines())))"
            "${outdir}/index.html" < "${contentdir}.depfiles" > "${contentdir}.dep"
        COMMAND "${GIDocgen_EXE}" generate
            ${common_flags}
            --no-namespace-dir
            --content-dir "${contentdir}"
            --content-dir "${toml_dir}"
            --output-dir "${outdir}"
            --config "${toml_path}"
            "${gir_path}"
    )

    add_custom_target("doc-${namespace}" DEPENDS "${outdir}/index.html")

    if (NOT TARGET doc-all)
        add_custom_target(doc-all ALL COMMENT "All documentation targets")
    endif ()
    add_dependencies(doc-all "doc-${namespace}")

    install(
        DIRECTORY "${outdir}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/doc"
        COMPONENT development
    )

    # Documentation check.
    add_custom_target("doc-check-${namespace}"
        COMMENT "Checking documentation: ${namespace}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS "${toml_path}" "${gir_path}"
        VERBATIM
        COMMAND "${GIDocgen_EXE}" check ${common_flags} "${gir_path}"
    )

    if (NOT TARGET doc-check-all)
        add_custom_target(doc-check-all COMMENT "Check all documentation targets")
    endif ()
    add_dependencies(doc-check-all "doc-check-${namespace}")
endfunction()
