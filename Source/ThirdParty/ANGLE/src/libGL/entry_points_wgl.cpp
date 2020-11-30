//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// entry_points_wgl.cpp: Implements the exported WGL functions.

#include "entry_points_wgl.h"

#include "common/debug.h"
#include "common/event_tracer.h"
#include "common/utilities.h"
#include "common/version.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/EGLSync.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/Thread.h"
#include "libANGLE/entry_points_utils.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/validationEGL.h"
#include "libGL/proc_table_wgl.h"
#include "libGLESv2/global_state.h"

using namespace wgl;
using namespace egl;

namespace
{

bool CompareProc(const ProcEntry &a, const char *b)
{
    return strcmp(a.first, b) < 0;
}

void ClipConfigs(const std::vector<const Config *> &filteredConfigs,
                 EGLConfig *output_configs,
                 EGLint config_size,
                 EGLint *num_config)
{
    EGLint result_size = static_cast<EGLint>(filteredConfigs.size());
    if (output_configs)
    {
        result_size = std::max(std::min(result_size, config_size), 0);
        for (EGLint i = 0; i < result_size; i++)
        {
            output_configs[i] = const_cast<Config *>(filteredConfigs[i]);
        }
    }
    *num_config = result_size;
}
}  // anonymous namespace

extern "C" {

// WGL 1.0
int GL_APIENTRY wglChoosePixelFormat(HDC hDc, const PIXELFORMATDESCRIPTOR *pPfd)
{
    UNIMPLEMENTED();
    return 1;
}

int GL_APIENTRY wglDescribePixelFormat(HDC hdc, int ipfd, UINT cjpfd, PIXELFORMATDESCRIPTOR *ppfd)
{
    UNIMPLEMENTED();
    if (ppfd)
    {
        ppfd->dwFlags      = ppfd->dwFlags | PFD_DRAW_TO_WINDOW;
        ppfd->dwFlags      = ppfd->dwFlags | PFD_SUPPORT_OPENGL;
        ppfd->dwFlags      = ppfd->dwFlags | PFD_GENERIC_ACCELERATED;
        ppfd->dwFlags      = ppfd->dwFlags | PFD_DOUBLEBUFFER;
        ppfd->iPixelType   = PFD_TYPE_RGBA;
        ppfd->cColorBits   = 24;
        ppfd->cRedBits     = 8;
        ppfd->cGreenBits   = 8;
        ppfd->cBlueBits    = 8;
        ppfd->cAlphaBits   = 8;
        ppfd->cDepthBits   = 24;
        ppfd->cStencilBits = 8;
        ppfd->nVersion     = 1;
    }
    return 1;
}

UINT GL_APIENTRY wglGetEnhMetaFilePixelFormat(HENHMETAFILE hemf,
                                              UINT cbBuffer,
                                              PIXELFORMATDESCRIPTOR *ppfd)
{
    UNIMPLEMENTED();
    return 1u;
}

int GL_APIENTRY wglGetPixelFormat(HDC hdc)
{
    UNIMPLEMENTED();
    return 1;
}

BOOL GL_APIENTRY wglSetPixelFormat(HDC hdc, int ipfd, const PIXELFORMATDESCRIPTOR *ppfd)
{
    UNIMPLEMENTED();
    return TRUE;
}

BOOL GL_APIENTRY wglSwapBuffers(HDC hdc)
{
    Thread *thread        = egl::GetCurrentThread();
    egl::Display *display = egl::Display::GetExistingDisplayFromNativeDisplay(hdc);

    ANGLE_EGL_TRY_RETURN(thread, display->getWGLSurface()->swap(thread->getContext()),
                         "wglSwapBuffers", display->getWGLSurface(), FALSE);
    return TRUE;
}

BOOL GL_APIENTRY wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
    UNIMPLEMENTED();
    return TRUE;
}

HGLRC GL_APIENTRY wglCreateContext(HDC hDc)
{
    Thread *thread = egl::GetCurrentThread();

    std::vector<EGLAttrib> displayAttributes;
    displayAttributes.push_back(EGL_PLATFORM_ANGLE_TYPE_ANGLE);
    GLenum platformType = EGL_PLATFORM_ANGLE_TYPE_DEFAULT_ANGLE;
    displayAttributes.push_back(platformType);
    displayAttributes.push_back(EGL_NONE);

    const auto &attribMapDisplay = AttributeMap::CreateFromAttribArray(displayAttributes.data());

    egl::Display *display = egl::Display::GetDisplayFromNativeDisplay(hDc, attribMapDisplay);

    ANGLE_EGL_TRY_RETURN(thread, display->initialize(), "wglCreateContext", display, nullptr);

    thread->setAPI(EGL_OPENGL_API);

    // Default config
    const EGLint configAttributes[] = {EGL_NONE};

    // Choose config
    EGLint configCount;
    EGLConfig config;
    AttributeMap attribMapConfig = AttributeMap::CreateFromIntArray(configAttributes);
    ClipConfigs(display->chooseConfig(attribMapConfig), &config, 1, &configCount);

    Config *configuration = static_cast<Config *>(config);

    // Initialize surface
    std::vector<EGLint> surfaceAttributes;
    surfaceAttributes.push_back(EGL_NONE);
    surfaceAttributes.push_back(EGL_NONE);
    AttributeMap surfAttributes = AttributeMap::CreateFromIntArray(&surfaceAttributes[0]);

    // Create first window surface
    egl::Surface *surface = nullptr;
    ANGLE_EGL_TRY_RETURN(
        thread,
        display->createWindowSurface(configuration, WindowFromDC(hDc), surfAttributes, &surface),
        "wglCreateContext", display, nullptr);

    // Initialize context
    EGLint contextAttibutes[] = {EGL_CONTEXT_CLIENT_VERSION, 4, EGL_CONTEXT_MINOR_VERSION, 6,
                                 EGL_NONE};

    gl::Context *sharedGLContext = static_cast<gl::Context *>(nullptr);
    AttributeMap ctxAttributes   = AttributeMap::CreateFromIntArray(contextAttibutes);

    gl::Context *context = nullptr;

    ANGLE_EGL_TRY_RETURN(thread,
                         display->createContext(configuration, sharedGLContext, EGL_OPENGL_API,
                                                ctxAttributes, &context),
                         "wglCreateContext", display, nullptr);

    return reinterpret_cast<HGLRC>(context);
}

