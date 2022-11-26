//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// X11Window.h: Definition of the implementation of OSWindow for X11

#ifndef UTIL_X11_WINDOW_H
#define UTIL_X11_WINDOW_H

#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include <X11/Xutil.h>
#include <string>

#include "util/OSWindow.h"
#include "util/util_export.h"

bool IsX11WindowAvailable();

class ANGLE_UTIL_EXPORT X11Window : public OSWindow
{
  public:
    X11Window();
    X11Window(int visualId);
    ~X11Window() override;

    void disableErrorMessageDialog() override;
    void destroy() override;

    void resetNativeWindow() override;
    EGLNativeWindowType getNativeWindow() const override;
    void *getPlatformExtension() override;
    EGLNativeDisplayType getNativeDisplay() const override;

    void messageLoop() override;

    void setMousePosition(int x, int y) override;
    bool setOrientation(int width, int height) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;

    void signalTestEvent() override;

  private:
    bool initializeImpl(const std::string &name, int width, int height) override;
    void processEvent(const XEvent &event);

    Atom WM_DELETE_WINDOW;
    Atom WM_PROTOCOLS;
    Atom TEST_EVENT;

    Display *mDisplay;
    Window mWindow;
    int mRequestedVisualId;
    bool mVisible;
    bool mConfigured = false;
    bool mDestroyed  = false;
};

#endif  // UTIL_X11_WINDOW_H
