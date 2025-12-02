/*
 * Copyright (c) 2025 Apple Inc. All rights reserved.
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
#import "DDMesh.h"

#import "APIConversions.h"
#import "Instance.h"
#import "TextureView.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#import <wtf/threads/BinarySemaphore.h>

#if ENABLE(GPU_PROCESS_MODEL)
#import "WebGPUSwiftInternal.h"
#endif

namespace WebGPU {

Ref<DDMesh> Instance::createModelBacking(const WGPUDDCreateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    return DDMesh::create(descriptor, *this);
#else
    UNUSED_PARAM(descriptor);
    return DDMesh::createInvalid(*this);
#endif
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(DDMesh);

DDMesh::DDMesh(const WGPUDDCreateMeshDescriptor& descriptor, Instance& instance)
    : m_instance(instance)
    , m_descriptor(descriptor)
{
    id<MTLDevice> device = instance.device();
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:descriptor.width height:descriptor.height mipmapped:NO];
    m_textures = [NSMutableArray array];
    for (RetainPtr ioSurface : descriptor.ioSurfaces)
        [m_textures addObject:[device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface.get() plane:0]];

#if ENABLE(GPU_PROCESS_MODEL)
    DDBridgeReceiver* ddReceiver = [[DDBridgeReceiver alloc] initWithDevice:instance.device()];
    Ref protectedThis = Ref { *this };
    [ddReceiver initRenderer:m_textures[0] completionHandler:[protectedThis, ddReceiver] mutable {
        protectedThis->m_ddReceiver = ddReceiver;
    }];
#endif
    m_ddMeshIdentifier = [[NSUUID alloc] init];
}

DDMesh::DDMesh(Instance& instance)
    : m_instance(instance)
{
}

bool DDMesh::isValid() const
{
    return true;
}

id<MTLTexture> DDMesh::texture() const
{
    ++m_currentTexture;
    if (m_currentTexture >= m_textures.count)
        m_currentTexture = 0;

    return m_textures.count ? m_textures[m_currentTexture] : nil;
}

DDMesh::~DDMesh() = default;

void DDMesh::render() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    if (id<MTLTexture> modelBacking = texture())
        [m_ddReceiver renderWithTexture:modelBacking];
#endif
}

void DDMesh::update(DDBridgeUpdateMesh* descriptor)
{
    if (!descriptor)
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    BinarySemaphore completion;
    [m_ddReceiver updateMesh:descriptor completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#endif
}

void DDMesh::updateTexture(DDBridgeUpdateTexture* descriptor)
{
    if (!descriptor)
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    BinarySemaphore completion;
    [m_ddReceiver updateTexture:descriptor completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#endif
}

void DDMesh::updateMaterial(DDBridgeUpdateMaterial* descriptor)
{
    if (!descriptor)
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    BinarySemaphore completion;
    [m_ddReceiver updateMaterial:descriptor completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#endif
}

void DDMesh::setTransform(const simd_float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_transform = transform;
    [m_ddReceiver setTransform:transform];
#else
    UNUSED_PARAM(transform);
#endif
}

void DDMesh::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_ddReceiver setCameraDistance:distance];
    render();
#else
    UNUSED_PARAM(distance);
#endif
}

void DDMesh::play(bool play)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_ddReceiver setPlaying:play];
#else
    UNUSED_PARAM(play);
#endif
}

}

#pragma mark WGPU Stubs

void wgpuDDMeshReference(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).ref();
}

void wgpuDDMeshRelease(WGPUDDMesh mesh)
{
    WebGPU::fromAPI(mesh).deref();
}

WGPU_EXPORT void wgpuDDMeshUpdate(WGPUDDMesh mesh, id desc)
{
    WebGPU::protectedFromAPI(mesh)->update(desc);
}

WGPU_EXPORT void wgpuDDMeshRender(WGPUDDMesh mesh)
{
    WebGPU::protectedFromAPI(mesh)->render();
}

WGPU_EXPORT void wgpuDDMeshSetTransform(WGPUDDMesh mesh, const simd_float4x4& transform)
{
    WebGPU::protectedFromAPI(mesh)->setTransform(transform);
}

WGPU_EXPORT void wgpuDDMeshTextureUpdate(WGPUDDMesh mesh, id desc)
{
    WebGPU::protectedFromAPI(mesh)->updateTexture(desc);
}

WGPU_EXPORT void wgpuDDMeshMaterialUpdate(WGPUDDMesh mesh, id desc)
{
    WebGPU::protectedFromAPI(mesh)->updateMaterial(desc);
}

WGPU_EXPORT void wgpuDDMeshSetCameraDistance(WGPUDDMesh mesh, float distance)
{
    WebGPU::protectedFromAPI(mesh)->setCameraDistance(distance);
}

WGPU_EXPORT void wgpuDDMeshPlay(WGPUDDMesh mesh, bool play)
{
    WebGPU::protectedFromAPI(mesh)->play(play);
}
