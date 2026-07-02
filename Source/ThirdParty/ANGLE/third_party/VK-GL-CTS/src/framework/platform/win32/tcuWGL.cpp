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
 *//*!
 * \file
 * \brief WGL Utilities.
 *//*--------------------------------------------------------------------*/

#include "tcuWGL.hpp"
#include "tcuWin32Window.hpp"
#include "deDynamicLibrary.hpp"
#include "deMemory.h"
#include "deStringUtil.hpp"
#include "tcuFormatUtil.hpp"
#include "gluRenderConfig.hpp"
#include "glwEnums.hpp"

#include <map>
#include <sstream>
#include <iterator>
#include <set>

#include <wingdi.h>

// WGL_ARB_pixel_format
#define WGL_NUMBER_PIXEL_FORMATS_ARB 0x2000
#define WGL_DRAW_TO_WINDOW_ARB 0x2001
#define WGL_DRAW_TO_BITMAP_ARB 0x2002
#define WGL_ACCELERATION_ARB 0x2003
#define WGL_NEED_PALETTE_ARB 0x2004
#define WGL_NEED_SYSTEM_PALETTE_ARB 0x2005
#define WGL_SWAP_LAYER_BUFFERS_ARB 0x2006
#define WGL_SWAP_METHOD_ARB 0x2007
#define WGL_NUMBER_OVERLAYS_ARB 0x2008
#define WGL_NUMBER_UNDERLAYS_ARB 0x2009
#define WGL_TRANSPARENT_ARB 0x200A
#define WGL_TRANSPARENT_RED_VALUE_ARB 0x2037
#define WGL_TRANSPARENT_GREEN_VALUE_ARB 0x2038
#define WGL_TRANSPARENT_BLUE_VALUE_ARB 0x2039
#define WGL_TRANSPARENT_ALPHA_VALUE_ARB 0x203A
#define WGL_TRANSPARENT_INDEX_VALUE_ARB 0x203B
#define WGL_SHARE_DEPTH_ARB 0x200C
#define WGL_SHARE_STENCIL_ARB 0x200D
#define WGL_SHARE_ACCUM_ARB 0x200E
#define WGL_SUPPORT_GDI_ARB 0x200F
#define WGL_SUPPORT_OPENGL_ARB 0x2010
#define WGL_DOUBLE_BUFFER_ARB 0x2011
#define WGL_STEREO_ARB 0x2012
#define WGL_PIXEL_TYPE_ARB 0x2013
#define WGL_COLOR_BITS_ARB 0x2014
#define WGL_RED_BITS_ARB 0x2015
#define WGL_RED_SHIFT_ARB 0x2016
#define WGL_GREEN_BITS_ARB 0x2017
#define WGL_GREEN_SHIFT_ARB 0x2018
#define WGL_BLUE_BITS_ARB 0x2019
#define WGL_BLUE_SHIFT_ARB 0x201A
#define WGL_ALPHA_BITS_ARB 0x201B
#define WGL_ALPHA_SHIFT_ARB 0x201C
#define WGL_ACCUM_BITS_ARB 0x201D
#define WGL_ACCUM_RED_BITS_ARB 0x201E
#define WGL_ACCUM_GREEN_BITS_ARB 0x201F
#define WGL_ACCUM_BLUE_BITS_ARB 0x2020
#define WGL_ACCUM_ALPHA_BITS_ARB 0x2021
#define WGL_DEPTH_BITS_ARB 0x2022
#define WGL_STENCIL_BITS_ARB 0x2023
#define WGL_AUX_BUFFERS_ARB 0x2024

#define WGL_NO_ACCELERATION_ARB 0x2025
#define WGL_GENERIC_ACCELERATION_ARB 0x2026
#define WGL_FULL_ACCELERATION_ARB 0x2027

#define WGL_TYPE_RGBA_ARB 0x202B
#define WGL_TYPE_COLORINDEX_ARB 0x202C

// WGL_ARB_color_buffer_float
#define WGL_TYPE_RGBA_FLOAT_ARB 0x21A0

