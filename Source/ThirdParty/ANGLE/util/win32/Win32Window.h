//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_WIN32_WINDOW_H
#define SAMPLE_UTIL_WIN32_WINDOW_H

#include "OSWindow.h"
#include <string>
#include <windows.h>

class Win32Window : public OSWindow
{
  public:
    Win32Window();
    ~Win32Window();

    bool initialize(const std::string &name, size_t width, size_t height) override;
    void destroy() override;

    EGLNativeWindowType getNativeWindow() const override;
    EGLNativeDisplayType getNativeDisplay() const override;

    void messageLoop() override;

    void pushEvent(Event event) override;

    void setMousePosition(int x, int y) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;

    void signalTestEvent() override;

  private:
    std::string mParentClassName;
    std::string mChildClassName;

    EGLNativeWindowType mNativeWindow;
    EGLNativeWindowType mParentWindow;
    EGLNativeDisplayType mNativeDisplay;
};

#endif // SAMPLE_UTIL_WINDOW_H
