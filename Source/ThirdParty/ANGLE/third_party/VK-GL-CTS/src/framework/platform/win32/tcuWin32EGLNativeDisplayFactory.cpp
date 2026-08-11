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
 * \brief Win32 EGL native display factory
 *//*--------------------------------------------------------------------*/

#include "tcuWin32EGLNativeDisplayFactory.hpp"

#include "egluDefs.hpp"
#include "tcuWin32Window.hpp"
#include "tcuWin32API.h"
#include "tcuTexture.hpp"
#include "deMemory.h"
#include "deThread.h"
#include "deClock.h"
#include "eglwLibrary.hpp"
#include "eglwEnums.hpp"

// Assume no call translation is needed
DE_STATIC_ASSERT(sizeof(eglw::EGLNativeDisplayType) == sizeof(HDC));
DE_STATIC_ASSERT(sizeof(eglw::EGLNativePixmapType) == sizeof(HBITMAP));
DE_STATIC_ASSERT(sizeof(eglw::EGLNativeWindowType) == sizeof(HWND));

namespace tcu
{
namespace win32
{
namespace
{

using namespace eglw;

enum
{
    DEFAULT_SURFACE_WIDTH  = 400,
    DEFAULT_SURFACE_HEIGHT = 300,
    WAIT_WINDOW_VISIBLE_MS =
        500 //!< Time to wait before issuing screenshot after changing window visibility (hack for DWM)
};

static const eglu::NativeDisplay::Capability DISPLAY_CAPABILITIES = eglu::NativeDisplay::CAPABILITY_GET_DISPLAY_LEGACY;
static const eglu::NativePixmap::Capability BITMAP_CAPABILITIES = eglu::NativePixmap::CAPABILITY_CREATE_SURFACE_LEGACY;
static const eglu::NativeWindow::Capability WINDOW_CAPABILITIES = (eglu::NativeWindow::Capability)(
    eglu::NativeWindow::CAPABILITY_CREATE_SURFACE_LEGACY | eglu::NativeWindow::CAPABILITY_GET_SURFACE_SIZE |
    eglu::NativeWindow::CAPABILITY_GET_SCREEN_SIZE | eglu::NativeWindow::CAPABILITY_READ_SCREEN_PIXELS |
    eglu::NativeWindow::CAPABILITY_SET_SURFACE_SIZE | eglu::NativeWindow::CAPABILITY_CHANGE_VISIBILITY);

class NativeDisplay : public eglu::NativeDisplay
{
public:
    NativeDisplay(void);
    virtual ~NativeDisplay(void)
    {
    }

    virtual EGLNativeDisplayType getLegacyNative(void)
    {
        return m_deviceContext;
    }
    const eglw::Library &getLibrary(void) const
    {
        return m_library;
    }

    HDC getDeviceContext(void)
    {
        return m_deviceContext;
    }

private:
    HDC m_deviceContext;
    eglw::DefaultLibrary m_library;
};

class NativePixmapFactory : public eglu::NativePixmapFactory
{
public:
    NativePixmapFactory(void);
    ~NativePixmapFactory(void)
    {
    }

    virtual eglu::NativePixmap *createPixmap(eglu::NativeDisplay *nativeDisplay, int width, int height) const;
    virtual eglu::NativePixmap *createPixmap(eglu::NativeDisplay *nativeDisplay, EGLDisplay display, EGLConfig config,
                                             const EGLAttrib *attribList, int width, int height) const;
};

class NativePixmap : public eglu::NativePixmap
{
public:
    NativePixmap(NativeDisplay *nativeDisplay, int width, int height, int bitDepth);
    virtual ~NativePixmap(void);

    EGLNativePixmapType getLegacyNative(void)
    {
        return m_bitmap;
    }

private:
    HBITMAP m_bitmap;
};

class NativeWindowFactory : public eglu::NativeWindowFactory
{
public:
    NativeWindowFactory(HINSTANCE instance);
    virtual ~NativeWindowFactory(void)
    {
    }

