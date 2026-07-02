#ifndef _TCUWGL_HPP
#define _TCUWGL_HPP
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

#include "tcuDefs.hpp"
#include "gluRenderConfig.hpp"
#include "gluRenderContext.hpp"
#include "deDynamicLibrary.h"
#include "tcuWin32API.h"

#include <vector>

namespace glu
{
struct RenderConfig;
}

namespace tcu
{
namespace wgl
{

class Library;
class Context;

/*--------------------------------------------------------------------*//*!
 * \brief WGL pixel format info.
 *//*--------------------------------------------------------------------*/
class PixelFormatInfo
{
public:
    enum PixelType
    {
        PIXELTYPE_RGBA = 0,
        PIXELTYPE_RGBA_FLOAT,
        PIXELTYPE_RGBA_UNSIGNED_FLOAT,
        PIXELTYPE_COLOR_INDEX,
        PIXELTYPE_UNKNOWN,

        PIXELTYPE_LAST
    };

    enum SurfaceFlags
    {
        SURFACE_WINDOW = (1 << 0),
        SURFACE_PIXMAP = (1 << 1)
    };

    enum Acceleration
    {
        ACCELERATION_NONE = 0,
        ACCELERATION_GENERIC,
        ACCELERATION_FULL,
        ACCELERATION_UNKNOWN,

        ACCELERATION_LAST
    };

    int pixelFormat;

    // From WGL_ARB_pixel_format
    uint32_t surfaceTypes;
    Acceleration acceleration;
    bool needPalette;
    bool needSystemPalette;
    // bool swapLayerBuffers;
    //    SwapMethod        swapMethod; { EXCHANGE, UNDEFINED }
    int numOverlays;
    int numUnderlays;
    // bool transparent;
    // int transparentRedValue;
    // int transparentGreenValue;
    // int transparentBlueValue;
    // int transparentAlphaValue;
    // int transparentIndexValue;
    // bool shareDepth;
    // bool shareStencil;
    // bool shareAccum;
    // bool supportGDI;
    bool supportOpenGL;
    bool doubleBuffer;
    bool stereo;
    PixelType pixelType;

    // int colorBits;
    int redBits;
    // int redShift;
    int greenBits;
    // int greenShift;
    int blueBits;
    // int blueShift;
    int alphaBits;
    // int alphaShift;

    int accumBits;
    // int accumRedBits;
    // int accumGreenBits;
    // int accumBlueBits;
    // int accumAlphaBits;

    int depthBits;
    int stencilBits;

    int numAuxBuffers;

    // From WGL_ARB_multisample
    int sampleBuffers;
    int samples;

    // From WGL_EXT_colorspace
    bool sRGB;

    // \todo [2013-04-14 pyry] Version bits?

    PixelFormatInfo(void)
        : pixelFormat(0)
        , surfaceTypes(0)
        , acceleration(ACCELERATION_LAST)
        , needPalette(false)
        , needSystemPalette(false)
        , numOverlays(0)
        , numUnderlays(0)
        , supportOpenGL(false)
        , doubleBuffer(false)
        , stereo(false)
        , pixelType(PIXELTYPE_LAST)
        , redBits(0)
        , greenBits(0)
        , blueBits(0)
        , alphaBits(0)
        , accumBits(0)
        , depthBits(0)
        , stencilBits(0)
        , numAuxBuffers(0)
        , sampleBuffers(0)
        , samples(0)
        , sRGB(false)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Core WGL API
 *
 * \note Created API objects depend on Core object being live. User is
 *         resposible of keeping Core live as long as there are API objects
 *         (such as GL contexts) live!
 *//*--------------------------------------------------------------------*/
class Core
{
public:
    Core(HINSTANCE instance);
    ~Core(void);

    std::vector<int> getPixelFormats(HDC deviceCtx) const;
    PixelFormatInfo getPixelFormatInfo(HDC deviceCtx, int pixelFormat) const;

    // Internal
    const Library *getLibrary(void) const
    {
        return m_library;
    }

private:
    Core(const Core &other);
    Core &operator=(const Core &other);

    Library *m_library;
};

//! Function pointer type.
typedef void(__stdcall *FunctionPtr)(void);

/*--------------------------------------------------------------------*//*!
 * \brief WGL context
 *
 * Context is currently made current to current thread in constructor
 * and detached in destructor. Thus context should be created in and
 * accessed from a single thread.
 *//*--------------------------------------------------------------------*/
class Context
{
public:
    Context(const Core *core, HDC deviceCtx, const Context *sharedContext, glu::ContextType ctxType, int pixelFormat,
            glu::ResetNotificationStrategy resetNotificationStrategy);
    ~Context(void);

    FunctionPtr getGLFunction(const char *name) const;

    void makeCurrent(void);
    void swapBuffers(void) const;

    HDC getDeviceContext(void) const
    {
        return m_deviceCtx;
    }
    HGLRC getGLContext(void) const
    {
        return m_context;
    }

private:
    Context(const Context &other);
    Context &operator=(const Context &other);

    const Core *m_core;
    HDC m_deviceCtx;
    HGLRC m_context;
};

//! Utility for selecting config. Returns -1 if no matching pixel format was found.
int choosePixelFormat(const Core &wgl, HDC deviceCtx, const glu::RenderConfig &config);

//! Is pixel format in general supported by dEQP tests?
bool isSupportedByTests(const PixelFormatInfo &pixelFormatInfo);

} // namespace wgl
} // namespace tcu

#endif // _TCUWGL_HPP
