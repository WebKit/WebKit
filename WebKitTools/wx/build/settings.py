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
# Common elements of the waf build system shared by all projects.

import commands
import os
import platform
import sys

from build_utils import *
from waf_extensions import *

# to be moved to wx when it supports more configs
from wxpresets import *

wk_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '../../..'))

if sys.platform.startswith('win'):
    if not 'WXWIN' in os.environ:
        print "Please set WXWIN to the directory containing wxWidgets."
        sys.exit(1)

    wx_root = os.environ['WXWIN']
else:
    wx_root = commands.getoutput('wx-config --prefix')

jscore_dir = os.path.join(wk_root, 'JavaScriptCore')
webcore_dir = os.path.join(wk_root, 'WebCore')
wklibs_dir = os.path.join(wk_root, 'WebKitLibraries')

common_defines = []
common_cxxflags = []
common_includes = []
common_libs = []
common_libpaths = []
common_frameworks = []

jscore_dirs = [
    'API',
    'bytecode',
    'bytecompiler',
    'debugger',
    'DerivedSources',
    'interpreter',
    'jit',
    'parser',
    'pcre',
    'profiler',
    'runtime',
    'wtf',
    'wtf/unicode',
    'wtf/unicode/icu',
]

webcore_dirs = [
    'accessibility', 
    'bindings/js', 
    'bridge', 
    'bridge/c', 
    'css',
    'DerivedSources',
    'dom',
    'dom/default',
    'editing', 
    'history', 
    'html', 
    'inspector', 
    'loader', 
    'loader/appcache', 
    'loader/archive', 
    'loader/icon', 
    'page', 
    'page/animation', 
    'platform', 
    'platform/animation', 
    'platform/graphics', 
    'platform/graphics/transforms',
    'platform/image-decoders',
    'platform/image-decoders/bmp', 
    'platform/image-decoders/gif', 
    'platform/image-decoders/ico', 
    'platform/image-decoders/jpeg', 
    'platform/image-decoders/png', 
    'platform/image-decoders/xbm', 
    'platform/image-decoders/zlib', 
    'platform/network', 
    'platform/sql', 
    'platform/text', 
    'plugins', 
    'rendering', 
    'rendering/style', 
    'storage', 
    'xml'
]

config_file = os.path.join(wk_root, 'WebKitBuild', 'Configuration')
config = 'Debug'

if os.path.exists(config_file):
    config = open(config_file).read()

output_dir = os.path.join(wk_root, 'WebKitBuild', config)

waf_configname = config.upper()

build_port = "wx"
building_on_win32 = sys.platform.startswith('win')

if building_on_win32:
    if config == 'Release':
        waf_configname = waf_configname + ' CRT_MULTITHREADED_DLL'
    else:
        waf_configname = waf_configname + ' CRT_MULTITHREADED_DLL_DBG'

create_hash_table = wk_root + "/JavaScriptCore/create_hash_table"
if building_on_win32:
    create_hash_table = get_output('cygpath --unix "%s"' % create_hash_table)
os.environ['CREATE_HASH_TABLE'] = create_hash_table

feature_defines = ['ENABLE_DATABASE', 'ENABLE_XSLT', 'ENABLE_JAVASCRIPT_DEBUGGER']

def common_set_options(opt):
    """
    Initialize common options provided to the user.
    """
    opt.tool_options('compiler_cxx')
    opt.tool_options('compiler_cc')
    opt.tool_options('python')
    
    opt.add_option('--wxpython', action='store_true', default=False, help='Create the wxPython bindings.')

