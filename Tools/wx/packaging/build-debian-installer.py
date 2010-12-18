#!/usr/bin/env python

import os
import shutil
import sys

sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "build")))

import build_utils

script_dir = os.path.abspath(os.path.dirname(__file__))
wxwebkit_dir = os.path.abspath(os.path.join(script_dir, "..", "..", "..", "WebKitBuild", "Debug" + build_utils.git_branch_name()))
wxwk_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))

try:
    os.chdir(wxwk_root)
    deb_dir = os.path.join(wxwk_root, 'wxwebkit')
    if os.path.exists(deb_dir):
        shutil.rmtree(deb_dir)
    os.makedirs(deb_dir)
    print "Archiving git tree..."
    os.system('git archive --format=tar HEAD | gzip > %s/webkitwx_0.1.orig.tar.gz' % deb_dir)
    src_root = os.path.join(deb_dir, 'webkitwx-0.1')
    print "Extracting tree..."    
    os.makedirs(src_root)
    os.chdir(src_root)
    os.system('tar xzvf ../webkitwx_0.1.orig.tar.gz')

    shutil.copytree(os.path.join(script_dir, 'debian'), os.path.join(src_root, 'debian'))

    print "Building package..."
    os.system('fakeroot debian/rules clean')
    os.system('fakeroot debian/rules build')
    os.system('debuild -i -rfakeroot -us -uc')
finally:
    shutil.rmtree(os.path.join(src_root, 'debian'))
