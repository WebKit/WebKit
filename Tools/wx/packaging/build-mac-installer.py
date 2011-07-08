#!/usr/bin/env python

# Copyright (C) 2009 Kevin Ollivier  All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
# EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
# PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
# OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
#
# Script for building Mac .pkg installer

import commands
import datetime
import distutils.sysconfig
import glob
import optparse
import os
import shutil
import string
import sys
import tempfile

script_dir = os.path.abspath(os.path.dirname(__file__))
sys.path.append(os.path.abspath(os.path.join(script_dir, "..", "..", "waf", "build")))

from build_utils import *

import wx

wxwk_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))
wxwebkit_dir = os.path.abspath(os.path.join(wxwk_root, "WebKitBuild", get_config(wxwk_root)))

wx_version = wx.__version__[:5]
py_version = sys.version[:3]

date = str(datetime.date.today())

platform = "osx"
    
pkgname = "wxWebKit-%s-wx%s-py%s-%s" % (platform, wx_version[:3], py_version, date)

tempdir = "/tmp/%s" % (pkgname)

if os.path.exists(tempdir):
    shutil.rmtree(tempdir)
    os.makedirs(tempdir)

installroot = os.path.join(tempdir, "install-root")
installapps = os.path.join(tempdir, "install-apps")

sp_root = distutils.sysconfig.get_python_lib()
wx_root = sp_root
if sys.platform.startswith("darwin"):
    build_string = "%s" % wx.__version__
    if wx.VERSION[1] <= 8:
        build_string = "unicode-%s" % build_string
    wx_root = "/usr/local/lib/wxPython-%s" % build_string
    sp_root = "%s/lib/python%s/site-packages" % (wx_root, py_version)

if "wxOSX-cocoa" in wx.PlatformInfo:
    sitepackages = "%s/wx-%s-osx_cocoa/wx" % (sp_root, wx_version[:5])
else:
    sitepackages = "%s/wx-%s-mac-unicode/wx" % (sp_root, wx_version[:3])
prefix = wx_root + "/lib"


def mac_update_dependencies(dylib, prefix, should_copy=True):
    """
    Copies any non-system dependencies into the bundle, and
    updates the install name path to the new path in the bundle.
    """
    global wx_root
    system_prefixes = ["/usr/lib", "/System/Library", wx_root]

    
    output = commands.getoutput("otool -L %s" % dylib).strip()
    for line in output.split("\n"):
        copy = should_copy
        change = True
        filename = line.split("(")[0].strip()
        filedir, basename = os.path.split(filename)
        dest_filename = os.path.join(prefix, basename)
        if os.path.exists(filename):
            for sys_prefix in system_prefixes:
                if filename.startswith(sys_prefix):
                    copy = False
                    change = False
            
            if copy:
                copydir = os.path.dirname(dylib)
                copyname = os.path.join(copydir, basename)
                if not os.path.exists(copyname):
                    shutil.copy(filename, copydir)
                    result = os.system("install_name_tool -id %s %s" % (dest_filename, copyname))
                    if result != 0:
                        print "Changing ID failed. Stopping release."
                        sys.exit(result)
            if change:
                print "changing %s to %s" % (filename, dest_filename)
                result = os.system("install_name_tool -change %s %s %s" % (filename, dest_filename, dylib))
                if result != 0:
                    sys.exit(result)

def exitIfError(cmd):
    print cmd
    retval = os.system(cmd)
    if retval != 0:
        if os.path.exists(tempdir):
            shutil.rmtree(tempdir)
        sys.exit(1)

wxroot = installroot + prefix
wxpythonroot = installroot + sitepackages
dmg_dir = "dmg_files"

try:
    if not os.path.exists(wxroot):
        os.makedirs(wxroot)
    
    if not os.path.exists(wxpythonroot):
        os.makedirs(wxpythonroot)
    
    for wildcard in ["*.py", "*.so"]:
        files = glob.glob(os.path.join(wxwebkit_dir, wildcard))
        for afile in files:
            shutil.copy(afile, wxpythonroot)
    
    for wildcard in ["*.dylib"]:
        files = glob.glob(os.path.join(wxwebkit_dir, wildcard))
        for afile in files:
            shutil.copy(afile, wxroot)
    
    if sys.platform.startswith("darwin"):
        dylib_path = os.path.join(wxroot, "libwxwebkit.dylib")
        os.system("install_name_tool -id %s %s" % (os.path.join(prefix, "libwxwebkit.dylib"), dylib_path))
        mac_update_dependencies(dylib_path, prefix)
        os.system("install_name_tool -id %s %s" % (os.path.join(wxpythonroot, "_webview.so"), prefix))
        mac_update_dependencies(os.path.join(wxpythonroot, "_webview.so"), prefix, should_copy=False)

        demodir = installroot + "/Applications/wxWebKit/Demos"
        if not os.path.exists(demodir):
            os.makedirs(demodir)

        shutil.copy(os.path.join(wxwk_root, "Source", "WebKit", "wx", "bindings", "python", "samples", "simple.py"), demodir)

        output_name = os.path.join(dmg_dir, pkgname + ".pkg")

        if os.path.exists(dmg_dir):
            shutil.rmtree(dmg_dir)

        os.makedirs(dmg_dir)

        pkg_args = ['--title ' + pkgname,
                    '--out %s' % output_name,
                    '--version ' + date.strip(),
                    '--id org.wxwebkit.wxwebkit',
                    '--domain system',
                    '--root-volume-only',
                    '--root ' + installroot,
                    '--verbose'
                    ]

        packagemaker = "/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker"
        exitIfError(packagemaker + " %s" % (string.join(pkg_args, " ")))

        os.system('hdiutil create -srcfolder %s -volname "%s" -imagekey zlib-level=9 %s.dmg' % (dmg_dir, pkgname, pkgname))
finally:
    if os.path.exists(tempdir):
        shutil.rmtree(tempdir)

    if os.path.exists(dmg_dir):
        shutil.rmtree(dmg_dir)
