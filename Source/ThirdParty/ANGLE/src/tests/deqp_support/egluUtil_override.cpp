/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief EGL utilities
 */ /*--------------------------------------------------------------------*/

#include "egluNativeDisplay_override.hpp"

#include "deSTLUtil.hpp"
#include "deStringUtil.hpp"
#include "egluConfigFilter.hpp"
#include "egluDefs.hpp"
#include "egluUtil.hpp"
#include "eglwEnums.hpp"
#include "eglwLibrary.hpp"
#include "glwEnums.hpp"
#include "tcuCommandLine.hpp"

#include <algorithm>
#include <sstream>

using std::string;
using std::vector;

namespace eglu
{

using namespace eglw;

vector<EGLint> attribMapToList(const AttribMap &attribs)
{
    vector<EGLint> attribList;

    for (AttribMap::const_iterator it = attribs.begin(); it != attribs.end(); ++it)
    {
        attribList.push_back(it->first);
        attribList.push_back(it->second);
    }

    attribList.push_back(EGL_NONE);

    return attribList;
}

Version getVersion(const Library &egl, EGLDisplay display)
{
    EGLint major, minor;

    // eglInitialize on already initialized displays just returns the version.
    EGLU_CHECK_CALL(egl, initialize(display, &major, &minor));

    return Version(major, minor);
}

vector<string> getExtensions(const Library &egl, EGLDisplay display)
{
    const char *const extensionStr = egl.queryString(display, EGL_EXTENSIONS);

    EGLU_CHECK_MSG(egl, "Querying extensions failed");

    return de::splitString(extensionStr, ' ');
}

bool hasExtension(const Library &egl, EGLDisplay display, const string &str)
{
    const vector<string> extensions = getExtensions(egl, display);
    return de::contains(extensions.begin(), extensions.end(), str);
}

vector<string> getClientExtensions(const Library &egl)
{
    return getExtensions(egl, EGL_NO_DISPLAY);
}

vector<string> getDisplayExtensions(const Library &egl, EGLDisplay display)
{
    DE_ASSERT(display != EGL_NO_DISPLAY);

    return getExtensions(egl, display);
}

vector<EGLConfig> getConfigs(const Library &egl, EGLDisplay display)
{
    vector<EGLConfig> configs;
    EGLint configCount = 0;
    EGLU_CHECK_CALL(egl, getConfigs(display, DE_NULL, 0, &configCount));

    if (configCount > 0)
    {
        configs.resize(configCount);
        EGLU_CHECK_CALL(egl,
                        getConfigs(display, &(configs[0]), (EGLint)configs.size(), &configCount));
    }

    return configs;
}

vector<EGLConfig> chooseConfigs(const Library &egl, EGLDisplay display, const EGLint *attribList)
{
    EGLint numConfigs = 0;

    EGLU_CHECK_CALL(egl, chooseConfig(display, attribList, DE_NULL, 0, &numConfigs));

    {
        vector<EGLConfig> configs(numConfigs);

        if (numConfigs > 0)
            EGLU_CHECK_CALL(
                egl, chooseConfig(display, attribList, &configs.front(), numConfigs, &numConfigs));

        return configs;
    }
}

vector<EGLConfig> chooseConfigs(const Library &egl, EGLDisplay display, const FilterList &filters)
{
    const vector<EGLConfig> allConfigs(getConfigs(egl, display));
    vector<EGLConfig> matchingConfigs;

    for (vector<EGLConfig>::const_iterator cfg = allConfigs.begin(); cfg != allConfigs.end(); ++cfg)
    {
        if (filters.match(egl, display, *cfg))
            matchingConfigs.push_back(*cfg);
    }

    return matchingConfigs;
}

EGLConfig chooseSingleConfig(const Library &egl, EGLDisplay display, const FilterList &filters)
{
    const vector<EGLConfig> allConfigs(getConfigs(egl, display));

    for (vector<EGLConfig>::const_iterator cfg = allConfigs.begin(); cfg != allConfigs.end(); ++cfg)
    {
        if (filters.match(egl, display, *cfg))
            return *cfg;
    }

    TCU_THROW(NotSupportedError, "No matching EGL config found");
}

EGLConfig chooseSingleConfig(const Library &egl, EGLDisplay display, const EGLint *attribList)
{
    const vector<EGLConfig> configs(chooseConfigs(egl, display, attribList));
    if (configs.empty())
        TCU_THROW(NotSupportedError, "No matching EGL config found");

    return configs.front();
}

vector<EGLConfig> chooseConfigs(const Library &egl, EGLDisplay display, const AttribMap &attribs)
{
    const vector<EGLint> attribList = attribMapToList(attribs);
    return chooseConfigs(egl, display, &attribList.front());
}

EGLConfig chooseSingleConfig(const Library &egl, EGLDisplay display, const AttribMap &attribs)
{
    const vector<EGLint> attribList = attribMapToList(attribs);
    return chooseSingleConfig(egl, display, &attribList.front());
}

EGLConfig chooseConfigByID(const Library &egl, EGLDisplay display, EGLint id)
{
    AttribMap attribs;

    attribs[EGL_CONFIG_ID]         = id;
    attribs[EGL_TRANSPARENT_TYPE]  = EGL_DONT_CARE;
    attribs[EGL_COLOR_BUFFER_TYPE] = EGL_DONT_CARE;
    attribs[EGL_RENDERABLE_TYPE]   = EGL_DONT_CARE;
    attribs[EGL_SURFACE_TYPE]      = EGL_DONT_CARE;

    return chooseSingleConfig(egl, display, attribs);
}

EGLint getConfigAttribInt(const Library &egl, EGLDisplay display, EGLConfig config, EGLint attrib)
{
    EGLint value = 0;
    EGLU_CHECK_CALL(egl, getConfigAttrib(display, config, attrib, &value));
    return value;
}

EGLint getConfigID(const Library &egl, EGLDisplay display, EGLConfig config)
{
    return getConfigAttribInt(egl, display, config, EGL_CONFIG_ID);
}

EGLint querySurfaceInt(const Library &egl, EGLDisplay display, EGLSurface surface, EGLint attrib)
{
    EGLint value = 0;
    EGLU_CHECK_CALL(egl, querySurface(display, surface, attrib, &value));
    return value;
}

tcu::IVec2 getSurfaceSize(const Library &egl, EGLDisplay display, EGLSurface surface)
{
    const EGLint width  = querySurfaceInt(egl, display, surface, EGL_WIDTH);
    const EGLint height = querySurfaceInt(egl, display, surface, EGL_HEIGHT);
    return tcu::IVec2(width, height);
}

tcu::IVec2 getSurfaceResolution(const Library &egl, EGLDisplay display, EGLSurface surface)
{
    const EGLint hRes = querySurfaceInt(egl, display, surface, EGL_HORIZONTAL_RESOLUTION);
    const EGLint vRes = querySurfaceInt(egl, display, surface, EGL_VERTICAL_RESOLUTION);

    if (hRes == EGL_UNKNOWN || vRes == EGL_UNKNOWN)
        TCU_THROW(NotSupportedError, "Surface doesn't support pixel density queries");
    return tcu::IVec2(hRes, vRes);
}

//! Get EGLdisplay using eglGetDisplay() or eglGetPlatformDisplayEXT()
EGLDisplay getDisplay(NativeDisplay &nativeDisplay)
{
    const Library &egl = nativeDisplay.getLibrary();
    const bool supportsLegacyGetDisplay =
        (nativeDisplay.getCapabilities() & NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY) != 0;
    const bool supportsPlatformGetDisplay =
        (nativeDisplay.getCapabilities() & NativeDisplay::CAPABILITY_GET_DISPLAY_PLATFORM) != 0;
    const bool supportsPlatformGetDisplayEXT =
        (nativeDisplay.getCapabilities() & NativeDisplay::CAPABILITY_GET_DISPLAY_PLATFORM_EXT) != 0;
    bool usePlatformExt = false;
    EGLDisplay display  = EGL_NO_DISPLAY;

    TCU_CHECK_INTERNAL(supportsLegacyGetDisplay || supportsPlatformGetDisplay);

    if (supportsPlatformGetDisplayEXT)
    {
        const vector<string> platformExts = getClientExtensions(egl);
        usePlatformExt                    = de::contains(platformExts.begin(), platformExts.end(),
                                      string("EGL_EXT_platform_base")) &&
                         de::contains(platformExts.begin(), platformExts.end(),
                                      string(nativeDisplay.getPlatformExtensionName()));
    }

    if (supportsPlatformGetDisplay)
    {
        display = egl.getPlatformDisplay(nativeDisplay.getPlatformType(),
                                         nativeDisplay.getPlatformNative(),
                                         nativeDisplay.getPlatformAttributes());
        EGLU_CHECK_MSG(egl, "eglGetPlatformDisplay()");
        TCU_CHECK(display != EGL_NO_DISPLAY);
    }
    else if (usePlatformExt)
    {
        const vector<EGLint> legacyAttribs =
            toLegacyAttribList(nativeDisplay.getPlatformAttributes());

        display = egl.getPlatformDisplayEXT(nativeDisplay.getPlatformType(),
                                            nativeDisplay.getPlatformNative(), &legacyAttribs[0]);
        EGLU_CHECK_MSG(egl, "eglGetPlatformDisplayEXT()");
        TCU_CHECK(display != EGL_NO_DISPLAY);
    }
    else if (supportsLegacyGetDisplay)
    {
        display = egl.getDisplay(nativeDisplay.getLegacyNative());
        EGLU_CHECK_MSG(egl, "eglGetDisplay()");
        TCU_CHECK(display != EGL_NO_DISPLAY);
    }
    else
        throw tcu::InternalError("No supported way to get EGL display", DE_NULL, __FILE__,
                                 __LINE__);

    DE_ASSERT(display != EGL_NO_DISPLAY);
    return display;
}

EGLDisplay getAndInitDisplay(NativeDisplay &nativeDisplay, Version *version)
{
    const Library &egl = nativeDisplay.getLibrary();
    EGLDisplay display = getDisplay(nativeDisplay);
    int major, minor;

    EGLU_CHECK_CALL(egl, initialize(display, &major, &minor));

    if (version)
        *version = Version(major, minor);

    return display;
}

//! Create EGL window surface using eglCreateWindowSurface() or eglCreatePlatformWindowSurfaceEXT()
EGLSurface createWindowSurface(NativeDisplay &nativeDisplay,
                               NativeWindow &window,
                               EGLDisplay display,
                               EGLConfig config,
                               const EGLAttrib *attribList)
{
    const Library &egl = nativeDisplay.getLibrary();
    const bool supportsLegacyCreate =
        (window.getCapabilities() & NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY) != 0;
    const bool supportsPlatformCreate =
        (window.getCapabilities() & NativeWindow::CAPABILITY_CREATE_SURFACE_PLATFORM) != 0;
    bool usePlatformExt = false;
    EGLSurface surface  = EGL_NO_SURFACE;

    TCU_CHECK_INTERNAL(supportsLegacyCreate || supportsPlatformCreate);

    if (supportsPlatformCreate)
    {
        const vector<string> platformExts = getClientExtensions(egl);
        usePlatformExt                    = de::contains(platformExts.begin(), platformExts.end(),
                                      string("EGL_EXT_platform_base")) &&
                         de::contains(platformExts.begin(), platformExts.end(),
                                      string(nativeDisplay.getPlatformExtensionName()));
    }

    // \todo [2014-03-13 pyry] EGL 1.5 core support
    if (usePlatformExt)
    {
        const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);

        surface = egl.createPlatformWindowSurfaceEXT(display, config, window.getPlatformNative(),
                                                     &legacyAttribs[0]);
        EGLU_CHECK_MSG(egl, "eglCreatePlatformWindowSurfaceEXT()");
        TCU_CHECK(surface != EGL_NO_SURFACE);
    }
    else if (supportsLegacyCreate)
    {
        const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);
        surface =
            egl.createWindowSurface(display, config, window.getLegacyNative(), &legacyAttribs[0]);
        EGLU_CHECK_MSG(egl, "eglCreateWindowSurface()");
        TCU_CHECK(surface != EGL_NO_SURFACE);
    }
    else
        throw tcu::InternalError("No supported way to create EGL window surface", DE_NULL, __FILE__,
                                 __LINE__);

    DE_ASSERT(surface != EGL_NO_SURFACE);
    return surface;
}

