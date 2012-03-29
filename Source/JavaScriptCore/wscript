#! /usr/bin/env python

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
# JavaScriptCore build script for the waf build system

import commands

from settings import *

def build(bld):

    import Options

    jscore_excludes = ['jsc.cpp', 'ExecutableAllocatorPosix.cpp', 'LLIntOffsetsExtractor.cpp']
    jscore_exclude_patterns = get_port_excludes(Options.options.port)
    jscore_exclude_patterns.append('*None.cpp')

    sources = []

    if Options.options.port == "wx":
        if building_on_win32:
            jscore_excludes += ['OSAllocatorPosix.cpp', 'ThreadingPthreads.cpp']
            sources.extend(['../WTF/wtf/ThreadingWin.cpp', '../WTF/wtf/ThreadSpecificWin.cpp', '../WTF/wtf/OSAllocatorWin.cpp'])
        else:
            jscore_excludes.append('JSStringRefBSTR.cpp')

    if sys.platform.startswith('darwin'):
        jscore_excludes.append('GCActivityCallback.cpp') # this is an empty impl.

    bld.env.LIBDIR = output_dir
    full_dirs = get_dirs_for_features(jscore_dir, features=[Options.options.port.lower()], dirs=jscore_dirs)
    abs_dirs = []
    for adir in full_dirs:
        abs_dirs.append(os.path.join(jscore_dir, adir))

    jscore_excludes.extend(get_excludes_in_dirs(abs_dirs, jscore_exclude_patterns))

    includes = common_includes + full_dirs + [output_dir]
    if sys.platform.startswith('darwin'):
        includes.append(os.path.join(jscore_dir, 'icu'))

    # 1. A simple program
    jscore = bld.new_task_gen(
        features = 'cc cxx cshlib',
        includes = '. .. assembler ../WTF ' + ' '.join(includes),
        source = sources,
        defines = ['BUILDING_JavaScriptCore'],
        target = 'jscore',
        uselib = 'WX ICU ' + get_config(),
        uselib_local = '',
        install_path = output_dir)

    jscore.find_sources_in_dirs(full_dirs, excludes = jscore_excludes)
    
    obj = bld.new_task_gen(
        features = 'cxx cprogram',
        includes = '. .. assembler ../WTF ' + ' '.join(includes),
        source = 'jsc.cpp',
        target = 'jsc',
        uselib = 'WX ICU ' + get_config(),
        uselib_local = 'jscore',
        install_path = output_dir,
        )
        
    if building_on_win32:
        myenv = obj.env.copy()
        myenv.CXXFLAGS = myenv.CXXFLAGS[:]
        myenv.CXXFLAGS.remove('/EHsc')
        obj.env = myenv

    bld.add_group()
