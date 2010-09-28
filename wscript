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
# WebCore build script for the waf build system

import Options

from settings import *

if build_port == "wx":
    webcore_dirs.extend(['WebKit/wx', 'WebKit/wx/WebKitSupport'])

wk_includes = ['.', 'WebCore', 'WebCore/DerivedSources',
                os.path.join(wk_root, 'JavaScriptCore'),
                os.path.join(wk_root, 'JavaScriptCore', 'wtf', 'text'),
                os.path.join(wk_root, 'WebCore'),
                os.path.join(output_dir),
                'WebCore/platform/image-decoders',
                'WebCore/platform/win',
                'WebCore/workers',
        ]

if build_port == "wx":
    wk_includes.append(os.path.join(wk_root, 'WebKit/wx'))
    wk_includes.append('WebCore/platform/wx/wxcode')

if sys.platform.startswith("win"):
    wk_includes.append(os.path.join(wk_root, 'WebCore','platform','win'))
    wk_includes.append(os.path.join(wk_root, 'WebCore','platform','graphics','win'))

windows_deps = [
                'lib/pthreadVC2.dll',
                'bin/icuuc40.dll', 'bin/icudt40.dll', 'bin/icuin40.dll',
                'bin/libcurl.dll', 'bin/libeay32.dll', 'bin/ssleay32.dll', 'bin/zlib1.dll',
                'lib/sqlite3.dll', 'bin/libxml2.dll', 'bin/libxslt.dll', 'bin/iconv.dll',
                ]

webcore_sources = {}

if build_port == "wx":
    webcore_sources['wx'] = ['WebCore/platform/KillRingNone.cpp', 'WebCore/bindings/cpp/WebDOMEventTarget.cpp']  

    if building_on_win32:
        # make sure platform/wx comes after this so we get the right
        # FontPlatformData.h
        webcore_dirs.extend(['WebCore/platform/wx/wxcode/win', 'WebCore/plugins/win'])
        webcore_sources['wx-win'] = [
               'WebCore/platform/graphics/win/GlyphPageTreeNodeCairoWin.cpp',
               'WebCore/platform/graphics/win/TransformationMatrixWin.cpp',
               'WebCore/platform/ScrollAnimatorWin.cpp',
               # wxTimer on Windows has a bug that causes it to eat crashes in callbacks
               # so we need to use the Win port's implementation until the wx bug fix is
               # widely available (it was fixed in 2.8.10).
               'WebCore/platform/win/SharedTimerWin.cpp',
               'WebCore/platform/win/WebCoreInstanceHandle.cpp',
               # Use the Windows plugin architecture
               #'WebCore/plugins/win/PluginDataWin.cpp',
               'WebCore/plugins/win/PluginDatabaseWin.cpp',
               'WebCore/plugins/win/PluginMessageThrottlerWin.cpp',
               'WebCore/plugins/win/PluginPackageWin.cpp',
               'WebCore/plugins/win/PluginViewWin.cpp',
        ]
    elif sys.platform.startswith('darwin'):
        webcore_dirs.append('WebCore/plugins/mac')
        webcore_dirs.append('WebCore/platform/wx/wxcode/mac/carbon')
        webcore_dirs.append('WebCore/platform/mac')
        webcore_dirs.append('WebCore/platform/text/mac')
        webcore_sources['wx-mac'] = [
               'WebCore/platform/mac/PurgeableBufferMac.cpp',
               'WebCore/platform/mac/WebCoreNSStringExtras.mm',
               'WebCore/platform/mac/WebCoreSystemInterface.mm',
               'WebCore/platform/graphics/cg/FloatSizeCG.cpp',
               'WebCore/platform/graphics/mac/ComplexTextController.cpp',
               'WebCore/platform/graphics/mac/ComplexTextControllerCoreText.cpp',
               'WebCore/platform/graphics/mac/ComplexTextControllerATSUI.cpp',
               'WebCore/platform/graphics/mac/GlyphPageTreeNodeMac.cpp',
               'WebCore/platform/graphics/mac/SimpleFontDataATSUI.mm',
               'WebCore/platform/graphics/mac/SimpleFontDataCoreText.cpp',
               'WebCore/platform/graphics/wx/FontPlatformDataWxMac.mm',
               'WebCore/platform/text/mac/ShapeArabic.c',
               'WebCore/platform/wx/wxcode/mac/carbon/fontprops.mm',
               'WebCore/plugins/mac/PluginPackageMac.cpp',
               'WebCore/plugins/mac/PluginViewMac.mm'
        ]
    else:
        webcore_sources['wx-gtk'] = [
               'WebCore/plugins/PluginViewNone.cpp',
               'WebCore/plugins/PluginPackageNone.cpp'
        ]
        webcore_dirs.append('WebCore/platform/wx/wxcode/gtk')
        
import TaskGen
from TaskGen import taskgen, feature, after
import Task, ccroot