//! Create EGL pixmap surface using eglCreatePixmapSurface() or eglCreatePlatformPixmapSurfaceEXT()
EGLSurface createPixmapSurface(NativeDisplay &nativeDisplay,
                               NativePixmap &pixmap,
                               EGLDisplay display,
                               EGLConfig config,
                               const EGLAttrib *attribList)
{
    const Library &egl = nativeDisplay.getLibrary();
    const bool supportsLegacyCreate =
        (pixmap.getCapabilities() & NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY) != 0;
    const bool supportsPlatformCreate =
        (pixmap.getCapabilities() & NativePixmap::CAPABILITY_CREATE_SURFACE_PLATFORM) != 0;
    bool usePlatformExt = false;
    EGLSurface surface  = EGL_NO_SURFACE;

    TCU_CHECK_INTERNAL(supportsLegacyCreate || supportsPlatformCreate);

    if (supportsPlatformCreate)
    {
        const vector<string> platformExts = getClientExtensions(egl);
        usePlatformExt                    = de::contains(platformExts.begin(), platformExts.end(),
                                      string("EGL_EXT_platform_base")) &&
                         de::contains(platformExts.begin(), platformExts.end(),
                                      string(nativeDisplay.getPlatformExtensionName()));
    }

    if (usePlatformExt)
    {
        const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);

        surface = egl.createPlatformPixmapSurfaceEXT(display, config, pixmap.getPlatformNative(),
                                                     &legacyAttribs[0]);
        EGLU_CHECK_MSG(egl, "eglCreatePlatformPixmapSurfaceEXT()");
        TCU_CHECK(surface != EGL_NO_SURFACE);
    }
    else if (supportsLegacyCreate)
    {
        const vector<EGLint> legacyAttribs = toLegacyAttribList(attribList);
        surface =
            egl.createPixmapSurface(display, config, pixmap.getLegacyNative(), &legacyAttribs[0]);
        EGLU_CHECK_MSG(egl, "eglCreatePixmapSurface()");
        TCU_CHECK(surface != EGL_NO_SURFACE);
    }
    else
        throw tcu::InternalError("No supported way to create EGL pixmap surface", DE_NULL, __FILE__,
                                 __LINE__);

    DE_ASSERT(surface != EGL_NO_SURFACE);
    return surface;
}