// WGL_EXT_pixel_type_packed_float
#define WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT 0x20A8

// WGL_ARB_multisample
#define WGL_SAMPLE_BUFFERS_ARB 0x2041
#define WGL_SAMPLES_ARB 0x2042

// WGL_EXT_colorspace
#define WGL_COLORSPACE_EXT 0x309D
#define WGL_COLORSPACE_SRGB_EXT 0x3089
#define WGL_COLORSPACE_LINEAR_EXT 0x308A

// WGL_ARB_create_context
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB 0x2093
#define WGL_CONTEXT_FLAGS_ARB 0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
#define WGL_CONTEXT_DEBUG_BIT_ARB 0x0001
#define WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB 0x0002
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002
#define WGL_CONTEXT_ES_PROFILE_BIT_EXT 0x00000004

// WGL_ARB_create_context_robustness
#define WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB 0x0004
#define WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB 0x8256
#define WGL_NO_RESET_NOTIFICATION_ARB 0x8261
#define WGL_LOSE_CONTEXT_ON_RESET_ARB 0x8252

// WGL ARB_create_context_no_error
#define WGL_CONTEXT_OPENGL_NO_ERROR_ARB 0x31B3

DE_BEGIN_EXTERN_C

// WGL core
typedef HGLRC(WINAPI *wglCreateContextFunc)(HDC hdc);
typedef BOOL(WINAPI *wglDeleteContextFunc)(HGLRC hglrc);
typedef BOOL(WINAPI *wglMakeCurrentFunc)(HDC hdc, HGLRC hglrc);
typedef PROC(WINAPI *wglGetProcAddressFunc)(LPCSTR lpszProc);
typedef BOOL(WINAPI *wglSwapLayerBuffersFunc)(HDC dhc, UINT fuPlanes);

// WGL_ARB_pixel_format
typedef BOOL(WINAPI *wglGetPixelFormatAttribivARBFunc)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes,
                                                       const int *piAttributes, int *piValues);
typedef BOOL(WINAPI *wglGetPixelFormatAttribfvARBFunc)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes,
                                                       const int *piAttributes, FLOAT *pfValues);
typedef BOOL(WINAPI *wglChoosePixelFormatARBFunc)(HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList,
                                                  UINT nMaxFormats, int *piFormats, UINT *nNumFormats);

// WGL_ARB_create_context
typedef HGLRC(WINAPI *wglCreateContextAttribsARBFunc)(HDC hdc, HGLRC hshareContext, const int *attribList);
typedef const char *(WINAPI *wglGetExtensionsStringARBFunc)(HDC hdc);

// WGL_EXT_swap_control
typedef BOOL(WINAPI *wglSwapIntervalEXTFunc)(int interval);

DE_END_EXTERN_C

namespace tcu
{
namespace wgl
{

// Functions

struct Functions
{
    // Core
    wglCreateContextFunc createContext;
    wglDeleteContextFunc deleteContext;
    wglMakeCurrentFunc makeCurrent;
    wglGetProcAddressFunc getProcAddress;
    wglSwapLayerBuffersFunc swapLayerBuffers;

    // WGL_ARB_pixel_format
    wglGetPixelFormatAttribivARBFunc getPixelFormatAttribivARB;
    wglGetPixelFormatAttribfvARBFunc getPixelFormatAttribfvARB;
    wglChoosePixelFormatARBFunc choosePixelFormatARB;

    // WGL_ARB_create_context
    wglCreateContextAttribsARBFunc createContextAttribsARB;
    wglGetExtensionsStringARBFunc getExtensionsStringARB;

    // WGL_EXT_swap_control
    wglSwapIntervalEXTFunc swapIntervalEXT;

    Functions(void)
        : createContext(nullptr)
        , deleteContext(nullptr)
        , makeCurrent(nullptr)
        , getProcAddress(nullptr)
        , swapLayerBuffers(nullptr)
        , getPixelFormatAttribivARB(nullptr)
        , getPixelFormatAttribfvARB(nullptr)
        , choosePixelFormatARB(nullptr)
        , createContextAttribsARB(nullptr)
        , getExtensionsStringARB(nullptr)
    {
    }
};

// Library

class Library
{
public:
    Library(HINSTANCE instance);
    ~Library(void);

