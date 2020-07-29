//
// Copyright 2018 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WGLWindow:
//   Implements initializing a WGL rendering context.
//

#ifndef UTIL_WINDOWS_WGLWINDOW_H_
#define UTIL_WINDOWS_WGLWINDOW_H_

#include "common/angleutils.h"
#include "export.h"
#include "util/EGLWindow.h"

class OSWindow;

namespace angle
{
class Library;
}  // namespace angle

class ANGLE_UTIL_EXPORT WGLWindow : public GLWindowBase
{
  public:
    static WGLWindow *New(int glesMajorVersion, int glesMinorVersion);
    static void Delete(WGLWindow **window);

    // Internally initializes GL resources.
    bool initializeGL(OSWindow *osWindow,
                      angle::Library *glWindowingLibrary,
                      angle::GLESDriverType driverType,
                      const EGLPlatformParameters &platformParams,
                      const ConfigParameters &configParams) override;
    void destroyGL() override;
    bool isGLInitialized() const override;

    bool makeCurrent() override;
    void swap() override;
    bool hasError() const override;
    bool setSwapInterval(EGLint swapInterval) override;
    angle::GenericProc getProcAddress(const char *name) override;

  private:
    WGLWindow(int glesMajorVersion, int glesMinorVersion);
    ~WGLWindow() override;

    // OS resources.
    HDC mDeviceContext;
    HGLRC mWGLContext;
    HWND mWindow;
};

#endif  // UTIL_WINDOWS_WGLWINDOW_H_
