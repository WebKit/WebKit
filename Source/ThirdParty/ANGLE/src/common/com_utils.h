//
// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMMON_COM_UTILS_H_
#define COMMON_COM_UTILS_H_

#include "common/angleutils.h"
#include "common/debug.h"

namespace angle
{

template <typename OutType>
ComPtr<OutType> DynamicCastComObject(IUnknown *object)
{
    ASSERT(object != nullptr);

    ComPtr<OutType> outObject;
    const HRESULT hr = object->QueryInterface(IID_PPV_ARGS(&outObject));
    RELEASE_ASSERT((outObject.Get() != nullptr) == SUCCEEDED(hr));
    return outObject;
}

}  // namespace angle

#endif  // COMMON_COM_UTILS_H_
