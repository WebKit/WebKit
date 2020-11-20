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

abi_arm = 'arm'
abi_arm64 = 'arm64'
abi_x86 = 'x86'
abi_x64 = 'x86_64'

abi_targets = [abi_arm, abi_arm64, abi_x86, abi_x64]


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

    # Split the gn target name (in the form of //gn_file_path:target_name) into gn_file_path and
    # target_name
    target_regex = re.compile(r"^//([a-zA-Z0-9\-_/]*):([a-zA-Z0-9\-_.]+)$")
    match = re.match(target_regex, target)
    assert match is not None

    gn_file_path = match.group(1)
    target_name = match.group(2)
    assert len(target_name) > 0

    # Clean up the gn file path to be a valid blueprint target name.
    gn_file_path = gn_file_path.replace("/", "_").replace(".", "_").replace("-", "_")

    # Generate a blueprint target name by merging the gn path and target so each target is unique.
    # Prepend the 'angle' prefix to all targets in the root path (empty gn_file_path).
    # Skip this step if the target name already starts with 'angle' to avoid target names such as 'angle_angle_common'.
    root_prefix = "angle"
    if len(gn_file_path) == 0 and not target_name.startswith(root_prefix):
        gn_file_path = root_prefix

    # Avoid names such as _angle_common if the gn_file_path is empty.
    if len(gn_file_path) > 0:
        gn_file_path += "_"

    return gn_file_path + target_name


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
    file_extension_allowlist = [
        '.c',
        '.cc',
        '.cpp',
    ]

    rebased_sources = []
    for source in sources:
        if os.path.splitext(source)[1] in file_extension_allowlist:
            rebased_sources.append(gn_path_to_blueprint_path(source))
    return rebased_sources


target_blockist = [
    '//build/config:shared_library_deps',
    '//third_party/vulkan-validation-layers/src:vulkan_clean_old_validation_layer_objects',
]

include_blocklist = [
    '//out/Android/gen/third_party/glslang/src/include/',
]


def gn_deps_to_blueprint_deps(target_info, build_info):
    static_libs = []
    shared_libs = []
    defaults = []
    generated_headers = []
    header_libs = []
    if 'deps' not in target_info:
        return static_libs, defaults

    for dep in target_info['deps']:
        if dep not in target_blockist:
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

    return static_libs, shared_libs, defaults, generated_headers, header_libs


def gn_libs_to_blueprint_shared_libraries(target_info):
    lib_blockist = [
        'android_support',
        'unwind',
    ]

    result = []
    if 'libs' in target_info:
        for lib in target_info['libs']:
            if lib not in lib_blockist:
                android_lib = lib if '@' in lib else 'lib' + lib
                result.append(android_lib)
    return result


def gn_include_dirs_to_blueprint_include_dirs(target_info):
    result = []
    if 'include_dirs' in target_info:
        for include_dir in target_info['include_dirs']:
            if len(include_dir) > 0 and include_dir not in include_blocklist:
                result.append(gn_path_to_blueprint_path(include_dir))
    return result


def escape_quotes(string):
    return string.replace("\"", "\\\"").replace("\'", "\\\'")


angle_cpu_bits_define = r'^ANGLE_IS_[0-9]+_BIT_CPU$'


def gn_cflags_to_blueprint_cflags(target_info):
    result = []

    # regexs of allowlisted cflags
    cflag_allowlist = [
        r'^-Wno-.*$',  # forward cflags that disable warnings
        r'-mpclmul'  # forward "-mpclmul" (used by zlib)
    ]

    for cflag_type in ['cflags', 'cflags_c', 'cflags_cc']:
        if cflag_type in target_info:
            for cflag in target_info[cflag_type]:
                for allowlisted_cflag in cflag_allowlist:
                    if re.search(allowlisted_cflag, cflag):
                        result.append(cflag)

    # Chrome and Android use different versions of Clang which support differnt warning options.
    # Ignore errors about unrecognized warning flags.
    result.append('-Wno-unknown-warning-option')

    if 'defines' in target_info:
        for define in target_info['defines']:
            # Don't emit ANGLE's CPU-bits define here, it will be part of the arch-specific
            # information later
            result.append('-D%s' % escape_quotes(define))

    return result


