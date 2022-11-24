#!/usr/bin/python3
#
# Copyright 2017 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# run_code_generation.py:
#   Runs ANGLE format table and other script code generation scripts.

import argparse
import hashlib
import json
import os
import subprocess
import sys
import platform

script_dir = sys.path[0]
root_dir = os.path.abspath(os.path.join(script_dir, '..'))

hash_dir = 'code_generation_hashes'

# auto_script is a standard way for scripts to return their inputs and outputs.


def get_child_script_dirname(script):
    # All script names are relative to ANGLE's root
    return os.path.dirname(os.path.abspath(os.path.join(root_dir, script)))


# Replace all backslashes with forward slashes to be platform independent
def clean_path_slashes(path):
    return path.replace("\\", "/")


# Takes a script file name which is relative to the code generation script's directory and
# changes it to be relative to the angle root directory
def rebase_script_path(script_path, relative_path):
    return os.path.relpath(os.path.join(os.path.dirname(script_path), relative_path), root_dir)


# Check if we need a module from vpython
def get_executable_name(first_line):
    binary = os.path.basename(first_line.strip().replace(' ', '/'))
    if platform.system() == 'Windows':
        if binary == 'python2':
            return 'python.bat'
        else:
            return binary + '.bat'
    else:
        return binary


def grab_from_script(script, param):
    res = ''
    f = open(os.path.basename(script), 'r')
    exe = get_executable_name(f.readline())
    try:
        res = subprocess.check_output([exe, script, param]).decode().strip()
    except Exception:
        print('Error grabbing script output: %s, executable %s' % (script, exe))
        raise
    f.close()
    if res == '':
        return []
    return [clean_path_slashes(rebase_script_path(script, name)) for name in res.split(',')]


def auto_script(script):
    # Set the CWD to the script directory.
    os.chdir(get_child_script_dirname(script))
    base_script = os.path.basename(script)
    info = {
        'inputs': grab_from_script(base_script, 'inputs'),
        'outputs': grab_from_script(base_script, 'outputs')
    }
    # Reset the CWD to the root ANGLE directory.
    os.chdir(root_dir)
    return info


generators = {
    'ANGLE features':
        'include/platform/gen_features.py',
    'ANGLE format':
        'src/libANGLE/renderer/gen_angle_format_table.py',
    'ANGLE load functions table':
        'src/libANGLE/renderer/gen_load_functions_table.py',
    'ANGLE shader preprocessor':
        'src/compiler/preprocessor/generate_parser.py',
    'ANGLE shader translator':
        'src/compiler/translator/generate_parser.py',
    'D3D11 blit shader selection':
        'src/libANGLE/renderer/d3d/d3d11/gen_blit11helper.py',
    'D3D11 format':
        'src/libANGLE/renderer/d3d/d3d11/gen_texture_format_table.py',
    'DXGI format':
        'src/libANGLE/renderer/gen_dxgi_format_table.py',
    'DXGI format support':
        'src/libANGLE/renderer/gen_dxgi_support_tables.py',
    'Emulated HLSL functions':
        'src/compiler/translator/gen_emulated_builtin_function_tables.py',
    'Extension files':
        'src/libANGLE/gen_extensions.py',
    'GL copy conversion table':
        'src/libANGLE/gen_copy_conversion_table.py',
    'GL CTS (dEQP) build files':
        'scripts/gen_vk_gl_cts_build.py',
    'GL/EGL/WGL loader':
        'scripts/generate_loader.py',
    'GL/EGL entry points':
        'scripts/generate_entry_points.py',
    'GLenum value to string map':
        'scripts/gen_gl_enum_utils.py',
    'GL format map':
        'src/libANGLE/gen_format_map.py',
    'interpreter utils':
        'scripts/gen_interpreter_utils.py',
    'Metal format table':
        'src/libANGLE/renderer/metal/gen_mtl_format_table.py',
    'Metal default shaders':
        'src/libANGLE/renderer/metal/shaders/gen_mtl_internal_shaders.py',
    'OpenGL dispatch table':
        'src/libANGLE/renderer/gl/generate_gl_dispatch_table.py',
    'overlay fonts':
        'src/libANGLE/gen_overlay_fonts.py',
    'overlay widgets':
        'src/libANGLE/gen_overlay_widgets.py',
    'packed enum':
        'src/common/gen_packed_gl_enums.py',
    'proc table':
        'scripts/gen_proc_table.py',
    'restricted traces':
        'src/tests/restricted_traces/gen_restricted_traces.py',
    'SPIR-V helpers':
        'src/common/spirv/gen_spirv_builder_and_parser.py',
    'Static builtins':
        'src/compiler/translator/gen_builtin_symbols.py',
    'Test spec JSON':
        'infra/specs/generate_test_spec_json.py',
    'uniform type':
        'src/common/gen_uniform_type_table.py',
    'Vulkan format':
        'src/libANGLE/renderer/vulkan/gen_vk_format_table.py',
    'Vulkan internal shader programs':
        'src/libANGLE/renderer/vulkan/gen_vk_internal_shaders.py',
    'Vulkan mandatory format support table':
        'src/libANGLE/renderer/vulkan/gen_vk_mandatory_format_support_table.py',
}


