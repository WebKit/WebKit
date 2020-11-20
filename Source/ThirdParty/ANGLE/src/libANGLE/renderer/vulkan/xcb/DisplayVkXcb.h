//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkXcb.h:
//    Defines the class interface for DisplayVkXcb, implementing DisplayVk for X via XCB.
//

#ifndef LIBANGLE_RENDERER_VULKAN_XCB_DISPLAYVKXCB_H_
#define LIBANGLE_RENDERER_VULKAN_XCB_DISPLAYVKXCB_H_

#include "libANGLE/renderer/vulkan/DisplayVk.h"

struct xcb_connection_t;

namespace rx
{

class DisplayVkXcb : public DisplayVk
{
  public:
    DisplayVkXcb(const egl::DisplayState &state);

    egl::Error initialize(egl::Display *display) override;
    void terminate() override;

    bool isValidNativeWindow(EGLNativeWindowType window) const override;

    SurfaceImpl *createWindowSurfaceVk(const egl::SurfaceState &state,
                                       EGLNativeWindowType window) override;

    egl::ConfigSet generateConfigs() override;
    void checkConfigSupport(egl::Config *config) override;

    const char *getWSIExtension() const override;
    angle::Result waitNativeImpl() override;

  private:
    xcb_connection_t *mXcbConnection;
};

}  // namespace rx

#endif  // LIBANGLE_RENDERER_VULKAN_XCB_DISPLAYVKXCB_H_
