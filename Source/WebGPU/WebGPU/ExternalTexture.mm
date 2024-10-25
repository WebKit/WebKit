/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#import "ExternalTexture.h"

#import "APIConversions.h"
#import "Device.h"
#import "TextureView.h"
#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebGPU {

Ref<ExternalTexture> Device::createExternalTexture(const WGPUExternalTextureDescriptor& descriptor)
{
    if (!isValid())
        return ExternalTexture::createInvalid(*this);

    return ExternalTexture::create(descriptor.pixelBuffer, descriptor.colorSpace, *this);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExternalTexture);

ExternalTexture::ExternalTexture(CVPixelBufferRef pixelBuffer, WGPUColorSpace colorSpace, Device& device)
    : m_pixelBuffer(pixelBuffer)
    , m_colorSpace(colorSpace)
    , m_device(device)
{
    update(pixelBuffer);
}

ExternalTexture::ExternalTexture(Device& device)
    : m_device(device)
{
}

bool ExternalTexture::isValid() const
{
    return m_pixelBuffer.get() || m_destroyed;
}

ExternalTexture::~ExternalTexture() = default;

void ExternalTexture::destroy()
{
    m_pixelBuffer = nil;
    m_destroyed = true;
    for (auto& commandEncoder : m_commandEncoders)
        commandEncoder.makeSubmitInvalid();

    m_commandEncoders.clear();
}

void ExternalTexture::undestroy()
{
    m_commandEncoders.clear();
    m_destroyed = false;
}

void ExternalTexture::setCommandEncoder(CommandEncoder& commandEncoder) const
{
    m_commandEncoders.add(commandEncoder);
    if (isDestroyed())
        commandEncoder.makeSubmitInvalid();
}

bool ExternalTexture::isDestroyed() const
{
    return m_destroyed;
}

void ExternalTexture::update(CVPixelBufferRef pixelBuffer)
{
#if HAVE(IOSURFACE_SET_OWNERSHIP_IDENTITY) && HAVE(TASK_IDENTITY_TOKEN)
    if (IOSurfaceRef ioSurface = CVPixelBufferGetIOSurface(pixelBuffer)) {
        if (auto optionalWebProcessID = m_device->webProcessID()) {
            if (auto webProcessID = optionalWebProcessID->sendRight())
                IOSurfaceSetOwnershipIdentity(ioSurface, webProcessID, kIOSurfaceMemoryLedgerTagGraphics, 0);
        }
    }
#endif
    m_pixelBuffer = pixelBuffer;
    m_commandEncoders.clear();
    m_destroyed = false;
}

size_t ExternalTexture::openCommandEncoderCount() const
{
    return m_commandEncoders.computeSize();
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuExternalTextureReference(WGPUExternalTexture externalTexture)
{
    WebGPU::fromAPI(externalTexture).ref();
}

void wgpuExternalTextureRelease(WGPUExternalTexture externalTexture)
{
    WebGPU::fromAPI(externalTexture).deref();
}

void wgpuExternalTextureDestroy(WGPUExternalTexture externalTexture)
{
    WebGPU::protectedFromAPI(externalTexture)->destroy();
}

void wgpuExternalTextureUndestroy(WGPUExternalTexture externalTexture)
{
    WebGPU::protectedFromAPI(externalTexture)->undestroy();
}

void wgpuExternalTextureUpdate(WGPUExternalTexture externalTexture, CVPixelBufferRef pixelBuffer)
{
    WebGPU::protectedFromAPI(externalTexture)->update(pixelBuffer);
}
