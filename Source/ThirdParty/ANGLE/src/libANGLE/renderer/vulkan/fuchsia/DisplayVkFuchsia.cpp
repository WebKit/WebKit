//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// DisplayVkFuchsia.h:
//    Implements methods from DisplayVkFuchsia
//

#include "libANGLE/renderer/vulkan/fuchsia/DisplayVkFuchsia.h"

#include "libANGLE/renderer/vulkan/fuchsia/WindowSurfaceVkFuchsia.h"
#include "libANGLE/renderer/vulkan/vk_caps_utils.h"

namespace rx
{

DisplayVkFuchsia::DisplayVkFuchsia(const egl::DisplayState &state) : DisplayVk(state) {}

bool DisplayVkFuchsia::isValidNativeWindow(EGLNativeWindowType window) const
{
    return WindowSurfaceVkFuchsia::isValidNativeWindow(window);
}

SurfaceImpl *DisplayVkFuchsia::createWindowSurfaceVk(const egl::SurfaceState &state,
                                                     EGLNativeWindowType window,
                                                     EGLint width,
                                                     EGLint height)
{
    ASSERT(isValidNativeWindow(window));
    return new WindowSurfaceVkFuchsia(state, window, width, height);
}

egl::ConfigSet DisplayVkFuchsia::generateConfigs()
{
    constexpr GLenum kColorFormats[] = {GL_BGRA8_EXT, GL_BGRX8_ANGLEX};
    constexpr EGLint kSampleCounts[] = {0};
    return egl_vk::GenerateConfigs(kColorFormats, egl_vk::kConfigDepthStencilFormats, kSampleCounts,
                                   this);
}

bool DisplayVkFuchsia::checkConfigSupport(egl::Config *config)
{
    // TODO(geofflang): Test for native support and modify the config accordingly.
    // anglebug.com/2692
    return true;
}

const char *DisplayVkFuchsia::getWSIExtension() const
{
    return VK_FUCHSIA_IMAGEPIPE_SURFACE_EXTENSION_NAME;
}

const char *DisplayVkFuchsia::getWSILayer() const
{
    return "VK_LAYER_FUCHSIA_imagepipe_swapchain";
}

}  // namespace rx
