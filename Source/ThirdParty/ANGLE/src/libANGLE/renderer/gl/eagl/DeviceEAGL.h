//
// Copyright 2020 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// DeviceEAGL.h: EAGL implementation of egl::Device

#ifndef LIBANGLE_RENDERER_GL_EAGL_DEVICEEAGL_H_
#define LIBANGLE_RENDERER_GL_EAGL_DEVICEEAGL_H_

#import "common/platform.h"

#if (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#include "libANGLE/Device.h"
#include "libANGLE/renderer/DeviceImpl.h"

namespace rx
{
class DeviceEAGL : public DeviceImpl
{
  public:
    DeviceEAGL();
    ~DeviceEAGL() override;

    egl::Error initialize() override;
    egl::Error getAttribute(const egl::Display *display,
                            EGLint attribute,
                            void **outValue) override;
    EGLint getType() override;
    void generateExtensions(egl::DeviceExtensions *outExtensions) const override;
};

}  // namespace rx

#endif  // (defined(ANGLE_PLATFORM_IOS) && !defined(ANGLE_PLATFORM_MACCATALYST)) || (defined(ANGLE_PLATFORM_MACCATALYST) && defined(ANGLE_CPU_ARM64))

#endif  // LIBANGLE_RENDERER_GL_EAGL_DEVICEEAGL_H_
