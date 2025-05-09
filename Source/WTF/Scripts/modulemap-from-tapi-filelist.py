#!/usr/bin/env python3

import argparse
import itertools
import json
import os
import sys

if sys.version_info < (3, 11):
    from pip._vendor import tomli
else:
    import tomllib

parser = argparse.ArgumentParser(description="generate wtf's modulemap from a machine-readable TAPI filelist")
parser.add_argument('tapi_filelist', type=argparse.FileType())
parser.add_argument('--config', required=True)
parser.add_argument('--relative-to', help="path to WTF in build products or the SDK")
args = parser.parse_args()

if sys.version_info < (3, 11):
    config = tomli.load(open(args.config))
else:
    config = tomllib.load(open(args.config, 'rb'))

config.setdefault('attributes', [])
config.setdefault('config-macros', [])
config.setdefault('one-submodule-per-header', False)
config.setdefault('requirements', [])
config.setdefault('textual-headers', [])
config.setdefault('module', {})

submodule_mappings = {path: module_name
                      for module_name, module in config['module'].items()
                      for path in module.get('headers', [])}

filelist = json.load(args.tapi_filelist)
for header in filelist['headers']:
    path_to_header = os.path.relpath(header['path'], args.relative_to)
    if config.get('framework-module'):
        path_to_header = (path_to_header.removeprefix('PrivateHeaders/')
                          .removeprefix('Headers/'))
    if path_to_header not in submodule_mappings:
        if config['one-submodule-per-header']:
            default_submodule_name = os.path.splitext(os.path.basename(path_to_header))[0].replace('-', '')
            submodule_mappings[path_to_header] = default_submodule_name
        else:
            submodule_mappings[path_to_header] = ''

if config.get('framework-module'):
    sys.stdout.write('framework ')
sys.stdout.write(f'module {config["module-name"]} ')
for attr in config['attributes']:
    sys.stdout.write(f'[{attr}] ')
sys.stdout.write('{\n')

if config['config-macros']:
    sys.stdout.write(f'    config_macros {", ".join(config["config-macros"])}\n')

for module_name, paths in itertools.groupby(
        sorted(submodule_mappings, key=submodule_mappings.__getitem__),
        key=submodule_mappings.__getitem__):
    # The top-level module's name will be the empty string "".
    in_submodule = bool(module_name)
    attrs = config['module'].get(module_name, {}).get('attributes', config['attributes'])
    reqs = config['module'].get(module_name, {}).get('requirements', config['requirements'])

    if in_submodule:
        sys.stdout.write(f'    explicit module {module_name} ')
        for attr in attrs:
            sys.stdout.write(f'[{attr}] ')
        sys.stdout.write('{\n')
        indent = ' ' * 8
    else:
        indent = ' ' * 4
    if reqs:
        sys.stdout.write(f'{indent}requires {", ".join(reqs)}\n')
    for path in paths:
        sys.stdout.write(indent)
        if path in config['textual-headers']:
            sys.stdout.write('textual ')
        sys.stdout.write(f'header "{path}"\n')
    if in_submodule:
        sys.stdout.write('    }\n')

sys.stdout.write('''\
    export *
}
''')