blueprint_library_target_types = {
    "static_library": "cc_library_static",
    "shared_library": "cc_library_shared",
    "source_set": "cc_defaults",
    "group": "cc_defaults",
}


def merge_bps(bps_for_abis):
    common_bp = {}
    for abi in abi_targets:
        for key in bps_for_abis[abi]:
            if isinstance(bps_for_abis[abi][key], list):
                # Find list values that are common to all ABIs
                for value in bps_for_abis[abi][key]:
                    value_in_all_abis = True
                    for abi2 in abi_targets:
                        if key == 'defaults':
                            # arch-specific defaults are not supported
                            break
                        value_in_all_abis = value_in_all_abis and (key in bps_for_abis[abi2].keys(
                        )) and (value in bps_for_abis[abi2][key])
                    if value_in_all_abis:
                        if key in common_bp.keys():
                            common_bp[key].append(value)
                        else:
                            common_bp[key] = [value]
                    else:
                        if 'arch' not in common_bp.keys():
                            # Make sure there is an 'arch' entry to hold ABI-specific values
                            common_bp['arch'] = {}
                            for abi3 in abi_targets:
                                common_bp['arch'][abi3] = {}
                        if key in common_bp['arch'][abi].keys():
                            common_bp['arch'][abi][key].append(value)
                        else:
                            common_bp['arch'][abi][key] = [value]
            else:
                # Assume everything that's not a list is common to all ABIs
                common_bp[key] = bps_for_abis[abi][key]

    return common_bp


def library_target_to_blueprint(target, build_info):
    bps_for_abis = {}
    blueprint_type = ""
    for abi in abi_targets:
        if target not in build_info[abi].keys():
            bps_for_abis[abi] = {}
            continue

        target_info = build_info[abi][target]

        blueprint_type = blueprint_library_target_types[target_info['type']]

        bp = {'name': gn_target_to_blueprint_target(target, target_info)}

        if 'sources' in target_info:
            bp['srcs'] = gn_sources_to_blueprint_sources(target_info['sources'])

        (bp['static_libs'], bp['shared_libs'], bp['defaults'], bp['generated_headers'],
         bp['header_libs']) = gn_deps_to_blueprint_deps(target_info, build_info[abi])
        bp['shared_libs'] += gn_libs_to_blueprint_shared_libraries(target_info)

        bp['local_include_dirs'] = gn_include_dirs_to_blueprint_include_dirs(target_info)

        bp['cflags'] = gn_cflags_to_blueprint_cflags(target_info)

        bp['sdk_version'] = sdk_version
        bp['stl'] = stl
        bps_for_abis[abi] = bp

    common_bp = merge_bps(bps_for_abis)

    return blueprint_type, common_bp


