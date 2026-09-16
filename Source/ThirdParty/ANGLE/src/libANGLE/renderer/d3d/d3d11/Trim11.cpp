//
// Copyright 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Trim11.cpp: Trim support utility class.

#include "libANGLE/renderer/d3d/d3d11/Trim11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

#if defined(ANGLE_ENABLE_WINDOWS_UWP)
#    include <windows.applicationmodel.core.h>
#    include <wrl.h>
#    include <wrl/wrappers/corewrappers.h>
using namespace Microsoft::WRL;
using namespace Microsoft::WRL::Wrappers;
using namespace ABI::Windows::ApplicationModel;
using namespace ABI::Windows::ApplicationModel::Core;
using namespace ABI::Windows::Foundation;
using namespace ABI::Windows::Foundation::Collections;
#endif

namespace rx
{

Trim11::Trim11(rx::Renderer11 *renderer) : mRenderer(renderer)
{
    bool result = true;
    result      = registerForRendererTrimRequest();
    ASSERT(result);
}

Trim11::~Trim11()
{
    unregisterForRendererTrimRequest();
}

void Trim11::trim()
{
    if (!mRenderer)
    {
        return;
    }

#if defined(ANGLE_ENABLE_WINDOWS_UWP)
    ID3D11Device *device                    = mRenderer->getDevice();
    angle::ComPtr<IDXGIDevice3> dxgiDevice3 = angle::DynamicCastComObject<IDXGIDevice3>(device);
    if (dxgiDevice3)
    {
        dxgiDevice3->Trim();
    }
#endif
}

bool Trim11::registerForRendererTrimRequest()
{
#if defined(ANGLE_ENABLE_WINDOWS_UWP)
    angle::ComPtr<ICoreApplication> coreApplication;
    HRESULT result = GetActivationFactory(
        HStringReference(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication).Get(),
        &coreApplication);
    if (SUCCEEDED(result))
    {
        auto suspendHandler = Callback<IEventHandler<SuspendingEventArgs *>>(
            [this](IInspectable *, ISuspendingEventArgs *) -> HRESULT {
                trim();
                return S_OK;
            });
        result =
            coreApplication->add_Suspending(suspendHandler.Get(), &mApplicationSuspendedEventToken);
    }
    if (FAILED(result))
    {
        return false;
    }
#endif
    return true;
}

void Trim11::unregisterForRendererTrimRequest()
{
#if defined(ANGLE_ENABLE_WINDOWS_UWP)
    if (mApplicationSuspendedEventToken.value != 0)
    {
        angle::ComPtr<ICoreApplication> coreApplication;
        if (SUCCEEDED(GetActivationFactory(
                HStringReference(RuntimeClass_Windows_ApplicationModel_Core_CoreApplication).Get(),
                &coreApplication)))
        {
            coreApplication->remove_Suspending(mApplicationSuspendedEventToken);
        }
        mApplicationSuspendedEventToken.value = 0;
    }
#endif
}

}  // namespace rx
