#!/usr/bin/python2
#
# Copyright 2018 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# generate_loader.py:
#   Generates dynamic loaders for various binding interfaces.
#   NOTE: don't run this script directly. Run scripts/run_code_generation.py.

import sys, os, pprint, json
from datetime import date
import registry_xml


internal_prefix = "l_"


def write_header(data_source_name,
                 all_cmds,
                 api,
                 preamble,
                 path,
                 lib,
                 ns="",
                 prefix=None,
                 export=""):
    file_name = "%s_loader_autogen.h" % api
    header_path = registry_xml.path_to(path, file_name)

    def pre(cmd):
        if prefix == None:
            return cmd
        return prefix + cmd[len(api):]

    with open(header_path, "w") as out:
        defines = [
            "#define %s%s %s%s%s" % (ns, pre(cmd), internal_prefix, ns, pre(cmd))
            for cmd in all_cmds
        ]
        var_protos = [
            "%sextern PFN%sPROC %s%s%s;" % (export, cmd.upper(), internal_prefix, ns, pre(cmd))
            for cmd in all_cmds
        ]
        loader_header = template_loader_h.format(
            script_name=os.path.basename(sys.argv[0]),
            data_source_name=data_source_name,
            year=date.today().year,
            defines="\n".join(defines),
            function_pointers="\n".join(var_protos),
            api_upper=api.upper(),
            api_lower=api,
            preamble=preamble,
            export=export,
            lib=lib.upper(),
            load_fn_name="Load%s%s" % (prefix if prefix else "", api.upper()))

        out.write(loader_header)
        out.close()


def write_source(data_source_name, all_cmds, api, path, ns="", prefix=None, export=""):
    file_name = "%s_loader_autogen.cpp" % api
    source_path = registry_xml.path_to(path, file_name)

    def pre(cmd):
        if prefix == None:
            return cmd
        return prefix + cmd[len(api):]

    with open(source_path, "w") as out:
        var_defs = [
            "%sPFN%sPROC %s%s%s;" % (export, cmd.upper(), internal_prefix, ns, pre(cmd))
            for cmd in all_cmds
        ]

        setter = "    %s%s%s = reinterpret_cast<PFN%sPROC>(loadProc(\"%s\"));"
        setters = [
            setter % (internal_prefix, ns, pre(cmd), cmd.upper(), pre(cmd)) for cmd in all_cmds
        ]

        loader_source = template_loader_cpp.format(
            script_name=os.path.basename(sys.argv[0]),
            data_source_name=data_source_name,
            year=date.today().year,
            function_pointers="\n".join(var_defs),
            set_pointers="\n".join(setters),
            api_upper=api.upper(),
            api_lower=api,
            load_fn_name="Load%s%s" % (prefix if prefix else "", api.upper()))

        out.write(loader_source)
        out.close()


def gen_libegl_loader():

    data_source_name = "egl.xml and egl_angle_ext.xml"
    xml = registry_xml.RegistryXML("egl.xml", "egl_angle_ext.xml")

    for major_version, minor_version in [[1, 0], [1, 1], [1, 2], [1, 3], [1, 4], [1, 5]]:
        annotation = "{}_{}".format(major_version, minor_version)
        name_prefix = "EGL_VERSION_"

        feature_name = "{}{}".format(name_prefix, annotation)

        xml.AddCommands(feature_name, annotation)

    xml.AddExtensionCommands(registry_xml.supported_egl_extensions, ['egl'])

    all_cmds = xml.all_cmd_names.get_all_commands()

    path = os.path.join("..", "src", "libEGL")
    write_header(
        data_source_name,
        all_cmds,
        "egl",
        libegl_preamble,
        path,
        "LIBEGL",
        prefix="EGL_",
        export="ANGLE_NO_EXPORT ")
    write_source(data_source_name, all_cmds, "egl", path, prefix="EGL_")


def gen_gl_loader():

    data_source_name = "gl.xml and gl_angle_ext.xml"
    xml = registry_xml.RegistryXML("gl.xml", "gl_angle_ext.xml")

    # First run through the main GLES entry points.  Since ES2+ is the primary use
    # case, we go through those first and then add ES1-only APIs at the end.
    for major_version, minor_version in [[2, 0], [3, 0], [3, 1], [1, 0]]:
        annotation = "{}_{}".format(major_version, minor_version)
        name_prefix = "GL_ES_VERSION_"

        is_gles1 = major_version == 1
        if is_gles1:
            name_prefix = "GL_VERSION_ES_CM_"

        feature_name = "{}{}".format(name_prefix, annotation)

        xml.AddCommands(feature_name, annotation)

    xml.AddExtensionCommands(registry_xml.supported_extensions, ['gles2', 'gles1'])

    all_cmds = xml.all_cmd_names.get_all_commands()

    if registry_xml.support_EGL_ANGLE_explicit_context:
        all_cmds += [cmd + "ContextANGLE" for cmd in xml.all_cmd_names.get_all_commands()]

    path = os.path.join("..", "util")
    ex = "ANGLE_UTIL_EXPORT "
    write_header(data_source_name, all_cmds, "gles", util_gles_preamble, path, "UTIL", export=ex)
    write_source(data_source_name, all_cmds, "gles", path, export=ex)


