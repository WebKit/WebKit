#  Copyright The ANGLE Project Authors. All rights reserved.
#  Use of this source code is governed by a BSD-style license that can be
#  found in the LICENSE file.
#
# Generates an Android.bp file from the json output of a 'gn desc' command.
# Example usage:
#   gn desc out/Android --format=json "*" > desc.json
#   python scripts/generate_android_bp.py desc.json > Android.bp

import json
import sys
import re
import os
import argparse
from datetime import date

root_targets = [
    "//:libGLESv2",
    "//:libGLESv1_CM",
    "//:libEGL",
    "//:libfeature_support",
]

sdk_version = '28'
stl = 'libc++_static'


def tabs(indent):
    return ' ' * (indent * 4)


def has_child_values(value):
    # Elements of the blueprint can be pruned if they are empty lists or dictionaries of empty
    # lists
    if isinstance(value, list):
        return len(value) > 0
    if isinstance(value, dict):
        for (item, item_value) in value.items():
            if has_child_values(item_value):
                return True
        return False

    # This is a value leaf node
    return True


def write_blueprint_key_value(output, name, value, indent=1):
    if not has_child_values(value):
        return

    if isinstance(value, set) or isinstance(value, list):
        value = list(sorted(set(value)))

    if isinstance(value, list):
        output.append(tabs(indent) + '%s: [' % name)
        for item in value:
            output.append(tabs(indent + 1) + '"%s",' % item)
        output.append(tabs(indent) + '],')
        return
    if isinstance(value, dict):
        if not value:
            return
        output.append(tabs(indent) + '%s: {' % name)
        for (item, item_value) in value.items():
            write_blueprint_key_value(output, item, item_value, indent + 1)
        output.append(tabs(indent) + '},')
        return
    if isinstance(value, bool):
        output.append(tabs(indent) + '%s: %s,' % (name, 'true' if value else 'false'))
        return
    output.append(tabs(indent) + '%s: "%s",' % (name, value))


def write_blueprint(output, target_type, values):
    output.append('%s {' % target_type)
    for (key, value) in values.items():
        write_blueprint_key_value(output, key, value)
    output.append('}')


def gn_target_to_blueprint_target(target, target_info):
    if 'output_name' in target_info:
        return target_info['output_name']

    # Prefix all targets with angle_
    # Remove the prefix //: from gn target names
    cleaned_path = re.sub(r'^//.*:', '', target)
    prefix = "angle_"
    if not cleaned_path.startswith(prefix):
        cleaned_path = prefix + cleaned_path
    return cleaned_path


def remap_gn_path(path):
    # TODO: pass the gn gen folder as an arg so it is future proof. b/150457277
    remap_folders = [
        ('out/Android/gen/angle/', ''),
        ('out/Android/gen/', ''),
    ]

    remapped_path = path
    for (remap_source, remap_dest) in remap_folders:
        remapped_path = remapped_path.replace(remap_source, remap_dest)

    return remapped_path


def gn_path_to_blueprint_path(source):
    # gn uses '//' to indicate the root directory, blueprint uses the .bp file's location
    return remap_gn_path(re.sub(r'^//?', '', source))


def gn_paths_to_blueprint_paths(paths):
    rebased_paths = []
    for path in paths:
        rebased_paths.append(gn_path_to_blueprint_path(path))
    return rebased_paths


def gn_sources_to_blueprint_sources(sources):
    # Blueprints only list source files in the sources list. Headers are only referenced though
    # include paths.
    file_extension_whitelist = [
        '.c',
        '.cc',
        '.cpp',
    ]

    rebased_sources = []
    for source in sources:
        if os.path.splitext(source)[1] in file_extension_whitelist:
            rebased_sources.append(gn_path_to_blueprint_path(source))
    return rebased_sources


target_blackist = [
    '//build/config:shared_library_deps',
]

include_blacklist = [
]


def gn_deps_to_blueprint_deps(target_info, build_info):
    static_libs = []
    shared_libs = []
    defaults = []
    generated_headers = []
    header_libs = []
    if not 'deps' in target_info:
        return (static_libs, defaults)

    for dep in target_info['deps']:
        if not dep in target_blackist:
            dep_info = build_info[dep]
            blueprint_dep_name = gn_target_to_blueprint_target(dep, dep_info)

            # Depending on the dep type, blueprints reference it differently.
            gn_dep_type = dep_info['type']
            if gn_dep_type == 'static_library':
                static_libs.append(blueprint_dep_name)
            elif gn_dep_type == 'shared_library':
                shared_libs.append(blueprint_dep_name)
            elif gn_dep_type == 'source_set' or gn_dep_type == 'group':
                defaults.append(blueprint_dep_name)
            elif gn_dep_type == 'action':
                generated_headers.append(blueprint_dep_name)

            # Blueprints do not chain linking of static libraries.
            (child_static_libs, _, _, child_generated_headers, _) = gn_deps_to_blueprint_deps(
                dep_info, build_info)

            # Each target needs to link all child static library dependencies.
            static_libs += child_static_libs

            # Each blueprint target runs genrules in a different output directory unlike GN. If a
            # target depends on another's genrule, it wont find the outputs. Propogate generated
            # headers up the dependency stack.
            generated_headers += child_generated_headers

    return (static_libs, shared_libs, defaults, generated_headers, header_libs)