static WindowParams::Visibility getWindowVisibility(tcu::WindowVisibility visibility)
{
    switch (visibility)
    {
        case tcu::WINDOWVISIBILITY_WINDOWED:
            return WindowParams::VISIBILITY_VISIBLE;
        case tcu::WINDOWVISIBILITY_FULLSCREEN:
            return WindowParams::VISIBILITY_FULLSCREEN;
        case tcu::WINDOWVISIBILITY_HIDDEN:
            return WindowParams::VISIBILITY_HIDDEN;

        default:
            DE_ASSERT(false);
            return WindowParams::VISIBILITY_DONT_CARE;
    }
}

WindowParams::Visibility parseWindowVisibility(const tcu::CommandLine &commandLine)
{
    return getWindowVisibility(commandLine.getVisibility());
}

EGLenum parseClientAPI(const std::string &api)
{
    if (api == "OpenGL")
        return EGL_OPENGL_API;
    else if (api == "OpenGL_ES")
        return EGL_OPENGL_ES_API;
    else if (api == "OpenVG")
        return EGL_OPENVG_API;
    else
        throw tcu::InternalError("Unknown EGL client API '" + api + "'");
}

vector<EGLenum> parseClientAPIs(const std::string &apiList)
{
    const vector<string> apiStrs = de::splitString(apiList, ' ');
    vector<EGLenum> apis;

    for (vector<string>::const_iterator api = apiStrs.begin(); api != apiStrs.end(); ++api)
        apis.push_back(parseClientAPI(*api));

    return apis;
}