def common_configure(conf):
    """
    Configuration used by all targets, called from the target's configure() step.
    """
    conf.check_tool('compiler_cxx')
    conf.check_tool('compiler_cc')
    conf.check_tool('python')
    conf.check_python_headers()
    
    if sys.platform.startswith('darwin'):
        conf.check_tool('osx')
    
    msvc_version = 'msvc2008'
    if building_on_win32:
        found_versions = conf.get_msvc_versions()
        if found_versions[0][0] == 'msvc 9.0':
            msvc_version = 'msvc2008'
        elif found_versions[0][0] == 'msvc 8.0':
            msvc_version = 'msvc2005'
       
    msvclibs_dir = ''
    if build_port == "wx":
        update_wx_deps(wk_root, msvc_version)
        msvclibs_dir = os.path.join(wklibs_dir, msvc_version, 'win')
    
        conf.env.append_value('CXXDEFINES', ['BUILDING_WX__=1', 'WTF_USE_WXGC=1'])

        if building_on_win32:
            conf.env.append_value('LIBPATH', os.path.join(msvclibs_dir, 'lib'))
            # wx settings
            wxdefines, wxincludes, wxlibs, wxlibpaths = get_wxmsw_settings(wx_root, shared=True, unicode=True, wxPython=True)
            conf.env['CXXDEFINES_WX'] = wxdefines
            conf.env['CPPPATH_WX'] = wxincludes
            conf.env['LIB_WX'] = wxlibs
            conf.env['LIBPATH_WX'] = wxlibpaths
    
    if sys.platform.startswith('darwin'):
        conf.env['LIB_ICU'] = ['icucore']
        # Apple does not ship the ICU headers with Mac OS X, so WebKit includes a copy of 3.2 headers
        conf.env['CPPPATH_ICU'] = [os.path.join(jscore_dir, 'icu'), os.path.join(webcore_dir, 'icu')]
    
        conf.env.append_value('CPPPATH', wklibs_dir)
        conf.env.append_value('LIBPATH', wklibs_dir)
    
    #conf.env['PREFIX'] = output_dir
    
    libprefix = ''
    if building_on_win32:
        libprefix = 'lib'
    
    conf.env['LIB_JSCORE'] = [libprefix + 'jscore']
    conf.env['LIB_WEBCORE'] = [libprefix + 'webcore']
    conf.env['LIB_WXWEBKIT'] = ['wxwebkit']
    conf.env['CXXDEFINES_WXWEBKIT'] = ['WXUSINGDLL_WEBKIT']
    
    conf.env.append_value('CXXDEFINES', feature_defines)
    if config == 'Release':
        conf.env.append_value('CPPDEFINES', 'NDEBUG')
        
    if building_on_win32:
        conf.env.append_value('CPPPATH', [
            os.path.join(jscore_dir, 'os-win32'),
            os.path.join(msvclibs_dir, 'include'),
            os.path.join(msvclibs_dir, 'include', 'pthreads'),
            os.path.join(msvclibs_dir, 'lib'),
            ])
            
        conf.env.append_value('LIB', ['libpng', 'libjpeg', 'pthreadVC2'])
        # common win libs
        conf.env.append_value('LIB', [
            'kernel32', 'user32','gdi32','comdlg32','winspool','winmm',
            'shell32', 'comctl32', 'ole32', 'oleaut32', 'uuid', 'advapi32', 
            'wsock32', 'gdiplus'])

        conf.env['LIB_ICU'] = ['icudt', 'icule', 'iculx', 'icuuc', 'icuin', 'icuio', 'icutu']
        
        #curl
        conf.env['LIB_CURL'] = ['libcurl']
        
        #sqlite3
        conf.env['CPPPATH_SQLITE3'] = [os.path.join(msvclibs_dir, 'include', 'SQLite')]
        conf.env['LIB_SQLITE3'] = ['sqlite3']
        
        #libxml2
        conf.env['LIB_XML'] = ['libxml2']
        
        #libxslt
        conf.env['LIB_XSLT'] = ['libxslt']
    else:    
        if build_port == 'wx':
            conf.env.append_value('LIB', ['png', 'pthread'])
            conf.env.append_value('LIBPATH', os.path.join(wklibs_dir, 'unix', 'lib'))
            conf.env.append_value('CPPPATH', os.path.join(wklibs_dir, 'unix', 'include'))
            conf.env.append_value('CXXFLAGS', ['-fPIC', '-DPIC'])
            
            conf.check_cfg(path='wx-config', args='--cxxflags --libs', package='', uselib_store='WX')
            
        conf.check_cfg(path='xslt-config', args='--cflags --libs', package='', uselib_store='XSLT')
        conf.check_cfg(path='xml2-config', args='--cflags --libs', package='', uselib_store='XML')
        conf.check_cfg(path='curl-config', args='--cflags --libs', package='', uselib_store='CURL')
        if sys.platform.startswith('darwin'):
            conf.env.append_value('LIB', ['WebCoreSQLite3'])
        
        if not sys.platform.startswith('darwin'):
            conf.check_cfg(package='cairo', args='--cflags --libs', uselib_store='WX')
            conf.check_cfg(package='pango', args='--cflags --libs', uselib_store='WX')
            conf.check_cfg(package='gtk+-2.0', args='--cflags --libs', uselib_store='WX')
            conf.check_cfg(package='sqlite3', args='--cflags --libs', uselib_store='SQLITE3')
            conf.check_cfg(path='icu-config', args='--cflags --ldflags', package='', uselib_store='ICU')




