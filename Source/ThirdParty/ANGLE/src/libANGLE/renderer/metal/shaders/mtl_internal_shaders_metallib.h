//
// Copyright 2023 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Header declaring `static constexpr uint8_t gDefaultMetallib[]` containing the default
// Metal shader binary.

#if ANGLE_METAL_XCODE_BUILDS_SHADERS
#    error The build system should override this file.
#elif defined(ANGLE_PLATFORM_MACOS)
#    include "libANGLE/renderer/metal/shaders/mtl_internal_shaders_macos_autogen.h"
#else
#    include "libANGLE/renderer/metal/shaders/mtl_internal_shaders_ios_autogen.h"
#endif