vector<EGLenum> getClientAPIs(const eglw::Library &egl, eglw::EGLDisplay display)
{
    return parseClientAPIs(egl.queryString(display, EGL_CLIENT_APIS));
}

EGLint getRenderableAPIsMask(const eglw::Library &egl, eglw::EGLDisplay display)
{
    const vector<EGLConfig> configs = getConfigs(egl, display);
    EGLint allAPIs                  = 0;

    for (vector<EGLConfig>::const_iterator i = configs.begin(); i != configs.end(); ++i)
        allAPIs |= getConfigAttribInt(egl, display, *i, EGL_RENDERABLE_TYPE);

    return allAPIs;
}

vector<EGLint> toLegacyAttribList(const EGLAttrib *attribs)
{
    const deUint64 attribMask = 0xffffffffull;  //!< Max bits that can be used
    vector<EGLint> legacyAttribs;

    if (attribs)
    {
        for (const EGLAttrib *attrib = attribs; *attrib != EGL_NONE; attrib += 2)
        {
            if ((attrib[0] & ~attribMask) || (attrib[1] & ~attribMask))
                throw tcu::InternalError("Failed to translate EGLAttrib to EGLint", DE_NULL,
                                         __FILE__, __LINE__);

            legacyAttribs.push_back((EGLint)attrib[0]);
            legacyAttribs.push_back((EGLint)attrib[1]);
        }
    }

    legacyAttribs.push_back(EGL_NONE);

    return legacyAttribs;
}