HGLRC GL_APIENTRY wglCreateLayerContext(HDC hDc, int level)
{
    UNIMPLEMENTED();
    return nullptr;
}

BOOL GL_APIENTRY wglDeleteContext(HGLRC oldContext)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglDescribeLayerPlane(HDC hDc,
                                       int pixelFormat,
                                       int layerPlane,
                                       UINT nBytes,
                                       LAYERPLANEDESCRIPTOR *plpd)
{
    UNIMPLEMENTED();
    return FALSE;
}

HGLRC GL_APIENTRY wglGetCurrentContext()
{
    UNIMPLEMENTED();
    return nullptr;
}

HDC GL_APIENTRY wglGetCurrentDC()
{
    UNIMPLEMENTED();
    return nullptr;
}

int GL_APIENTRY
wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, COLORREF *pcr)
{
    UNIMPLEMENTED();
    return 0;
}

PROC GL_APIENTRY wglGetProcAddress(LPCSTR lpszProc)
{
    ANGLE_SCOPED_GLOBAL_LOCK();
    FUNC_EVENT("const char *procname = \"%s\"", lpszProc);
    egl::Thread *thread = egl::GetCurrentThread();

    const ProcEntry *entry =
        std::lower_bound(&g_procTable[0], &g_procTable[g_numProcs], lpszProc, CompareProc);

    thread->setSuccess();

    if (entry == &g_procTable[g_numProcs] || strcmp(entry->first, lpszProc) != 0)
    {
        return nullptr;
    }

    return entry->second;
}

BOOL GL_APIENTRY wglMakeCurrent(HDC hDc, HGLRC newContext)
{
    Thread *thread        = egl::GetCurrentThread();
    egl::Display *display = egl::Display::GetExistingDisplayFromNativeDisplay(hDc);
    const gl::Context *context =
        GetContextIfValid(display, reinterpret_cast<gl::Context *>(newContext));

    // If display or context are invalid, make thread's current rendering context not current
    if (!context)
    {
        gl::Context *oldContext = thread->getContext();
        if (oldContext)
        {
            ANGLE_EGL_TRY_RETURN(thread, oldContext->unMakeCurrent(display), "wglMakeCurrent",
                                 GetContextIfValid(display, oldContext), EGL_FALSE);
        }
        SetContextCurrent(thread, nullptr);
        return TRUE;
    }

    egl::Surface *surface        = display->getWGLSurface();
    Surface *previousDraw        = thread->getCurrentDrawSurface();
    Surface *previousRead        = thread->getCurrentReadSurface();
    gl::Context *previousContext = thread->getContext();

    if (previousDraw != surface || previousRead != surface || previousContext != context)
    {
        ANGLE_EGL_TRY_RETURN(
            thread,
            display->makeCurrent(thread, surface, surface, const_cast<gl::Context *>(context)),
            "wglMakeCurrent", GetContextIfValid(display, context), EGL_FALSE);

        SetContextCurrent(thread, const_cast<gl::Context *>(context));
    }

    return TRUE;
}

BOOL GL_APIENTRY wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL bRealize)
{
    UNIMPLEMENTED();
    return FALSE;
}

int GL_APIENTRY
wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane, int iStart, int cEntries, const COLORREF *pcr)
{
    UNIMPLEMENTED();
    return 0;
}

BOOL GL_APIENTRY wglShareLists(HGLRC hrcSrvShare, HGLRC hrcSrvSource)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglSwapLayerBuffers(HDC hdc, UINT fuFlags)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglUseFontBitmapsA(HDC hDC, DWORD first, DWORD count, DWORD listBase)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglUseFontBitmapsW(HDC hDC, DWORD first, DWORD count, DWORD listBase)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglUseFontOutlinesA(HDC hDC,
                                     DWORD first,
                                     DWORD count,
                                     DWORD listBase,
                                     FLOAT deviation,
                                     FLOAT extrusion,
                                     int format,
                                     LPGLYPHMETRICSFLOAT lpgmf)
{
    UNIMPLEMENTED();
    return FALSE;
}

BOOL GL_APIENTRY wglUseFontOutlinesW(HDC hDC,
                                     DWORD first,
                                     DWORD count,
                                     DWORD listBase,
                                     FLOAT deviation,
                                     FLOAT extrusion,
                                     int format,
                                     LPGLYPHMETRICSFLOAT lpgmf)
{
    UNIMPLEMENTED();
    return FALSE;
}

}  // extern "C"
