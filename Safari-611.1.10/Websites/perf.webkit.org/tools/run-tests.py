#!/usr/bin/python

import os
import subprocess
import sys


def main():
    tools_dir = os.path.dirname(__file__)
    root_dir = os.path.abspath(os.path.join(tools_dir, '..'))
    node_modules_dir = os.path.join(root_dir, 'node_modules')

    if not os.path.exists(node_modules_dir):
        os.makedirs(node_modules_dir)
    packages = ['mocha', 'pg', 'form-data']
    for package_name in packages:
        target_dir = os.path.join(node_modules_dir, package_name)
        if not os.path.isdir(target_dir):
            subprocess.call(['npm', 'install', package_name], cwd=node_modules_dir)

    mocha_path = os.path.join(node_modules_dir, 'mocha/bin/mocha')
    test_paths = sys.argv[1:] or ['unit-tests', 'server-tests']
    return subprocess.call([mocha_path] + test_paths)

if __name__ == "__main__":
    main()
