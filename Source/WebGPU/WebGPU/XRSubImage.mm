/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "XRSubImage.h"

#import "APIConversions.h"
#import "Device.h"
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

XRSubImage::XRSubImage(bool, Device& device)
    : m_device(device)
{
}

XRSubImage::XRSubImage(Device& device)
    : m_device(device)
{
}

XRSubImage::~XRSubImage() = default;

Ref<XRSubImage> Device::createXRSubImage()
{
    if (!isValid())
        return XRSubImage::createInvalid(*this);

    return XRSubImage::create(*this);
}

void XRSubImage::setLabel(String&&)
{
}

bool XRSubImage::isValid() const
{
    return true;
}

RefPtr<XRSubImage> XRBinding::getViewSubImage(WGPUXREye eye)
{
    return device().getXRViewSubImage(eye);
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuXRSubImageReference(WGPUXRSubImage subImage)
{
    WebGPU::fromAPI(subImage).ref();
}

void wgpuXRSubImageRelease(WGPUXRSubImage subImage)
{
    WebGPU::fromAPI(subImage).deref();
}