def gen_egl_loader():

    data_source_name = "egl.xml and egl_angle_ext.xml"
    xml = registry_xml.RegistryXML("egl.xml", "egl_angle_ext.xml")

    for major_version, minor_version in [[1, 0], [1, 1], [1, 2], [1, 3], [1, 4], [1, 5]]:
        annotation = "{}_{}".format(major_version, minor_version)
        name_prefix = "EGL_VERSION_"

        feature_name = "{}{}".format(name_prefix, annotation)

        xml.AddCommands(feature_name, annotation)

    xml.AddExtensionCommands(registry_xml.supported_egl_extensions, ['egl'])

    all_cmds = xml.all_cmd_names.get_all_commands()

    path = os.path.join("..", "util")
    ex = "ANGLE_UTIL_EXPORT "
    write_header(data_source_name, all_cmds, "egl", util_egl_preamble, path, "UTIL", export=ex)
    write_source(data_source_name, all_cmds, "egl", path, export=ex)


def gen_wgl_loader():

    supported_wgl_extensions = [
        "WGL_ARB_create_context",
        "WGL_ARB_extensions_string",
        "WGL_EXT_swap_control",
    ]

    source = "wgl.xml"
    xml = registry_xml.RegistryXML(source)

    for major_version, minor_version in [[1, 0]]:
        annotation = "{}_{}".format(major_version, minor_version)
        name_prefix = "WGL_VERSION_"

        feature_name = "{}{}".format(name_prefix, annotation)

        xml.AddCommands(feature_name, annotation)

    xml.AddExtensionCommands(supported_wgl_extensions, ['wgl'])

    all_cmds = xml.all_cmd_names.get_all_commands()

    path = os.path.join("..", "util", "windows")
    write_header(source, all_cmds, "wgl", util_wgl_preamble, path, "UTIL_WINDOWS", "_")
    write_source(source, all_cmds, "wgl", path, "_")


def main():

    # Handle inputs/outputs for run_code_generation.py's auto_script
    if len(sys.argv) > 1:
        inputs = [
            'gl.xml',
            'gl_angle_ext.xml',
            'egl.xml',
            'egl_angle_ext.xml',
            'registry_xml.py',
            'wgl.xml',
        ]
        outputs = [
            '../src/libEGL/egl_loader_autogen.cpp',
            '../src/libEGL/egl_loader_autogen.h',
            '../util/egl_loader_autogen.cpp',
            '../util/egl_loader_autogen.h',
            '../util/gles_loader_autogen.cpp',
            '../util/gles_loader_autogen.h',
            '../util/windows/wgl_loader_autogen.cpp',
            '../util/windows/wgl_loader_autogen.h',
        ]

        if sys.argv[1] == 'inputs':
            print ','.join(inputs)
        elif sys.argv[1] == 'outputs':
            print ','.join(outputs)
        else:
            print('Invalid script parameters')
            return 1
        return 0

    gen_libegl_loader()
    gen_gl_loader()
    gen_egl_loader()
    gen_wgl_loader()
    return 0


libegl_preamble = """#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <export.h>
"""

util_gles_preamble = """#if defined(GL_GLES_PROTOTYPES) && GL_GLES_PROTOTYPES
#error "Don't define GL prototypes if you want to use a loader!"
#endif  // defined(GL_GLES_PROTOTYPES)

#include "angle_gl.h"
#include "util/util_export.h"
"""

util_egl_preamble = """#include "util/util_export.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
"""

util_wgl_preamble = """
#include <WGL/wgl.h>
#include <GLES2/gl2.h>

// We add an underscore before each function name to ensure common names like "ChoosePixelFormat"
// and "SwapBuffers" don't conflict with our function pointers. We can't use a namespace because
// some functions conflict with preprocessor definitions.
"""

template_loader_h = """// GENERATED FILE - DO NOT EDIT.
// Generated by {script_name} using data from {data_source_name}.
//
// Copyright {year} The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// {api_lower}_loader_autogen.h:
//   Simple {api_upper} function loader.

#ifndef {lib}_{api_upper}_LOADER_AUTOGEN_H_
#define {lib}_{api_upper}_LOADER_AUTOGEN_H_

{preamble}
{defines}
{function_pointers}

namespace angle
{{
using GenericProc = void (*)();
using LoadProc = GenericProc (KHRONOS_APIENTRY *)(const char *);
{export}void {load_fn_name}(LoadProc loadProc);
}}  // namespace angle

#endif  // {lib}_{api_upper}_LOADER_AUTOGEN_H_
"""

template_loader_cpp = """// GENERATED FILE - DO NOT EDIT.
// Generated by {script_name} using data from {data_source_name}.
//
// Copyright {year} The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// {api_lower}_loader_autogen.cpp:
//   Simple {api_upper} function loader.

#include "{api_lower}_loader_autogen.h"

{function_pointers}

namespace angle
{{
void {load_fn_name}(LoadProc loadProc)
{{
{set_pointers}
}}
}}  // namespace angle
"""

if __name__ == '__main__':
    sys.exit(main())
