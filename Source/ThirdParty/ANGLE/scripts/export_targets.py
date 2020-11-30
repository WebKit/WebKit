#! /usr/bin/env python3
assert __name__ == '__main__'

'''
To update ANGLE in Gecko, use Windows with git-bash, and setup depot_tools, python2, and
python3. Because depot_tools expects `python` to be `python2` (shame!), python2 must come
before python3 in your path.

Upstream: https://chromium.googlesource.com/angle/angle

Our repo: https://github.com/mozilla/angle
It has branches like 'firefox-60' which is the branch we use for pulling into
Gecko with this script.

This script leaves a record of the merge-base and cherry-picks that we pull into
Gecko. (gfx/angle/cherries.log)

ANGLE<->Chrome version mappings are here: https://omahaproxy.appspot.com/
An easy choice is to grab Chrome's Beta's ANGLE branch.

## Usage

Prepare your env:

~~~
export PATH="$PATH:/path/to/depot_tools"
~~~

If this is a new repo, don't forget:

~~~
# In the angle repo:
./scripts/bootstrap.py
gclient sync
~~~

Update: (in the angle repo)

~~~
# In the angle repo:
/path/to/gecko/gfx/angle/update-angle.py origin/chromium/XXXX
git push moz # Push the firefox-XX branch to github.com/mozilla/angle
~~~~

'''

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import * # mypy annotations

REPO_DIR = pathlib.Path.cwd()

GN_ENV = dict(os.environ)
# We need to set DEPOT_TOOLS_WIN_TOOLCHAIN to 0 for non-Googlers, but otherwise
# leave it unset since vs_toolchain.py assumes that the user is a Googler with
# the Visual Studio files in depot_tools if DEPOT_TOOLS_WIN_TOOLCHAIN is not
# explicitly set to 0.
vs_found = False
for directory in os.environ['PATH'].split(os.pathsep):
    vs_dir = os.path.join(directory, 'win_toolchain', 'vs_files')
    if os.path.exists(vs_dir):
        vs_found = True
        break
if not vs_found:
    GN_ENV['DEPOT_TOOLS_WIN_TOOLCHAIN'] = '0'

if len(sys.argv) < 3:
    sys.exit('Usage: export_targets.py OUT_DIR ROOTS...')

(OUT_DIR, *ROOTS) = sys.argv[1:]
for x in ROOTS:
    assert x.startswith('//:')

# ------------------------------------------------------------------------------

def run_checked(*args, **kwargs):
    print(' ', args, file=sys.stderr)
    sys.stderr.flush()
    return subprocess.run(args, check=True, **kwargs)


def sortedi(x):
    return sorted(x, key=str.lower)


def dag_traverse(root_keys: Sequence[str], pre_recurse_func: Callable[[str], list]):
    visited_keys: Set[str] = set()

    def recurse(key):
        if key in visited_keys:
            return
        visited_keys.add(key)

        t = pre_recurse_func(key)
        try:
            (next_keys, post_recurse_func) = t
        except ValueError:
            (next_keys,) = t
            post_recurse_func = None

        for x in next_keys:
            recurse(x)

        if post_recurse_func:
            post_recurse_func(key)
        return

    for x in root_keys:
        recurse(x)
    return

# ------------------------------------------------------------------------------

print('Importing graph', file=sys.stderr)

try:
    p = run_checked('gn', 'desc', '--format=json', str(OUT_DIR), '*', stdout=subprocess.PIPE,
                env=GN_ENV, shell=(True if sys.platform == 'win32' else False))
except subprocess.CalledProcessError:
    sys.stderr.buffer.write(b'"gn desc" failed. Is depot_tools in your PATH?\n')
    exit(1)

# -

print('\nProcessing graph', file=sys.stderr)
descs = json.loads(p.stdout.decode())

# Ready to traverse
# ------------------------------------------------------------------------------

LIBRARY_TYPES = ('shared_library', 'static_library')

def flattened_target(target_name: str, descs: dict, stop_at_lib: bool =True) -> dict:
    flattened = dict(descs[target_name])

    EXPECTED_TYPES = LIBRARY_TYPES + ('source_set', 'group', 'action')

    def pre(k):
        dep = descs[k]

        dep_type = dep['type']
        deps = dep['deps']
        if stop_at_lib and dep_type in LIBRARY_TYPES:
            return ((),)

        if dep_type == 'copy':
            assert not deps, (target_name, dep['deps'])
        else:
            assert dep_type in EXPECTED_TYPES, (k, dep_type)
            for (k,v) in dep.items():
                if type(v) in (list, tuple, set):
                    flattened[k] = sortedi(set(flattened.get(k, []) + v))
                else:
                    #flattened.setdefault(k, v)
                    pass
        return (deps,)

    dag_traverse(descs[target_name]['deps'], pre)
    return flattened

# ------------------------------------------------------------------------------
# Check that includes are valid. (gn's version of this check doesn't seem to work!)

INCLUDE_REGEX = re.compile(b'(?:^|\\n) *# *include +([<"])([^>"]+)[>"]')
assert INCLUDE_REGEX.match(b'#include "foo"')
assert INCLUDE_REGEX.match(b'\n#include "foo"')