def gn_libs_to_blueprint_shared_libraries(target_info):
    lib_blackist = [
        'android_support',
    ]

    result = []
    if 'libs' in target_info:
        for lib in target_info['libs']:
            if not lib in lib_blackist:
                android_lib = lib if '@' in lib else 'lib' + lib
                result.append(android_lib)
    return result


def gn_include_dirs_to_blueprint_include_dirs(target_info):
    result = []
    if 'include_dirs' in target_info:
        for include_dir in target_info['include_dirs']:
            if not include_dir in include_blacklist:
                result.append(gn_path_to_blueprint_path(include_dir))
    return result


def escape_quotes(str):
    return str.replace("\"", "\\\"").replace("\'", "\\\'")


angle_cpu_bits_define = r'^ANGLE_IS_[0-9]+_BIT_CPU$'


def gn_cflags_to_blueprint_cflags(target_info):
    result = []

    # Only forward cflags that disable warnings
    cflag_whitelist = r'^-Wno-.*$'

    for cflag_type in ['cflags', 'cflags_c', 'cflags_cc']:
        if cflag_type in target_info:
            for cflag in target_info[cflag_type]:
                if re.search(cflag_whitelist, cflag):
                    result.append(cflag)

    # Chrome and Android use different versions of Clang which support differnt warning options.
    # Ignore errors about unrecognized warning flags.
    result.append('-Wno-unknown-warning-option')

    if 'defines' in target_info:
        for define in target_info['defines']:
            # Don't emit ANGLE's CPU-bits define here, it will be part of the arch-specific
            # information later
            if not re.search(angle_cpu_bits_define, define):
                result.append('-D%s' % escape_quotes(define))

    return result


def gn_arch_specific_to_blueprint(target_info):
    arch_infos = {
        'arm': {
            'bits': 32
        },
        'arm64': {
            'bits': 64
        },
        'x86': {
            'bits': 32
        },
        'x86_64': {
            'bits': 64
        },
    }

    result = {}
    for (arch_name, arch_info) in arch_infos.items():
        result[arch_name] = {'cflags': []}

    # If the target has ANGLE's CPU-bits define, replace it with the arch-specific bits here.
    if 'defines' in target_info:
        for define in target_info['defines']:
            if re.search(angle_cpu_bits_define, define):
                for (arch_name, arch_info) in arch_infos.items():
                    result[arch_name]['cflags'].append('-DANGLE_IS_%d_BIT_CPU' % arch_info['bits'])

    return result


blueprint_library_target_types = {
    "static_library": "cc_library_static",
    "shared_library": "cc_library_shared",
    "source_set": "cc_defaults",
    "group": "cc_defaults",
}


def library_target_to_blueprint(target, build_info):
    target_info = build_info[target]

    blueprint_type = blueprint_library_target_types[target_info['type']]

    bp = {}
    bp['name'] = gn_target_to_blueprint_target(target, target_info)

    if 'sources' in target_info:
        bp['srcs'] = gn_sources_to_blueprint_sources(target_info['sources'])

    (bp['static_libs'], bp['shared_libs'], bp['defaults'], bp['generated_headers'],
     bp['header_libs']) = gn_deps_to_blueprint_deps(target_info, build_info)
    bp['shared_libs'] += gn_libs_to_blueprint_shared_libraries(target_info)

    bp['local_include_dirs'] = gn_include_dirs_to_blueprint_include_dirs(target_info)

    bp['cflags'] = gn_cflags_to_blueprint_cflags(target_info)
    bp['arch'] = gn_arch_specific_to_blueprint(target_info)

    bp['sdk_version'] = sdk_version
    bp['stl'] = stl

    return (blueprint_type, bp)


def gn_action_args_to_blueprint_args(blueprint_inputs, blueprint_outputs, args):
    # TODO: pass the gn gen folder as an arg so we know how to get from the gen path to the root
    # path. b/150457277
    remap_folders = [
        ('../../', ''),
        ('gen/', ''),
    ]

    result_args = []
    for arg in args:
        # Attempt to find if this arg is a path to one of the inputs. If it is, use the blueprint
        # $(location <path>) argument instead so the path gets remapped properly to the location
        # that the script is run from
        remapped_path_arg = arg
        for (remap_source, remap_dest) in remap_folders:
            remapped_path_arg = remapped_path_arg.replace(remap_source, remap_dest)

        if remapped_path_arg in blueprint_inputs or remapped_path_arg in blueprint_outputs:
            result_args.append('$(location %s)' % remapped_path_arg)
        elif os.path.basename(remapped_path_arg) in blueprint_outputs:
            result_args.append('$(location %s)' % os.path.basename(remapped_path_arg))
        else:
            result_args.append(remapped_path_arg)

    return result_args


