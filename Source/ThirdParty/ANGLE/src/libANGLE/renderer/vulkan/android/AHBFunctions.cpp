//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "libANGLE/renderer/vulkan/android/AHBFunctions.h"

#include <dlfcn.h>

namespace rx
{

namespace
{

template <class T>
void AssignFn(void *handle, const char *name, T &fn)
{
    fn = reinterpret_cast<T>(dlsym(handle, name));
}

}  // namespace

AHBFunctions::AHBFunctions()
{
    void *handle = dlopen(nullptr, RTLD_NOW);
    AssignFn(handle, "AHardwareBuffer_acquire", mAcquireFn);
    AssignFn(handle, "AHardwareBuffer_describe", mDescribeFn);
    AssignFn(handle, "AHardwareBuffer_release", mReleaseFn);
}

AHBFunctions::~AHBFunctions() = default;

}  // namespace rx
