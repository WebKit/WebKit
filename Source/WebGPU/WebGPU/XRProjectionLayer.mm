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
#import "XRProjectionLayer.h"

#import "APIConversions.h"
#import "Device.h"
#import "MetalSPI.h"
#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

XRProjectionLayer::XRProjectionLayer(bool, Device& device)
    : m_sharedEvent(std::make_pair(nil, 0))
    , m_device(device)
{
    m_colorTextures = [NSMutableDictionary dictionary];
    m_depthTextures = [NSMutableDictionary dictionary];
}

XRProjectionLayer::XRProjectionLayer(Device& device)
    : m_sharedEvent(std::make_pair(nil, 0))
    , m_device(device)
{
}

XRProjectionLayer::~XRProjectionLayer() = default;

void XRProjectionLayer::setLabel(String&&)
{
}

bool XRProjectionLayer::isValid() const
{
    return !!m_colorTextures;
}

id<MTLTexture> XRProjectionLayer::colorTexture() const
{
    return m_colorTexture;
}

id<MTLTexture> XRProjectionLayer::depthTexture() const
{
    return m_depthTexture;
}

const std::pair<id<MTLSharedEvent>, uint64_t>& XRProjectionLayer::completionEvent() const
{
    return m_sharedEvent;
}

size_t XRProjectionLayer::reusableTextureIndex() const
{
    return m_reusableTextureIndex;
}

void XRProjectionLayer::startFrame(size_t frameIndex, WTF::MachSendRight&& colorBuffer, WTF::MachSendRight&& depthBuffer, WTF::MachSendRight&& completionSyncEvent, size_t reusableTextureIndex)
{
#if !PLATFORM(IOS_FAMILY_SIMULATOR) && !PLATFORM(WATCHOS)
    id<MTLDevice> device = m_device->device();
    m_reusableTextureIndex = reusableTextureIndex;
    NSNumber* textureKey = @(reusableTextureIndex);
    if (colorBuffer.sendRight()) {
        MTLSharedTextureHandle* m_colorHandle = [[MTLSharedTextureHandle alloc] initWithMachPort:colorBuffer.sendRight()];
        m_colorTexture = [device newSharedTextureWithHandle:m_colorHandle];
        [m_colorTextures setObject:m_colorTexture forKey:textureKey];
    } else
        m_colorTexture = [m_colorTextures objectForKey:textureKey];

    if (depthBuffer.sendRight()) {
        MTLSharedTextureHandle* m_depthHandle = [[MTLSharedTextureHandle alloc] initWithMachPort:depthBuffer.sendRight()];
        m_depthTexture = [device newSharedTextureWithHandle:m_depthHandle];
        [m_depthTextures setObject:m_depthTexture forKey:textureKey];
    } else
        m_depthTexture = [m_depthTextures objectForKey:textureKey];

    if (completionSyncEvent.sendRight())
        m_sharedEvent = std::make_pair([(id<MTLDeviceSPI>)device newSharedEventWithMachPort:completionSyncEvent.sendRight()], frameIndex);
#else
    UNUSED_PARAM(frameIndex);
    UNUSED_PARAM(colorBuffer);
    UNUSED_PARAM(depthBuffer);
    UNUSED_PARAM(completionSyncEvent);
    UNUSED_PARAM(reusableTextureIndex);
#endif
}

Ref<XRProjectionLayer> XRBinding::createXRProjectionLayer(WGPUTextureFormat colorFormat, WGPUTextureFormat* optionalDepthStencilFormat, WGPUTextureUsageFlags flags, double scale)
{
    UNUSED_PARAM(colorFormat);
    UNUSED_PARAM(optionalDepthStencilFormat);
    UNUSED_PARAM(flags);
    UNUSED_PARAM(scale);

    return XRProjectionLayer::create(device());
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuXRProjectionLayerReference(WGPUXRProjectionLayer projectionLayer)
{
    WebGPU::fromAPI(projectionLayer).ref();
}

void wgpuXRProjectionLayerRelease(WGPUXRProjectionLayer projectionLayer)
{
    WebGPU::fromAPI(projectionLayer).deref();
}

void wgpuXRProjectionLayerStartFrame(WGPUXRProjectionLayer layer, size_t frameIndex, WTF::MachSendRight&& colorBuffer, WTF::MachSendRight&& depthBuffer, WTF::MachSendRight&& completionSyncEvent, size_t reusableTextureIndex)
{
    WebGPU::protectedFromAPI(layer)->startFrame(frameIndex, WTFMove(colorBuffer), WTFMove(depthBuffer), WTFMove(completionSyncEvent), reusableTextureIndex);
}