blueprint_gen_types = {
    "action": "cc_genrule",
}


def action_target_to_blueprint(target, build_info):
    target_info = build_info[target]
    blueprint_type = blueprint_gen_types[target_info['type']]

    bp = {}
    bp['name'] = gn_target_to_blueprint_target(target, target_info)

    # Blueprints use only one 'srcs', merge all gn inputs into one list.
    gn_inputs = []
    if 'inputs' in target_info:
        gn_inputs += target_info['inputs']
    if 'sources' in target_info:
        gn_inputs += target_info['sources']
    bp_srcs = gn_paths_to_blueprint_paths(gn_inputs)

    bp['srcs'] = bp_srcs

    # genrules generate the output right into the 'root' directory. Strip any path before the
    # file name.
    bp_outputs = []
    for gn_output in target_info['outputs']:
        bp_outputs.append(os.path.basename(gn_output))

    bp['out'] = bp_outputs

    bp['tool_files'] = [gn_path_to_blueprint_path(target_info['script'])]

    # Generate the full command, $(location) refers to tool_files[0], the script
    cmd = ['$(location)'] + gn_action_args_to_blueprint_args(bp_srcs, bp_outputs,
                                                             target_info['args'])
    bp['cmd'] = ' '.join(cmd)

    return (blueprint_type, bp)


def gn_target_to_blueprint(target, build_info):
    gn_type = build_info[target]['type']
    if gn_type in blueprint_library_target_types:
        return library_target_to_blueprint(target, build_info)
    elif gn_type in blueprint_gen_types:
        return action_target_to_blueprint(target, build_info)
    else:
        raise RuntimeError("Unknown gn target type: " + gn_type)


def get_gn_target_dependencies(output_dependencies, build_info, target):
    output_dependencies.insert(0, target)
    for dep in build_info[target]['deps']:
        if dep in target_blackist:
            # Blacklisted dep
            continue
        if dep in output_dependencies:
            # Already added this dep
            continue
        if not dep in build_info:
            # No info for this dep, skip it
            continue

        # Recurse
        get_gn_target_dependencies(output_dependencies, build_info, dep)


def main():
    parser = argparse.ArgumentParser(
        description='Generate Android blueprints from gn descriptions.')
    parser.add_argument(
        'gn_json',
        help='gn desc in json format. Generated with \'gn desc <out_dir> --format=json "*"\'.')
    args = parser.parse_args()

    with open(args.gn_json, 'r') as f:
        build_info = json.load(f)

    targets_to_write = []
    for root_target in root_targets:
        get_gn_target_dependencies(targets_to_write, build_info, root_target)

    blueprint_targets = []

    for target in targets_to_write:
        blueprint_targets.append(gn_target_to_blueprint(target, build_info))

    # Add APKs with all of the root libraries
    blueprint_targets.append(('filegroup', {
        'name': 'ANGLE_srcs',
        'srcs': ['src/**/*.java',],
    }))

    blueprint_targets.append((
        'java_defaults',
        {
            'name':
                'ANGLE_java_defaults',
            'sdk_version':
                'system_current',
            'min_sdk_version':
                sdk_version,
            'compile_multilib':
                'both',
            'use_embedded_native_libs':
                True,
            'jni_libs': [
                gn_target_to_blueprint_target(target, build_info[target])
                for target in root_targets
            ],
            'aaptflags': [
                # Don't compress *.json files
                '-0 .json',
                # Give com.android.angle.common Java files access to the R class
                '--extra-packages com.android.angle.common',
            ],
            'srcs': [':ANGLE_srcs'],
            'privileged':
                True,
            'owner':
                'google',
        }))

    blueprint_targets.append((
        'android_library',
        {
            'name': 'ANGLE_library',
            'sdk_version': 'system_current',
            'min_sdk_version': sdk_version,
            'resource_dirs': ['src/android_system_settings/res',],
            'asset_dirs': ['src/android_system_settings/assets',],
            'aaptflags': [
                # Don't compress *.json files
                '-0 .json',
            ],
            'manifest': 'src/android_system_settings/src/com/android/angle/AndroidManifest.xml',
            'static_libs': ['androidx.preference_preference',],
        }))

    blueprint_targets.append(('android_app', {
        'name': 'ANGLE',
        'defaults': ['ANGLE_java_defaults'],
        'static_libs': ['ANGLE_library'],
        'manifest': 'src/android_system_settings/src/com/android/angle/AndroidManifest.xml',
        'required': ['privapp_whitelist_com.android.angle'],
    }))

    output = [
        """// GENERATED FILE - DO NOT EDIT.
// Generated by %s
//
// Copyright %s The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
""" % (sys.argv[0], date.today().year)
    ]
    for (blueprint_type, blueprint_data) in blueprint_targets:
        write_blueprint(output, blueprint_type, blueprint_data)

    print '\n'.join(output)


if __name__ == '__main__':
    sys.exit(main())