def md5(fname):
    hash_md5 = hashlib.md5()
    with open(fname, "r") as f:
        for chunk in iter(lambda: f.read(4096), ""):
            hash_md5.update(chunk.encode())
    return hash_md5.hexdigest()


def get_hash_file_name(name):
    return name.replace(' ', '_').replace('/', '_') + '.json'


def any_hash_dirty(name, filenames, new_hashes, old_hashes):
    found_dirty_hash = False

    for fname in filenames:
        if not os.path.isfile(fname):
            print('File not found: "%s". Code gen dirty for %s' % (fname, name))
            found_dirty_hash = True
        else:
            new_hashes[fname] = md5(fname)
            if (not fname in old_hashes) or (old_hashes[fname] != new_hashes[fname]):
                print('Hash for "%s" dirty for %s generator.' % (fname, name))
                found_dirty_hash = True
    return found_dirty_hash


def any_old_hash_missing(all_new_hashes, all_old_hashes):
    result = False
    for file, old_hashes in all_old_hashes.items():
        if file not in all_new_hashes:
            print('"%s" does not exist. Code gen dirty.' % file)
            result = True
        else:
            for name, _ in old_hashes.items():
                if name not in all_new_hashes[file]:
                    print('Hash for %s is missing from "%s". Code gen is dirty.' % (name, file))
                    result = True
    return result


def update_output_hashes(script, outputs, new_hashes):
    for output in outputs:
        if not os.path.isfile(output):
            print('Output is missing from %s: %s' % (script, output))
            sys.exit(1)
        new_hashes[output] = md5(output)


def load_hashes():
    hashes = {}
    for file in os.listdir(hash_dir):
        hash_fname = os.path.join(hash_dir, file)
        with open(hash_fname) as hash_file:
            try:
                hashes[file] = json.load(hash_file)
            except ValueError:
                raise Exception("Could not decode JSON from %s" % file)
    return hashes


def main():
    os.chdir(script_dir)

    all_old_hashes = load_hashes()
    all_new_hashes = {}
    any_dirty = False

    parser = argparse.ArgumentParser(description='Generate ANGLE internal code.')
    parser.add_argument(
        '-v',
        '--verify-no-dirty',
        dest='verify_only',
        action='store_true',
        help='verify hashes are not dirty')
    parser.add_argument(
        '-g', '--generator', action='append', nargs='*', type=str, dest='specified_generators'),

    args = parser.parse_args()

    ranGenerators = generators
    runningSingleGenerator = False
    if (args.specified_generators):
        ranGenerators = {k: v for k, v in generators.items() if k in args.specified_generators[0]}
        runningSingleGenerator = True

    if len(ranGenerators) == 0:
        print("No valid generators specified.")
        return 1

    for name, script in sorted(ranGenerators.items()):
        info = auto_script(script)
        fname = get_hash_file_name(name)
        filenames = info['inputs'] + info['outputs'] + [script]
        new_hashes = {}
        if fname not in all_old_hashes:
            all_old_hashes[fname] = {}
        if any_hash_dirty(name, filenames, new_hashes, all_old_hashes[fname]):
            any_dirty = True

            if not args.verify_only:
                print('Running ' + name + ' code generator')

                # Set the CWD to the script directory.
                os.chdir(get_child_script_dirname(script))

                f = open(os.path.basename(script), "r")
                if subprocess.call([get_executable_name(f.readline()),
                                    os.path.basename(script)]) != 0:
                    sys.exit(1)
                f.close()

        # Update the hash dictionary.
        all_new_hashes[fname] = new_hashes

    if not runningSingleGenerator and any_old_hash_missing(all_new_hashes, all_old_hashes):
        any_dirty = True

    if args.verify_only:
        sys.exit(any_dirty)

    if any_dirty:
        args = ['git.bat'] if os.name == 'nt' else ['git']
        args += ['cl', 'format']
        print('Calling git cl format')
        if subprocess.call(args) != 0:
            sys.exit(1)

        # Update the output hashes again since they can be formatted.
        for name, script in sorted(ranGenerators.items()):
            info = auto_script(script)
            fname = get_hash_file_name(name)
            update_output_hashes(name, info['outputs'], all_new_hashes[fname])

        os.chdir(script_dir)

        for fname, new_hashes in all_new_hashes.items():
            hash_fname = os.path.join(hash_dir, fname)
            json.dump(
                new_hashes,
                open(hash_fname, "w"),
                indent=2,
                sort_keys=True,
                separators=(',', ':\n    '))


if __name__ == '__main__':
    sys.exit(main())
