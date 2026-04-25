//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/wgpu/wgpu_proc_utils.h"

// DawnNative.h includes the webgpu cpp API which depends on wgpu function declarations. To hide the
// declarations from the rest of the ANGLE WebGPU backend, only enable them in this file.
#undef WGPU_SKIP_DECLARATIONS
#include <dawn/native/DawnNative.h>

namespace rx
{
namespace webgpu
{
const DawnProcTable &GetDefaultProcTable()
{
    return dawn::native::GetProcs();
}
}  // namespace webgpu
}  // namespace rx
