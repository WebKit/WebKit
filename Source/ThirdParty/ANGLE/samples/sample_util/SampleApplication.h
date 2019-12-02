//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_SAMPLE_APPLICATION_H
#define SAMPLE_UTIL_SAMPLE_APPLICATION_H

#include <stdint.h>
#include <list>
#include <memory>
#include <string>

#include "common/system_utils.h"
#include "util/EGLPlatformParameters.h"
#include "util/OSWindow.h"
#include "util/Timer.h"
#include "util/egl_loader_autogen.h"

class EGLWindow;

namespace angle
{
class Library;
}  // namespace angle

class SampleApplication
{
  public:
    SampleApplication(std::string name,
                      int argc,
                      char **argv,
                      EGLint glesMajorVersion = 2,
                      EGLint glesMinorVersion = 0,
                      uint32_t width          = 1280,
                      uint32_t height         = 720);
    virtual ~SampleApplication();

    virtual bool initialize();
    virtual void destroy();

    virtual void step(float dt, double totalTime);
    virtual void draw();

    virtual void swap();

    virtual void onKeyUp(const Event::KeyEvent &keyEvent);
    virtual void onKeyDown(const Event::KeyEvent &keyEvent);

    OSWindow *getWindow() const;
    EGLConfig getConfig() const;
    EGLDisplay getDisplay() const;
    EGLSurface getSurface() const;
    EGLContext getContext() const;

    int run();
    void exit();

  private:
    bool popEvent(Event *event);

    std::string mName;
    uint32_t mWidth;
    uint32_t mHeight;
    bool mRunning;

    Timer mTimer;
    EGLWindow *mEGLWindow;
    OSWindow *mOSWindow;

    EGLPlatformParameters mPlatformParams;

    // Handle to the entry point binding library.
    std::unique_ptr<angle::Library> mEntryPointsLib;
};

#endif  // SAMPLE_UTIL_SAMPLE_APPLICATION_H