def generate_webcore_derived_sources():
    # build the derived sources
    derived_sources_dir = os.path.join(webcore_dir, 'DerivedSources')
    wc_dir = webcore_dir
    if building_on_win32:
        wc_dir = get_output('cygpath --unix "%s"' % wc_dir)
    if not os.path.exists(derived_sources_dir):
        os.mkdir(derived_sources_dir)

    olddir = os.getcwd()
    os.chdir(derived_sources_dir)
    
    # DerivedSources.make expects Cygwin (i.e. Unix-style) python, so use that instead.
    if building_on_win32:
        oldpath = os.environ["PATH"]
        os.environ["PATH"] = "/usr/bin" + os.pathsep + os.environ["PATH"]
    os.system('make -f %s/DerivedSources.make WebCore=%s SOURCE_ROOT=%s all FEATURE_DEFINES="%s"' % (wc_dir, wc_dir, wc_dir, ' '.join(feature_defines)))
    if building_on_win32:
        os.environ["PATH"] = oldpath
    os.chdir(olddir)

def generate_jscore_derived_sources():
    # build the derived sources
    js_dir = jscore_dir
    if building_on_win32:
        js_dir = get_output('cygpath --unix "%s"' % js_dir)
    derived_sources_dir = os.path.join(jscore_dir, 'DerivedSources')
    if not os.path.exists(derived_sources_dir):
        os.mkdir(derived_sources_dir)

    olddir = os.getcwd()
    os.chdir(derived_sources_dir)

    # DerivedSources.make expects Cygwin (i.e. Unix-style) python, so use that instead.
    if building_on_win32:
        oldpath = os.environ["PATH"]
        os.environ["PATH"] = "/usr/bin" + os.pathsep + os.environ["PATH"]
    command = 'make -f %s/DerivedSources.make JavaScriptCore=%s BUILT_PRODUCTS_DIR=%s all FEATURE_DEFINES="%s"' % (js_dir, js_dir, js_dir, ' '.join(feature_defines))
    os.system(command)
    if building_on_win32:
        os.environ["PATH"] = oldpath
    os.chdir(olddir)

def set_options(opt):
    common_set_options(opt)

def configure(conf):
    common_configure(conf)
    generate_jscore_derived_sources()
    generate_webcore_derived_sources()
    if build_port == "wx" and sys.platform.startswith('win'):
        graphics_dir = os.path.join(wk_root, 'WebCore', 'platform', 'graphics')
        # HACK ALERT: MSVC automatically adds the source file's directory as the first entry in the
        # path. Unfortunately, that means when compiling these files we will end up including
        # win/FontPlatformData.h, which breaks wx compilation. So we copy the files to the wx dir.
        for afile in ['UniscribeController.h', 'UniscribeController.cpp', 'GlyphPageTreeNodeCairoWin.cpp']:
            shutil.copy(os.path.join(graphics_dir, 'win', afile), os.path.join(graphics_dir, 'wx'))

    webcore_out_dir = os.path.join(output_dir, 'WebCore')
    if not os.path.exists(webcore_out_dir):
        os.makedirs(webcore_out_dir)
    shutil.copy('WebCore/platform/mac/WebCoreSystemInterface.h', os.path.join(output_dir, 'WebCore', 'WebCoreSystemInterface.h'))
    jscore_out_dir = os.path.join(output_dir, 'JavaScriptCore')
    if not os.path.exists(jscore_out_dir):
        os.makedirs(jscore_out_dir)
    for api_file in glob.glob(os.path.join(jscore_dir, 'API/*.h')):
        shutil.copy(api_file, os.path.join(jscore_out_dir, os.path.basename(api_file)))

    if build_port == "wx" and Options.options.wxpython:
        common_configure(conf)
        conf.check_tool('swig', tooldir='WebKit/wx/bindings/python')
        conf.check_swig_version('1.3.29')

