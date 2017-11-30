//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkXcb.cpp:
//    Implements the class methods for DisplayVkXcb.
//

#include "libANGLE/renderer/vulkan/xcb/DisplayVkXcb.h"

#include <xcb/xcb.h>

#include "libANGLE/renderer/vulkan/xcb/WindowSurfaceVkXcb.h"

namespace rx
{

DisplayVkXcb::DisplayVkXcb(const egl::DisplayState &state)
    : DisplayVk(state), mXcbConnection(nullptr)
{
}

egl::Error DisplayVkXcb::initialize(egl::Display *display)
{
    mXcbConnection = xcb_connect(nullptr, nullptr);
    if (mXcbConnection == nullptr)
    {
        return egl::EglNotInitialized();
    }
    return DisplayVk::initialize(display);
}

void DisplayVkXcb::terminate()
{
    ASSERT(mXcbConnection != nullptr);
    xcb_disconnect(mXcbConnection);
    mXcbConnection = nullptr;
    DisplayVk::terminate();
}

bool DisplayVkXcb::isValidNativeWindow(EGLNativeWindowType window) const
{
    // There doesn't appear to be an xcb function explicitly for checking the validity of a
    // window ID, but xcb_query_tree_reply will return nullptr if the window doesn't exist.
    xcb_query_tree_cookie_t cookie = xcb_query_tree(mXcbConnection, window);
    xcb_query_tree_reply_t *reply  = xcb_query_tree_reply(mXcbConnection, cookie, nullptr);
    if (reply)
    {
        free(reply);
        return true;
    }
    return false;
}

SurfaceImpl *DisplayVkXcb::createWindowSurfaceVk(const egl::SurfaceState &state,
                                                 EGLNativeWindowType window,
                                                 EGLint width,
                                                 EGLint height)
{
    return new WindowSurfaceVkXcb(state, window, width, height, mXcbConnection);
}

const char *DisplayVkXcb::getWSIName() const
{
    return VK_KHR_XCB_SURFACE_EXTENSION_NAME;
}

}  // namespace rx