    const Functions &getFunctions(void) const
    {
        return m_functions;
    }
    const de::DynamicLibrary &getGLLibrary(void) const
    {
        return m_library;
    }
    bool isWglExtensionSupported(const char *extName) const;

private:
    de::DynamicLibrary m_library;
    Functions m_functions;
    std::set<std::string> m_extensions;
};

Library::Library(HINSTANCE instance) : m_library("opengl32.dll")
{
    // Temporary 1x1 window for creating context
    win32::Window tmpWindow(instance, 1, 1);

    // Load WGL core.
    m_functions.createContext    = (wglCreateContextFunc)m_library.getFunction("wglCreateContext");
    m_functions.deleteContext    = (wglDeleteContextFunc)m_library.getFunction("wglDeleteContext");
    m_functions.getProcAddress   = (wglGetProcAddressFunc)m_library.getFunction("wglGetProcAddress");
    m_functions.makeCurrent      = (wglMakeCurrentFunc)m_library.getFunction("wglMakeCurrent");
    m_functions.swapLayerBuffers = (wglSwapLayerBuffersFunc)m_library.getFunction("wglSwapLayerBuffers");

    if (!m_functions.createContext || !m_functions.deleteContext || !m_functions.getProcAddress ||
        !m_functions.makeCurrent || !m_functions.swapLayerBuffers)
        throw ResourceError("Failed to load core WGL functions");

    {
        PIXELFORMATDESCRIPTOR pixelFormatDesc;
        deMemset(&pixelFormatDesc, 0, sizeof(pixelFormatDesc));

        pixelFormatDesc.nSize      = sizeof(pixelFormatDesc);
        pixelFormatDesc.nVersion   = 1;
        pixelFormatDesc.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pixelFormatDesc.iPixelType = PFD_TYPE_RGBA;
        pixelFormatDesc.iLayerType = PFD_MAIN_PLANE;

        int pixelFormat = ChoosePixelFormat(tmpWindow.getDeviceContext(), &pixelFormatDesc);
        if (!SetPixelFormat(tmpWindow.getDeviceContext(), pixelFormat, &pixelFormatDesc))
            throw ResourceError("Failed to set pixel format for temporary context creation");
    }

    // Create temporary context for loading extension functions.
    HGLRC tmpCtx = m_functions.createContext(tmpWindow.getDeviceContext());
    if (!tmpCtx || !m_functions.makeCurrent(tmpWindow.getDeviceContext(), tmpCtx))
    {
        if (tmpCtx)
            m_functions.deleteContext(tmpCtx);
        throw ResourceError("Failed to create temporary WGL context");
    }

    // WGL_ARB_pixel_format
    m_functions.getPixelFormatAttribivARB =
        (wglGetPixelFormatAttribivARBFunc)m_functions.getProcAddress("wglGetPixelFormatAttribivARB");
    m_functions.getPixelFormatAttribfvARB =
        (wglGetPixelFormatAttribfvARBFunc)m_functions.getProcAddress("wglGetPixelFormatAttribfvARB");
    m_functions.choosePixelFormatARB =
        (wglChoosePixelFormatARBFunc)m_functions.getProcAddress("wglChoosePixelFormatARB");

    // WGL_ARB_create_context
    m_functions.createContextAttribsARB =
        (wglCreateContextAttribsARBFunc)m_functions.getProcAddress("wglCreateContextAttribsARB");
    m_functions.getExtensionsStringARB =
        (wglGetExtensionsStringARBFunc)m_functions.getProcAddress("wglGetExtensionsStringARB");

    // WGL_EXT_swap_control
    m_functions.swapIntervalEXT = (wglSwapIntervalEXTFunc)m_functions.getProcAddress("wglSwapIntervalEXT");

    m_functions.makeCurrent(tmpWindow.getDeviceContext(), NULL);
    m_functions.deleteContext(tmpCtx);

    if (!m_functions.getPixelFormatAttribivARB || !m_functions.getPixelFormatAttribfvARB ||
        !m_functions.choosePixelFormatARB || !m_functions.createContextAttribsARB ||
        !m_functions.getExtensionsStringARB)
        throw ResourceError("Failed to load WGL extension functions");

    const char *extensions = m_functions.getExtensionsStringARB(tmpWindow.getDeviceContext());
    std::istringstream extStream(extensions);
    m_extensions =
        std::set<std::string>(std::istream_iterator<std::string>(extStream), std::istream_iterator<std::string>());
}

Library::~Library(void)
{
}

bool Library::isWglExtensionSupported(const char *extName) const
{
    return m_extensions.find(extName) != m_extensions.end();
}

// Core

Core::Core(HINSTANCE instance) : m_library(new Library(instance))
{
}

Core::~Core(void)
{
    delete m_library;
}

std::vector<int> Core::getPixelFormats(HDC deviceCtx) const
{
    const Functions &wgl = m_library->getFunctions();

    int attribs[] = {WGL_NUMBER_PIXEL_FORMATS_ARB};
    int values[DE_LENGTH_OF_ARRAY(attribs)];

    if (!wgl.getPixelFormatAttribivARB(deviceCtx, 0, 0, DE_LENGTH_OF_ARRAY(attribs), &attribs[0], &values[0]))
        TCU_THROW(ResourceError, "Failed to query number of WGL pixel formats");

    std::vector<int> pixelFormats(values[0]);
    for (int i = 0; i < values[0]; i++)
        pixelFormats[i] = i + 1;

    return pixelFormats;
}

static PixelFormatInfo::Acceleration translateAcceleration(int accel)
{
    switch (accel)
    {
    case WGL_NO_ACCELERATION_ARB:
        return PixelFormatInfo::ACCELERATION_NONE;
    case WGL_GENERIC_ACCELERATION_ARB:
        return PixelFormatInfo::ACCELERATION_GENERIC;
    case WGL_FULL_ACCELERATION_ARB:
        return PixelFormatInfo::ACCELERATION_FULL;
    default:
        return PixelFormatInfo::ACCELERATION_UNKNOWN;
    }
}

static PixelFormatInfo::PixelType translatePixelType(int type)
{
    switch (type)
    {
    case WGL_TYPE_RGBA_ARB:
        return PixelFormatInfo::PIXELTYPE_RGBA;
    case WGL_TYPE_RGBA_FLOAT_ARB:
        return PixelFormatInfo::PIXELTYPE_RGBA_FLOAT;
    case WGL_TYPE_RGBA_UNSIGNED_FLOAT_EXT:
        return PixelFormatInfo::PIXELTYPE_RGBA_UNSIGNED_FLOAT;
    case WGL_TYPE_COLORINDEX_ARB:
        return PixelFormatInfo::PIXELTYPE_COLOR_INDEX;
    default:
        return PixelFormatInfo::PIXELTYPE_UNKNOWN;
    }
}

static void getPixelFormatAttribs(const Functions &wgl, HDC deviceCtx, int pixelFormat, int numAttribs,
                                  const int *attribs, std::map<int, int> *dst)
{
    std::vector<int> values(numAttribs);

    if (!wgl.getPixelFormatAttribivARB(deviceCtx, pixelFormat, 0, numAttribs, &attribs[0], &values[0]))
        TCU_THROW(ResourceError, "Pixel format query failed");

    for (int ndx = 0; ndx < numAttribs; ++ndx)
        (*dst)[attribs[ndx]] = values[ndx];
}

PixelFormatInfo Core::getPixelFormatInfo(HDC deviceCtx, int pixelFormat) const
{
    std::vector<int> s_attribsToQuery{
        WGL_DRAW_TO_WINDOW_ARB,   WGL_DRAW_TO_BITMAP_ARB,      WGL_ACCELERATION_ARB,
        WGL_NEED_PALETTE_ARB,     WGL_NEED_SYSTEM_PALETTE_ARB, WGL_NUMBER_OVERLAYS_ARB,
        WGL_NUMBER_UNDERLAYS_ARB, WGL_SUPPORT_OPENGL_ARB,      WGL_DOUBLE_BUFFER_ARB,
        WGL_STEREO_ARB,           WGL_PIXEL_TYPE_ARB,          WGL_RED_BITS_ARB,
        WGL_GREEN_BITS_ARB,       WGL_BLUE_BITS_ARB,           WGL_ALPHA_BITS_ARB,
        WGL_ACCUM_BITS_ARB,       WGL_DEPTH_BITS_ARB,          WGL_STENCIL_BITS_ARB,
        WGL_AUX_BUFFERS_ARB,      WGL_SAMPLE_BUFFERS_ARB,      WGL_SAMPLES_ARB,
    };
    if (getLibrary()->isWglExtensionSupported("WGL_EXT_colorspace"))
        s_attribsToQuery.push_back(WGL_COLORSPACE_EXT);

    const Functions &wgl = m_library->getFunctions();
    std::map<int, int> values;

    getPixelFormatAttribs(wgl, deviceCtx, pixelFormat, static_cast<int>(s_attribsToQuery.size()),
                          s_attribsToQuery.data(), &values);

    // Translate values.
    PixelFormatInfo info;

    info.pixelFormat = pixelFormat;
    info.surfaceTypes |= (values[WGL_DRAW_TO_WINDOW_ARB] ? PixelFormatInfo::SURFACE_WINDOW : 0);
    info.surfaceTypes |= (values[WGL_DRAW_TO_BITMAP_ARB] ? PixelFormatInfo::SURFACE_PIXMAP : 0);
    info.acceleration      = translateAcceleration(values[WGL_ACCELERATION_ARB]);
    info.needPalette       = values[WGL_NEED_PALETTE_ARB] != 0;
    info.needSystemPalette = values[WGL_NEED_SYSTEM_PALETTE_ARB] != 0;
    info.numOverlays       = values[WGL_NUMBER_OVERLAYS_ARB] != 0;
    info.numUnderlays      = values[WGL_NUMBER_UNDERLAYS_ARB] != 0;
    info.supportOpenGL     = values[WGL_SUPPORT_OPENGL_ARB] != 0;
    info.doubleBuffer      = values[WGL_DOUBLE_BUFFER_ARB] != 0;
    info.stereo            = values[WGL_STEREO_ARB] != 0;
    info.pixelType         = translatePixelType(values[WGL_PIXEL_TYPE_ARB]);
    info.redBits           = values[WGL_RED_BITS_ARB];
    info.greenBits         = values[WGL_GREEN_BITS_ARB];
    info.blueBits          = values[WGL_BLUE_BITS_ARB];
    info.alphaBits         = values[WGL_ALPHA_BITS_ARB];
    info.accumBits         = values[WGL_ACCUM_BITS_ARB];
    info.depthBits         = values[WGL_DEPTH_BITS_ARB];
    info.stencilBits       = values[WGL_STENCIL_BITS_ARB];
    info.numAuxBuffers     = values[WGL_AUX_BUFFERS_ARB];
    info.sampleBuffers     = values[WGL_SAMPLE_BUFFERS_ARB];
    info.samples           = values[WGL_SAMPLES_ARB];
    info.sRGB              = (getLibrary()->isWglExtensionSupported("WGL_EXT_colorspace")) ?
                                 (values[WGL_COLORSPACE_EXT] == WGL_COLORSPACE_SRGB_EXT) :
                                 false;

    return info;
}

// Context

Context::Context(const Core *core, HDC deviceCtx, const Context *sharedContext, glu::ContextType ctxType,
                 int pixelFormat, glu::ResetNotificationStrategy resetNotificationStrategy)
    : m_core(core)
    , m_deviceCtx(deviceCtx)
    , m_context(0)
{
    const Functions &wgl = core->getLibrary()->getFunctions();
    std::vector<int> attribList;

    // Context version and profile
    {
        int profileBit  = 0;
        HGLRC sharedCtx = nullptr;
        int minor       = ctxType.getMinorVersion();
        int major       = ctxType.getMajorVersion();

        switch (ctxType.getProfile())
        {
        case glu::PROFILE_CORE:
            profileBit = WGL_CONTEXT_CORE_PROFILE_BIT_ARB;
            if (major == 3 && minor < 3)
                minor = 3;
            break;

        case glu::PROFILE_ES:
            profileBit = WGL_CONTEXT_ES_PROFILE_BIT_EXT;
            break;

        case glu::PROFILE_COMPATIBILITY:
            profileBit = WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB;
            break;

        default:
            TCU_THROW(NotSupportedError, "Unsupported context type for WGL");
        }

        attribList.push_back(WGL_CONTEXT_MAJOR_VERSION_ARB);
        attribList.push_back(major);
        attribList.push_back(WGL_CONTEXT_MINOR_VERSION_ARB);
        attribList.push_back(minor);
        attribList.push_back(WGL_CONTEXT_PROFILE_MASK_ARB);
        attribList.push_back(profileBit);
    }

    // Context flags
    {
        int flags = 0;

        if ((ctxType.getFlags() & glu::CONTEXT_FORWARD_COMPATIBLE) != 0)
        {
            if (glu::isContextTypeES(ctxType))
                TCU_THROW(InternalError, "Only OpenGL core contexts can be forward-compatible");

            flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
        }

        if ((ctxType.getFlags() & glu::CONTEXT_DEBUG) != 0)
            flags |= WGL_CONTEXT_DEBUG_BIT_ARB;

        if ((ctxType.getFlags() & glu::CONTEXT_ROBUST) != 0)
            flags |= WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB;

        if ((ctxType.getFlags() & glu::CONTEXT_NO_ERROR) != 0)
        {
            if (core->getLibrary()->isWglExtensionSupported("WGL_ARB_create_context_no_error"))
            {
                attribList.push_back(WGL_CONTEXT_OPENGL_NO_ERROR_ARB);
                attribList.push_back(1);
            }
            else
                TCU_THROW(NotSupportedError,
                          "WGL_ARB_create_context_no_error is required for creating no-error contexts");
        }

        if (flags != 0)
        {
            attribList.push_back(WGL_CONTEXT_FLAGS_ARB);
            attribList.push_back(flags);
        }
    }

    if (resetNotificationStrategy != glu::RESET_NOTIFICATION_STRATEGY_NOT_SPECIFIED)
    {
        attribList.push_back(WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB);

        if (resetNotificationStrategy == glu::RESET_NOTIFICATION_STRATEGY_NO_RESET_NOTIFICATION)
            attribList.push_back(WGL_NO_RESET_NOTIFICATION_ARB);
        else if (resetNotificationStrategy == glu::RESET_NOTIFICATION_STRATEGY_LOSE_CONTEXT_ON_RESET)
            attribList.push_back(WGL_LOSE_CONTEXT_ON_RESET_ARB);
        else
            TCU_THROW(InternalError, "Unknown reset notification strategy");
    }

    // Set pixel format
    {
        PIXELFORMATDESCRIPTOR pixelFormatDesc;
        deMemset(&pixelFormatDesc, 0, sizeof(pixelFormatDesc));

        if (!DescribePixelFormat(deviceCtx, pixelFormat, sizeof(pixelFormatDesc), &pixelFormatDesc))
            throw ResourceError("DescribePixelFormat() failed");

        if (!SetPixelFormat(deviceCtx, pixelFormat, &pixelFormatDesc))
            throw ResourceError("Failed to set pixel format");
    }

    HGLRC sharedCtx = nullptr;
    if (nullptr != sharedContext)
        sharedCtx = sharedContext->m_context;

    // Terminate attribList
    attribList.push_back(0);

    // Create context
    m_context = wgl.createContextAttribsARB(deviceCtx, sharedCtx, &attribList[0]);

    if (!m_context)
        TCU_THROW(ResourceError, "Failed to create WGL context");

    if (!wgl.makeCurrent(deviceCtx, m_context))
    {
        wgl.deleteContext(m_context);
        TCU_THROW(ResourceError, "wglMakeCurrent() failed");
    }

    if (core->getLibrary()->isWglExtensionSupported("WGL_EXT_swap_control"))
        core->getLibrary()->getFunctions().swapIntervalEXT(0);
}

Context::~Context(void)
{
    const Functions &wgl = m_core->getLibrary()->getFunctions();

    wgl.makeCurrent(m_deviceCtx, NULL);
    wgl.deleteContext(m_context);
}

FunctionPtr Context::getGLFunction(const char *name) const
{
    FunctionPtr ptr = nullptr;

    // Try first with wglGeProcAddress()
    ptr = (FunctionPtr)m_core->getLibrary()->getFunctions().getProcAddress(name);

    // Fall-back to dynlib
    if (!ptr)
        ptr = (FunctionPtr)m_core->getLibrary()->getGLLibrary().getFunction(name);

    return ptr;
}

void Context::makeCurrent(void)
{
    const Functions &wgl = m_core->getLibrary()->getFunctions();
    if (!wgl.makeCurrent(m_deviceCtx, m_context))
        TCU_THROW(ResourceError, "wglMakeCurrent() failed");
}

void Context::swapBuffers(void) const
{
    const Functions &wgl = m_core->getLibrary()->getFunctions();
    if (!wgl.swapLayerBuffers(m_deviceCtx, WGL_SWAP_MAIN_PLANE))
        TCU_THROW(ResourceError, "wglSwapBuffers() failed");
}

bool isSupportedByTests(const PixelFormatInfo &info)
{
    if (!info.supportOpenGL)
        return false;

    if (info.acceleration != wgl::PixelFormatInfo::ACCELERATION_FULL)
        return false;

    if (info.pixelType != wgl::PixelFormatInfo::PIXELTYPE_RGBA)
        return false;

    if ((info.surfaceTypes & wgl::PixelFormatInfo::SURFACE_WINDOW) == 0)
        return false;

    if (info.needPalette || info.needSystemPalette)
        return false;

    if (info.numOverlays != 0 || info.numUnderlays != 0)
        return false;

    if (info.stereo)
        return false;

    return true;
}

int choosePixelFormat(const Core &wgl, HDC deviceCtx, const glu::RenderConfig &config)
{
    std::vector<int> pixelFormats = wgl.getPixelFormats(deviceCtx);

    for (std::vector<int>::const_iterator fmtIter = pixelFormats.begin(); fmtIter != pixelFormats.end(); ++fmtIter)
    {
        const PixelFormatInfo info = wgl.getPixelFormatInfo(deviceCtx, *fmtIter);

        // Skip formats that are fundamentally not compatible with current tests
        if (!isSupportedByTests(info))
            continue;

        if (config.redBits != glu::RenderConfig::DONT_CARE && config.redBits != info.redBits)
            continue;

        if (config.greenBits != glu::RenderConfig::DONT_CARE && config.greenBits != info.greenBits)
            continue;

        if (config.blueBits != glu::RenderConfig::DONT_CARE && config.blueBits != info.blueBits)
            continue;

        if (config.alphaBits != glu::RenderConfig::DONT_CARE && config.alphaBits != info.alphaBits)
            continue;

        if (config.depthBits != glu::RenderConfig::DONT_CARE && config.depthBits != info.depthBits)
            continue;

        if (config.stencilBits != glu::RenderConfig::DONT_CARE && config.stencilBits != info.stencilBits)
            continue;

        if (config.numSamples != glu::RenderConfig::DONT_CARE && config.numSamples != info.samples)
            continue;

        // Passed all tests - select this.
        return info.pixelFormat;
    }

    return -1;
}

} // namespace wgl
} // namespace tcu