def build(bld):  
    import TaskGen
    global wk_includes

    bld.add_subdirs('JavaScriptCore')

    if sys.platform.startswith('darwin'):
        TaskGen.task_gen.mappings['.mm'] = TaskGen.task_gen.mappings['.cxx']
        TaskGen.task_gen.mappings['.m'] = TaskGen.task_gen.mappings['.cxx']

    features = [build_port]
    exclude_patterns = ['*AllInOne.cpp', '*Brew.cpp', '*CFNet.cpp', '*Chromium*.cpp', 
            '*Efl.cpp', '*Gtk.cpp', '*Haiku.cpp', '*Mac.cpp', '*None.cpp', '*Qt.cpp', '*Safari.cpp',
            'test*bindings.*', '*WinCE.cpp', "WebDOMCanvas*.cpp", "WebDOMSVG*.cpp"]
    if build_port == 'wx':
        features.append('curl')
        exclude_patterns.append('*Win.cpp')
        
    if sys.platform.startswith('darwin'):
        features.append('cf')
        
    else:
        exclude_patterns.append('*CF.cpp')

    full_dirs = get_dirs_for_features(wk_root, features=features, dirs=webcore_dirs)

    jscore_dir = os.path.join(wk_root, 'JavaScriptCore')
    for item in os.listdir(jscore_dir):
        fullpath = os.path.join(jscore_dir, item)
        if os.path.isdir(fullpath) and not item == "os-win32" and not item == 'icu':
            wk_includes.append(fullpath)

    wk_includes.append(os.path.join(jscore_dir, 'wtf', 'unicode'))
    wk_includes.append(os.path.join(jscore_dir, 'wtf', 'unicode', 'icu'))
    wk_includes += common_includes + full_dirs
    if sys.platform.startswith('darwin'):
        wk_includes.append(os.path.join(webcore_dir, 'icu'))

    cxxflags = []
    if building_on_win32:
        cxxflags.append('/FIWebCorePrefix.h')
    else:
        cxxflags.extend(['-include', 'WebCorePrefix.h'])

    webcore = bld.new_task_gen(
        features = 'cc cxx cshlib',
        includes = ' '.join(wk_includes),
        source = ' '.join(flattenSources(webcore_sources.values())),
        cxxflags = cxxflags,
        defines = ['WXMAKINGDLL_WEBKIT', 'BUILDING_WebCore'],
        libpath = [output_dir],
        target = 'wxwebkit',
        uselib = 'WX ICU XML XSLT CURL SQLITE3 WKINTERFACE ' + get_config(),
        uselib_local = 'jscore',
        install_path = output_dir,
        )
        
    excludes = []
    
    if build_port == 'wx':
        excludes = get_excludes(webcore_dir, exclude_patterns)
        excludes.extend(['UserStyleSheetLoader.cpp', 'RenderMediaControls.cpp'])

        # intermediate sources
        excludes.append('ColorData.cpp')
        excludes.append('CSSValueKeywords.cpp')
        excludes.append('CSSPropertyNames.cpp')
        excludes.append('DocTypeStrings.cpp')
        excludes.append('HTMLEntityNames.cpp')
        excludes.append('tokenizer.cpp')

        # Qt specific file in common sources
        excludes.append('ContextShadow.cpp')

        # FIXME: these three require headers that I can't seem to find in trunk.
        # Investigate how to resolve these issues.
        excludes.append('JSAbstractView.cpp')
        excludes.append('JSPositionCallback.cpp')
        excludes.append('JSInspectorController.cpp')
        
        # The bindings generator seems to think these are ref-counted, while they aren't in trunk.
        excludes.append('JSElementTimeControl.cpp')
        excludes.append('JSSVGAnimatedPathData.cpp')
        excludes.append('JSSVGAnimatedPoints.cpp')
        excludes.append('JSSVGExternalResourcesRequired.cpp')
        excludes.append('JSSVGFilterPrimitiveStandardAttributes.cpp')
        excludes.append('JSSVGLocatable.cpp')
        excludes.append('JSSVGStyleTable.cpp')
        excludes.append('JSSVGTests.cpp')
        excludes.append('JSSVGStylable.cpp')
        excludes.append('JSSVGZoomAndPan.cpp')
        
        # These are files that expect methods not in the base C++ class, usually XYZAnimated methods.
        excludes.append('JSSVGFitToViewBox.cpp')
        excludes.append('JSSVGLangSpace.cpp')
        excludes.append('JSSVGTransformable.cpp')
        excludes.append('JSSVGURIReference.cpp')
        
        # These are C++ DOM Bindings that won't compile because they look for things not in trunk.
        excludes.append('WebDOMEventTarget.cpp')
        excludes.append('WebDOMAbstractView.cpp')
        excludes.append('WebDOMBlobBuilder.cpp')
        excludes.append('WebDOMEventListenerCustom.cpp')
        excludes.append('WebDOMElementTimeControl.cpp')
        excludes.append('WebDOMImageData.cpp')
        excludes.append('WebDOMInspectorBackend.cpp')
        excludes.append('WebDOMScriptProfile.cpp')
        excludes.append('WebDOMScriptProfileNode.cpp')
        excludes.append('WebNativeEventListener.cpp')
        
        if building_on_win32:
            excludes.append('SharedTimerWx.cpp')
            excludes.append('RenderThemeWin.cpp')
            excludes.append('KeyEventWin.cpp')
            
        if building_on_win32 or sys.platform.startswith('darwin'):
            excludes.append('GlyphMapWx.cpp')
        excludes.append('AuthenticationCF.cpp')
        excludes.append('LoaderRunLoopCF.cpp')
        excludes.append('ResourceErrorCF.cpp')
        
        if sys.platform.startswith('darwin'):
            webcore.includes += ' WebKit/mac/WebCoreSupport WebCore/platform/mac'
            webcore.source += " WebKit/mac/WebCoreSupport/WebSystemInterface.mm"
            
        if building_on_win32:
            for wxlib in bld.env['LIB_WX']:
                wxlibname = os.path.join(bld.env['LIBPATH_WX'][0], wxlib + '_vc.dll')
                if os.path.exists(wxlibname):
                    bld.install_files(webcore.install_path, [wxlibname])
        
            for dep in windows_deps:
                bld.install_files(webcore.install_path, [os.path.join(msvclibs_dir, dep)])

    webcore.find_sources_in_dirs(full_dirs, excludes = excludes, exts=['.c', '.cpp'])

    bld.add_group()
    
    if build_port == "wx":    
        bld.add_subdirs(['WebKitTools/DumpRenderTree', 'WebKitTools/wx/browser', 'WebKit/wx/bindings/python'])
