#!/usr/bin/python

import os
import subprocess


def main():
    tools_dir = os.path.dirname(__file__)
    root_dir = os.path.abspath(os.path.join(tools_dir, '..'))
    node_modules_dir = os.path.join(root_dir, 'node_modules')

    os.chdir(root_dir)
    packages = ['mocha', 'pg']
    for package_name in packages:
        target_dir = os.path.join(node_modules_dir, package_name)
        if not os.path.isdir(target_dir):
            subprocess.call(['npm', 'install', package_name])

    mocha_path = os.path.join(node_modules_dir, 'mocha/bin/mocha')
    return subprocess.call([mocha_path, 'unit-tests', 'server-tests'])

if __name__ == "__main__":
    main()