template <typename Factory>
static const Factory &selectFactory(const tcu::FactoryRegistry<Factory> &registry,
                                    const char *objectTypeName,
                                    const char *cmdLineArg)
{
    if (cmdLineArg)
    {
        const Factory *factory = registry.getFactoryByName(cmdLineArg);

        if (factory)
            return *factory;
        else
        {
            tcu::print("ERROR: Unknown or unsupported EGL %s type '%s'", objectTypeName,
                       cmdLineArg);
            tcu::print("Available EGL %s types:\n", objectTypeName);
            for (size_t ndx = 0; ndx < registry.getFactoryCount(); ndx++)
                tcu::print("  %s: %s\n", registry.getFactoryByIndex(ndx)->getName(),
                           registry.getFactoryByIndex(ndx)->getDescription());

            TCU_THROW(NotSupportedError, (string("Unsupported or unknown EGL ") + objectTypeName +
                                          " type '" + cmdLineArg + "'")
                                             .c_str());
        }
    }
    else if (!registry.empty())
        return *registry.getDefaultFactory();
    else
        TCU_THROW(NotSupportedError,
                  (string("No factory supporting EGL '") + objectTypeName + "' type").c_str());
}

const NativeDisplayFactory &selectNativeDisplayFactory(const NativeDisplayFactoryRegistry &registry,
                                                       const tcu::CommandLine &cmdLine)
{
    return selectFactory(registry, "display", cmdLine.getEGLDisplayType());
}

const NativeWindowFactory &selectNativeWindowFactory(const NativeDisplayFactory &factory,
                                                     const tcu::CommandLine &cmdLine)
{
    return selectFactory(factory.getNativeWindowRegistry(), "window", cmdLine.getEGLWindowType());
}

const NativePixmapFactory &selectNativePixmapFactory(const NativeDisplayFactory &factory,
                                                     const tcu::CommandLine &cmdLine)
{
    return selectFactory(factory.getNativePixmapRegistry(), "pixmap", cmdLine.getEGLPixmapType());
}

}  // namespace eglu
