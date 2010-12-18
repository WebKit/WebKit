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
# Library for functions to determine wx settings based on configuration

import os
import re

import Options

def parse_build_cfg(filename):
    cfg_file = open(filename, 'r')
    cfg = {}
    for cfg_line in cfg_file.readlines():
        key = None
        value = None
        parts = cfg_line.split('=')
        if len(parts) >= 1:
            key = parts[0].strip()
        
        if len(parts) >= 2:
            value = parts[1].strip()
            if value.isdigit():
                value = int(value)
        
        if key:
            cfg[key] = value

    return cfg

def get_wx_version(wx_root):
    versionText = open(os.path.join(wx_root, "include", "wx", "version.h"), "r").read()
    
    majorVersion = re.search("#define\swxMAJOR_VERSION\s+(\d+)", versionText).group(1)
    minorVersion = re.search("#define\swxMINOR_VERSION\s+(\d+)", versionText).group(1)
    
    return (majorVersion, minorVersion)

def get_wxmsw_settings(wx_root, shared = False, unicode = False, debug = False, wxPython=False):
    if not os.path.exists(wx_root):
        print "Directory %s does not exist." % wx_root
        sys.exit(1)

    defines = ['__WXMSW__']
    includes = [os.path.join(wx_root, 'include')]
    cxxflags = []
    libs = []
    libpaths = []
    
    libdir = os.path.join(wx_root, 'lib')
    ext = ''
    postfix = 'vc'
    
    version_str_nodot = ''.join(get_wx_version(wx_root))
    
    if shared:
        defines.append('WXUSINGDLL')
        libdir = os.path.join(libdir, Options.options.wx_compiler_prefix + '_dll')
    else:
        libdir = os.path.join(libdir, Options.options.wx_compiler_prefix + '_lib')
        
    if unicode:
        defines.append('_UNICODE')
        ext += 'u'
    
    depext = ''
    if wxPython:
        ext += 'h'
        depext += 'h'
    elif debug:
        ext += 'd'
        depext += 'd'

    configdir = os.path.join(libdir, 'msw' + ext)
    
    monolithic = False 
    cfg_file = os.path.join(configdir, 'build.cfg')
    if os.path.exists(cfg_file):
        cfg = parse_build_cfg(cfg_file)
        if "MONOLITHIC" in cfg:
            monolithic = cfg["MONOLITHIC"]
    libpaths.append(libdir)
    includes.append(configdir)
    
    def get_wxlib_name(name):
        if name == 'base':
            return 'wxbase%s%s' % (version_str_nodot, ext)
        
        return "wxmsw%s%s_%s" % (version_str_nodot, ext, name)
    
    libs.extend(['wxzlib' + depext, 'wxjpeg' + depext, 'wxpng' + depext, 'wxexpat' + depext])
    if monolithic:
        libs.extend(["wxmsw%s%s" % (version_str_nodot, ext)])
    else:
        libs.extend([get_wxlib_name('base'), get_wxlib_name('core')])
    
    if wxPython or debug:
        defines.append('__WXDEBUG__')
        
    return (defines, includes, libs, libpaths)
