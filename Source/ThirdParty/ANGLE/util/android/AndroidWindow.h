//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// AndroidWindow.h: Definition of the implementation of OSWindow for Android

#ifndef UTIL_ANDROID_WINDOW_H_
#define UTIL_ANDROID_WINDOW_H_

#include "util/OSWindow.h"

class AndroidWindow : public OSWindow
{
  public:
    AndroidWindow();
    ~AndroidWindow() override;

    bool initialize(const std::string &name, size_t width, size_t height) override;
    void destroy() override;

    void resetNativeWindow() override;
    EGLNativeWindowType getNativeWindow() const override;
    EGLNativeDisplayType getNativeDisplay() const override;

    void messageLoop() override;

    void setMousePosition(int x, int y) override;
    bool setPosition(int x, int y) override;
    bool resize(int width, int height) override;
    void setVisible(bool isVisible) override;

    void signalTestEvent() override;
};

#endif /* UTIL_ANDROID_WINDOW_H_ */
