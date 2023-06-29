//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file includes APIs to detect whether certain Apple renderer is available for testing.
//

#include "test_utils/angle_test_instantiate_apple.h"

#include "common/apple_platform_utils.h"
#include "test_utils/angle_test_instantiate.h"

namespace angle
{

bool IsMetalTextureSwizzleAvailable()
{
    // NOTE(hqle): This might not be accurate, since the capabilities also depend on underlying
    // hardwares, however, it is OK for testing.
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13))
    {
        // All NVIDIA and older Intel don't support swizzle because they are GPU family 1.
        // We don't have a way to detect Metal family here, so skip all Intel for now.
        return !IsIntel() && !IsNVIDIA();
    }
    return false;
}

bool IsMetalCompressedTexture3DAvailable()
{
    if (ANGLE_APPLE_AVAILABLE_XCI(10.15, 13.1, 13.0))
    {
        return true;
    }
    return false;
}

}  // namespace angle