# Most of these are ignored because this script does not currently handle
# #includes in #ifdefs properly, so they will erroneously be marked as being
# included, but not part of the source list.
IGNORED_INCLUDES = {
    b'compiler/translator/TranslatorESSL.h',
    b'compiler/translator/TranslatorGLSL.h',
    b'compiler/translator/TranslatorHLSL.h',
    b'compiler/translator/TranslatorMetal.h',
    b'compiler/translator/TranslatorVulkan.h',
    b'libANGLE/renderer/d3d/DeviceD3D.h',
    b'libANGLE/renderer/d3d/DisplayD3D.h',
    b'libANGLE/renderer/d3d/RenderTargetD3D.h',
    b'libANGLE/renderer/d3d/d3d11/winrt/NativeWindow11WinRT.h',
    b'libANGLE/renderer/gl/cgl/DisplayCGL.h',
    b'libANGLE/renderer/gl/eagl/DisplayEAGL.h',
    b'libANGLE/renderer/gl/egl/android/DisplayAndroid.h',
    b'libANGLE/renderer/gl/egl/DisplayEGL.h',
    b'libANGLE/renderer/gl/egl/gbm/DisplayGbm.h',
    b'libANGLE/renderer/gl/glx/DisplayGLX.h',
    b'libANGLE/renderer/gl/wgl/DisplayWGL.h',
    b'libANGLE/renderer/metal/DisplayMtl_api.h',
    b'libANGLE/renderer/null/DisplayNULL.h',
    b'libANGLE/renderer/vulkan/android/DisplayVkAndroid.h',
    b'libANGLE/renderer/vulkan/fuchsia/DisplayVkFuchsia.h',
    b'libANGLE/renderer/vulkan/ggp/DisplayVkGGP.h',
    b'libANGLE/renderer/vulkan/mac/DisplayVkMac.h',
    b'libANGLE/renderer/vulkan/win32/DisplayVkWin32.h',
    b'libANGLE/renderer/vulkan/xcb/DisplayVkXcb.h',
    b'third_party/volk/volk.h',
    b'kernel/image.h',
}

IGNORED_INCLUDE_PREFIXES = {
    b'android',
    b'Carbon',
    b'CoreFoundation',
    b'CoreServices',
    b'IOSurface',
    b'mach',
    b'mach-o',
    b'OpenGL',
    b'pci',
    b'sys',
    b'wrl',
    b'X11',
}

IGNORED_DIRECTORIES = {
    '//third_party/glslang',
    '//third_party/SwiftShader',
    '//third_party/vulkan-headers',
    '//third_party/vulkan-loader',
    '//third_party/vulkan-tools',
    '//third_party/vulkan-validation-layers',
    '//third_party/zlib',
}

def has_all_includes(target_name: str, descs: dict) -> bool:
    for ignored_directory in IGNORED_DIRECTORIES:
        if target_name.startswith(ignored_directory):
            return True

    flat = flattened_target(target_name, descs, stop_at_lib=False)
    acceptable_sources = flat.get('sources', []) + flat.get('outputs', [])
    acceptable_sources = {x.rsplit('/', 1)[-1].encode() for x in acceptable_sources}

    ret = True
    desc = descs[target_name]
    for cur_file in desc.get('sources', []):
        assert cur_file.startswith('/'), cur_file
        if not cur_file.startswith('//'):
            continue
        cur_file = pathlib.Path(cur_file[2:])
        text = cur_file.read_bytes()
        for m in INCLUDE_REGEX.finditer(text):
            if m.group(1) == b'<':
                continue
            include = m.group(2)
            if include in IGNORED_INCLUDES:
                continue
            try:
                (prefix, _) = include.split(b'/', 1)
                if prefix in IGNORED_INCLUDE_PREFIXES:
                    continue
            except ValueError:
                pass

            include_file = include.rsplit(b'/', 1)[-1]
            if include_file not in acceptable_sources:
                #print('  acceptable_sources:')
                #for x in sorted(acceptable_sources):
                #    print('   ', x)
                print('Warning in {}: {}: Invalid include: {}'.format(target_name, cur_file, include), file=sys.stderr)
                ret = False
            #print('Looks valid:', m.group())
            continue

    return ret

# -
# Gather real targets:

def gather_libraries(roots: Sequence[str], descs: dict) -> Set[str]:
    libraries = set()
    def fn(target_name):
        cur = descs[target_name]
        print('  ' + cur['type'], target_name, file=sys.stderr)
        assert has_all_includes(target_name, descs), target_name

        if cur['type'] in ('shared_library', 'static_library'):
            libraries.add(target_name)
        return (cur['deps'], )

    dag_traverse(roots, fn)
    return libraries

# -

libraries = gather_libraries(ROOTS, descs)
print(f'\n{len(libraries)} libraries:', file=sys.stderr)
for k in libraries:
    print(f'  {k}', file=sys.stderr)
print('\nstdout begins:', file=sys.stderr)
sys.stderr.flush()

# ------------------------------------------------------------------------------
# Output

out = {k: flattened_target(k, descs) for k in libraries}

for (k,desc) in out.items():
    dep_libs: Set[str] = set()
    for dep_name in set(desc['deps']):
        dep = descs[dep_name]
        if dep['type'] in LIBRARY_TYPES:
            dep_libs.add(dep_name[3:])
    desc['deps'] = sortedi(dep_libs)

json.dump(out, sys.stdout, indent='  ')
exit(0)