    virtual eglu::NativeWindow *createWindow(eglu::NativeDisplay *nativeDisplay,
                                             const eglu::WindowParams &params) const;

private:
    const HINSTANCE m_instance;
};

class NativeWindow : public eglu::NativeWindow
{
public:
    NativeWindow(NativeDisplay *nativeDisplay, HINSTANCE instance, const eglu::WindowParams &params);
    virtual ~NativeWindow(void);

    EGLNativeWindowType getLegacyNative(void)
    {
        return m_window.getHandle();
    }
    virtual IVec2 getSurfaceSize(void) const;
    virtual IVec2 getScreenSize(void) const
    {
        return getSurfaceSize();
    }
    virtual void processEvents(void);
    virtual void setSurfaceSize(IVec2 size);
    virtual void setVisibility(eglu::WindowParams::Visibility visibility);
    virtual void readScreenPixels(tcu::TextureLevel *dst) const;

private:
    win32::Window m_window;
    eglu::WindowParams::Visibility m_curVisibility;
    uint64_t m_setVisibleTime; //!< Time window was set visible.
};

// NativeDisplay

NativeDisplay::NativeDisplay(void)
    : eglu::NativeDisplay(DISPLAY_CAPABILITIES)
    , m_deviceContext((HDC)EGL_DEFAULT_DISPLAY)
    , m_library("libEGL.dll")
{
}

// NativePixmap

NativePixmap::NativePixmap(NativeDisplay *nativeDisplay, int width, int height, int bitDepth)
    : eglu::NativePixmap(BITMAP_CAPABILITIES)
    , m_bitmap(nullptr)
{
    const HDC deviceCtx = nativeDisplay->getDeviceContext();
    BITMAPINFO bitmapInfo;

    memset(&bitmapInfo, 0, sizeof(bitmapInfo));

    if (bitDepth != 24 && bitDepth != 32)
        throw NotSupportedError("Unsupported pixmap bit depth", nullptr, __FILE__, __LINE__);

    bitmapInfo.bmiHeader.biSize          = sizeof(bitmapInfo);
    bitmapInfo.bmiHeader.biWidth         = width;
    bitmapInfo.bmiHeader.biHeight        = height;
    bitmapInfo.bmiHeader.biPlanes        = 1;
    bitmapInfo.bmiHeader.biBitCount      = bitDepth;
    bitmapInfo.bmiHeader.biCompression   = BI_RGB;
    bitmapInfo.bmiHeader.biSizeImage     = 0;
    bitmapInfo.bmiHeader.biXPelsPerMeter = 1;
    bitmapInfo.bmiHeader.biYPelsPerMeter = 1;
    bitmapInfo.bmiHeader.biClrUsed       = 0;
    bitmapInfo.bmiHeader.biClrImportant  = 0;

    void *bitmapPtr = nullptr;
    m_bitmap        = CreateDIBSection(deviceCtx, &bitmapInfo, DIB_RGB_COLORS, &bitmapPtr, NULL, 0);

    if (!m_bitmap)
        throw ResourceError("Failed to create bitmap", nullptr, __FILE__, __LINE__);
}

NativePixmap::~NativePixmap(void)
{
    DeleteObject(m_bitmap);
}

// NativePixmapFactory

NativePixmapFactory::NativePixmapFactory(void)
    : eglu::NativePixmapFactory("bitmap", "Win32 Bitmap", BITMAP_CAPABILITIES)
{
}

eglu::NativePixmap *NativePixmapFactory::createPixmap(eglu::NativeDisplay *nativeDisplay, EGLDisplay display,
                                                      EGLConfig config, const EGLAttrib *attribList, int width,
                                                      int height) const
{
    const Library &egl = nativeDisplay->getLibrary();
    int redBits        = 0;
    int greenBits      = 0;
    int blueBits       = 0;
    int alphaBits      = 0;
    int bitSum         = 0;

    DE_ASSERT(display != EGL_NO_DISPLAY);

    egl.getConfigAttrib(display, config, EGL_RED_SIZE, &redBits);
    egl.getConfigAttrib(display, config, EGL_GREEN_SIZE, &greenBits);
    egl.getConfigAttrib(display, config, EGL_BLUE_SIZE, &blueBits);
    egl.getConfigAttrib(display, config, EGL_ALPHA_SIZE, &alphaBits);
    EGLU_CHECK_MSG(egl, "eglGetConfigAttrib()");

    bitSum = redBits + greenBits + blueBits + alphaBits;

    return new NativePixmap(dynamic_cast<NativeDisplay *>(nativeDisplay), width, height, bitSum);
}

eglu::NativePixmap *NativePixmapFactory::createPixmap(eglu::NativeDisplay *nativeDisplay, int width, int height) const
{
    const int defaultDepth = 32;
    return new NativePixmap(dynamic_cast<NativeDisplay *>(nativeDisplay), width, height, defaultDepth);
}

// NativeWindowFactory

NativeWindowFactory::NativeWindowFactory(HINSTANCE instance)
    : eglu::NativeWindowFactory("window", "Win32 Window", WINDOW_CAPABILITIES)
    , m_instance(instance)
{
}

eglu::NativeWindow *NativeWindowFactory::createWindow(eglu::NativeDisplay *nativeDisplay,
                                                      const eglu::WindowParams &params) const
{
    return new NativeWindow(dynamic_cast<NativeDisplay *>(nativeDisplay), m_instance, params);
}

// NativeWindow

NativeWindow::NativeWindow(NativeDisplay *nativeDisplay, HINSTANCE instance, const eglu::WindowParams &params)
    : eglu::NativeWindow(WINDOW_CAPABILITIES)
    , m_window(instance, params.width == eglu::WindowParams::SIZE_DONT_CARE ? DEFAULT_SURFACE_WIDTH : params.width,
               params.height == eglu::WindowParams::SIZE_DONT_CARE ? DEFAULT_SURFACE_HEIGHT : params.height)
    , m_curVisibility(eglu::WindowParams::VISIBILITY_HIDDEN)
    , m_setVisibleTime(0)
{
    if (params.visibility != eglu::WindowParams::VISIBILITY_DONT_CARE)
        setVisibility(params.visibility);
}

void NativeWindow::setVisibility(eglu::WindowParams::Visibility visibility)
{
    switch (visibility)
    {
    case eglu::WindowParams::VISIBILITY_HIDDEN:
        m_window.setVisible(false);
        m_curVisibility = visibility;
        break;

    case eglu::WindowParams::VISIBILITY_VISIBLE:
    case eglu::WindowParams::VISIBILITY_FULLSCREEN:
        // \todo [2014-03-12 pyry] Implement FULLSCREEN, or at least SW_MAXIMIZE.
        m_window.setVisible(true);
        m_curVisibility  = eglu::WindowParams::VISIBILITY_VISIBLE;
        m_setVisibleTime = deGetMicroseconds();
        break;

    default:
        DE_ASSERT(false);
    }
}

NativeWindow::~NativeWindow(void)
{
}

IVec2 NativeWindow::getSurfaceSize(void) const
{
    return m_window.getSize();
}

void NativeWindow::processEvents(void)
{
    m_window.processEvents();
}

void NativeWindow::setSurfaceSize(IVec2 size)
{
    m_window.setSize(size.x(), size.y());
}

void NativeWindow::readScreenPixels(tcu::TextureLevel *dst) const
{
    HDC windowDC      = nullptr;
    HDC screenDC      = nullptr;
    HDC tmpDC         = nullptr;
    HBITMAP tmpBitmap = nullptr;
    RECT rect;

    TCU_CHECK_INTERNAL(m_curVisibility != eglu::WindowParams::VISIBILITY_HIDDEN);

    // Hack for DWM: There is no way to wait for DWM animations to finish, so we just have to wait
    // for a while before issuing screenshot if window was just made visible.
    {
        const int64_t timeSinceVisibleUs = (int64_t)(deGetMicroseconds() - m_setVisibleTime);

        if (timeSinceVisibleUs < (int64_t)WAIT_WINDOW_VISIBLE_MS * 1000)
            deSleep(WAIT_WINDOW_VISIBLE_MS - (uint32_t)(timeSinceVisibleUs / 1000));
    }

    TCU_CHECK(GetClientRect(m_window.getHandle(), &rect));

    try
    {
        const int width  = rect.right - rect.left;
        const int height = rect.bottom - rect.top;
        BITMAPINFOHEADER bitmapInfo;

        deMemset(&bitmapInfo, 0, sizeof(bitmapInfo));

        screenDC = GetDC(nullptr);
        TCU_CHECK(screenDC);

        windowDC = GetDC(m_window.getHandle());
        TCU_CHECK(windowDC);

        tmpDC = CreateCompatibleDC(screenDC);
        TCU_CHECK(tmpDC != nullptr);

        MapWindowPoints(m_window.getHandle(), nullptr, (LPPOINT)&rect, 2);

        tmpBitmap = CreateCompatibleBitmap(screenDC, width, height);
        TCU_CHECK(tmpBitmap != nullptr);

        TCU_CHECK(SelectObject(tmpDC, tmpBitmap) != nullptr);

        TCU_CHECK(BitBlt(tmpDC, 0, 0, width, height, screenDC, rect.left, rect.top, SRCCOPY));

        bitmapInfo.biSize          = sizeof(BITMAPINFOHEADER);
        bitmapInfo.biWidth         = width;
        bitmapInfo.biHeight        = -height;
        bitmapInfo.biPlanes        = 1;
        bitmapInfo.biBitCount      = 32;
        bitmapInfo.biCompression   = BI_RGB;
        bitmapInfo.biSizeImage     = 0;
        bitmapInfo.biXPelsPerMeter = 0;
        bitmapInfo.biYPelsPerMeter = 0;
        bitmapInfo.biClrUsed       = 0;
        bitmapInfo.biClrImportant  = 0;

        dst->setStorage(TextureFormat(TextureFormat::BGRA, TextureFormat::UNORM_INT8), width, height);

        TCU_CHECK(GetDIBits(screenDC, tmpBitmap, 0, height, dst->getAccess().getDataPtr(), (BITMAPINFO *)&bitmapInfo,
                            DIB_RGB_COLORS));

        DeleteObject(tmpBitmap);
        tmpBitmap = nullptr;

        ReleaseDC(nullptr, screenDC);
        screenDC = nullptr;

        ReleaseDC(m_window.getHandle(), windowDC);
        windowDC = nullptr;

        DeleteDC(tmpDC);
        tmpDC = nullptr;
    }
    catch (...)
    {
        if (screenDC)
            ReleaseDC(nullptr, screenDC);

        if (windowDC)
            ReleaseDC(m_window.getHandle(), windowDC);

        if (tmpBitmap)
            DeleteObject(tmpBitmap);

        if (tmpDC)
            DeleteDC(tmpDC);

        throw;
    }
}

} // namespace

EGLNativeDisplayFactory::EGLNativeDisplayFactory(HINSTANCE instance)
    : eglu::NativeDisplayFactory("win32", "Native Win32 Display", DISPLAY_CAPABILITIES)
    , m_instance(instance)
{
    m_nativeWindowRegistry.registerFactory(new NativeWindowFactory(m_instance));
    m_nativePixmapRegistry.registerFactory(new NativePixmapFactory());
}

EGLNativeDisplayFactory::~EGLNativeDisplayFactory(void)
{
}

eglu::NativeDisplay *EGLNativeDisplayFactory::createDisplay(const EGLAttrib *attribList) const
{
    DE_UNREF(attribList);
    return new NativeDisplay();
}

} // namespace win32
} // namespace tcu