def gn_action_args_to_blueprint_args(blueprint_inputs, blueprint_outputs, args):
    # TODO: pass the gn gen folder as an arg so we know how to get from the gen path to the root
    # path. b/150457277
    remap_folders = [
        # Specific special-cases first, since the other will strip the prefixes.
        ('gen/third_party/glslang/src/include/glslang/build_info.h', 'glslang/build_info.h'),
        ('third_party/glslang/src', 'external/angle/third_party/glslang/src'),
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


inputs_blocklist = [
    '//.git/HEAD',
]

outputs_remap = {
    'build_info.h': 'glslang/build_info.h',
}


def is_input_in_tool_files(tool_files, input):
    return input in tool_files


def action_target_to_blueprint(target, build_info):
    target_info = build_info[target]
    blueprint_type = blueprint_gen_types[target_info['type']]

    bp = {'name': gn_target_to_blueprint_target(target, target_info)}

    # Blueprints use only one 'srcs', merge all gn inputs into one list.
    gn_inputs = []
    if 'inputs' in target_info:
        for input in target_info['inputs']:
            if input not in inputs_blocklist:
                gn_inputs.append(input)
    if 'sources' in target_info:
        gn_inputs += target_info['sources']
    # Filter out the 'script' entry since Android.bp doesn't like the duplicate entries
    if 'script' in target_info:
        gn_inputs = [
            input for input in gn_inputs
            if not is_input_in_tool_files(target_info['script'], input)
        ]
    bp_srcs = gn_paths_to_blueprint_paths(gn_inputs)

    bp['srcs'] = bp_srcs

    # genrules generate the output right into the 'root' directory. Strip any path before the
    # file name.
    bp_outputs = []
    for gn_output in target_info['outputs']:
        output = os.path.basename(gn_output)
        if output in outputs_remap.keys():
            output = outputs_remap[output]
        bp_outputs.append(output)

    bp['out'] = bp_outputs

    bp['tool_files'] = [gn_path_to_blueprint_path(target_info['script'])]

    # Generate the full command, $(location) refers to tool_files[0], the script
    cmd = ['$(location)'] + gn_action_args_to_blueprint_args(bp_srcs, bp_outputs,
                                                             target_info['args'])
    bp['cmd'] = ' '.join(cmd)

    bp['sdk_version'] = sdk_version

    return blueprint_type, bp


def gn_target_to_blueprint(target, build_info):
    for abi in abi_targets:
        gn_type = build_info[abi][target]['type']
        if gn_type in blueprint_library_target_types:
            return library_target_to_blueprint(target, build_info)
        elif gn_type in blueprint_gen_types:
            return action_target_to_blueprint(target, build_info[abi])
        else:
            # Target is not used by this ABI
            continue


def get_gn_target_dependencies(output_dependencies, build_info, target):
    if target not in output_dependencies:
        output_dependencies.insert(0, target)

    for dep in build_info[target]['deps']:
        if dep in target_blockist:
            # Blocklisted dep
            continue
        if dep not in build_info:
            # No info for this dep, skip it
            continue

        # Recurse
        get_gn_target_dependencies(output_dependencies, build_info, dep)


def main():
    parser = argparse.ArgumentParser(
        description='Generate Android blueprints from gn descriptions.')

    for abi in abi_targets:
        fixed_abi = abi
        if abi == abi_x64:
            fixed_abi = 'x64'  # gn uses x64, rather than x86_64
        parser.add_argument(
            'gn_json_' + fixed_abi,
            help=fixed_abi +
            'gn desc in json format. Generated with \'gn desc <out_dir> --format=json "*"\'.')
    args = vars(parser.parse_args())

    build_info = {}
    for abi in abi_targets:
        fixed_abi = abi
        if abi == abi_x64:
            fixed_abi = 'x64'  # gn uses x64, rather than x86_64
        with open(args['gn_json_' + fixed_abi], 'r') as f:
            build_info[abi] = json.load(f)

    targets_to_write = []
    for abi in abi_targets:
        for root_target in root_targets:
            get_gn_target_dependencies(targets_to_write, build_info[abi], root_target)

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
            'name': 'ANGLE_java_defaults',
            'sdk_version': 'system_current',
            'min_sdk_version': sdk_version,
            'compile_multilib': 'both',
            'use_embedded_native_libs': True,
            'jni_libs': [
                # hack: assume abi_arm
                gn_target_to_blueprint_target(target, build_info[abi_arm][target])
                for target in root_targets
            ],
            'aaptflags': [
                # Don't compress *.json files
                '-0 .json',
                # Give com.android.angle.common Java files access to the R class
                '--extra-packages com.android.angle.common',
            ],
            'srcs': [':ANGLE_srcs'],
            'plugins': ['java_api_finder',],
            'privileged': True,
            'owner': 'google',
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

    print('\n'.join(output))


if __name__ == '__main__':
    sys.exit(main())
