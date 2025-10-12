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

#if ENABLE(WEBGPU_SWIFT)
#import "WebGPUSwiftInternal.h"
#endif

namespace WebGPU {

#if ENABLE(WEBGPU_SWIFT)
static NSArray<DDBridgeVertexAttributeFormat*>* convertAttributes(const Vector<WGPUDDVertexAttributeFormat>& vertexAttributes)
{
    NSMutableArray<DDBridgeVertexAttributeFormat*>* result = [NSMutableArray array];
    for (auto& a : vertexAttributes)
        [result addObject:[[DDBridgeVertexAttributeFormat alloc] initWithSemantic:a.semantic format:a.format layoutIndex:a.layoutIndex offset:a.offset]];

    return result;
}
static NSArray<DDBridgeVertexLayout*>* convertLayouts(const Vector<WGPUDDVertexLayout>& layouts)
{
    NSMutableArray<DDBridgeVertexLayout*>* result = [NSMutableArray array];
    for (auto& a : layouts)
        [result addObject:[[DDBridgeVertexLayout alloc] initWithBufferIndex:a.bufferIndex bufferOffset:a.bufferOffset bufferStride:a.bufferStride]];

    return result;
}

static DDBridgeAddMeshRequest* convertDescriptor(const WGPUDDMeshDescriptor& descriptor)
{
    DDBridgeAddMeshRequest* result = [[DDBridgeAddMeshRequest alloc] initWithIndexCapacity:descriptor.indexCapacity
        indexType:descriptor.indexType
        vertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convertAttributes(descriptor.vertexAttributes)
        vertexLayouts:convertLayouts(descriptor.vertexLayouts)
    ];

    return result;
}
#endif

Ref<DDMesh> Instance::createModelBacking(const WGPUDDCreateMeshDescriptor& descriptor)
{
#if ENABLE(WEBGPU_SWIFT)
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

#if ENABLE(WEBGPU_SWIFT)
    m_ddReceiver = [[DDBridgeReceiver alloc] init];
    [m_ddReceiver setDeviceWithDevice:instance.device()];
#endif
    m_ddMeshIdentifier = [[NSUUID alloc] initWithUUIDString:@"E621E1F8-C36C-495A-93FC-0C247A3E6E5F"];
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


#if ENABLE(WEBGPU_SWIFT)
static DDBridgeChainedFloat4x4 *convertFloats(const Vector<simd_float4x4>& fs)
{
    auto count = fs.size();
    if (!count)
        return nil;

    DDBridgeChainedFloat4x4 *result = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[0]];
    DDBridgeChainedFloat4x4 *current = result;

    for (uint32_t i = 1; i < count; ++i) {
        DDBridgeChainedFloat4x4 *next = [[DDBridgeChainedFloat4x4 alloc] initWithTransform:fs[i]];
        current.next = next;
    }

    return result;
}

static DDBridgeMeshPart *convertPart(const WGPUDDMeshPart& part)
{
    return [[DDBridgeMeshPart alloc] initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:part.topology materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<DDBridgeSetPart*> *convertParts(const Vector<KeyValuePair<int32_t, WGPUDDMeshPart>>& parts)
{
    NSMutableArray<DDBridgeSetPart*>* result = [NSMutableArray array];
    for (auto& kvp : parts) {
        DDBridgeSetPart* part = [[DDBridgeSetPart alloc] initWithIndex:kvp.key part:convertPart(kvp.value)];
        [result addObject:part];
    }
    return result;
}

static NSArray<NSUUID *> *convertMaterialIDs(const Vector<String>& ids)
{
    NSMutableArray<NSUUID *>* result = [NSMutableArray array];
    for (auto& i : ids)
        [result addObject:[[NSUUID alloc] initWithUUIDString:i.createNSString().get()]];
    return result;
}

static NSData* convertUint8s(const Vector<uint8_t>& data)
{
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

static NSArray<DDBridgeSetRenderFlags*> *convertRenderFlags(const Vector<KeyValuePair<int32_t, uint64_t>>& renderFlags)
{
    NSMutableArray<DDBridgeSetRenderFlags*>* result = [NSMutableArray array];
    for (auto& r : renderFlags)
        [result addObject:[[DDBridgeSetRenderFlags alloc] initWithIndex:r.key renderFlags:r.value]];
    return result;
}

static NSArray<DDBridgeReplaceVertices*> *convertVertices(const Vector<WGPUDDReplaceVertices>& vertices)
{
    NSMutableArray<DDBridgeReplaceVertices*>* result = [NSMutableArray array];
    for (auto& v : vertices)
        [result addObject:[[DDBridgeReplaceVertices alloc] initWithBufferIndex:v.bufferIndex buffer:convertUint8s(v.buffer)]];

    return result;
}

static DDBridgeUpdateMesh *convertDescriptor(WGPUDDUpdateMeshDescriptor& descriptor)
{
    DDBridgeUpdateMesh *result = [[DDBridgeUpdateMesh alloc] initWithPartCount:descriptor.partCount
        parts:convertParts(descriptor.parts)
        renderFlags:convertRenderFlags(descriptor.renderFlags)
        vertices:convertVertices(descriptor.vertices)
        indices:convertUint8s(descriptor.indices)
        transform:descriptor.transform
        instanceTransforms:convertFloats(descriptor.instanceTransforms4x4)
        materialIds:convertMaterialIDs(descriptor.materialIds)];

    return result;
}
#endif

void DDMesh::render() const
{
#if ENABLE(WEBGPU_SWIFT)
    if (id<MTLTexture> modelBacking = texture())
        [m_ddReceiver renderWithTexture:modelBacking];
#endif
}

void DDMesh::update(WGPUDDUpdateMeshDescriptor* desc)
{
#if ENABLE(WEBGPU_SWIFT)
    if (desc) {
        [m_ddReceiver updateMesh:convertDescriptor(*desc) identifier:m_ddMeshIdentifier];
        render();
    }
#else
    UNUSED_PARAM(desc);
#endif
}

void DDMesh::addMesh(WGPUDDMeshDescriptor* desc)
{
    if (!desc)
        return;

#if ENABLE(WEBGPU_SWIFT)
    auto& descriptor = *desc;
    [m_ddReceiver addMesh:convertDescriptor(descriptor) identifier:m_ddMeshIdentifier];
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

WGPU_EXPORT void wgpuDDMeshUpdate(WGPUDDMesh mesh, WGPUDDUpdateMeshDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->update(desc);
}

WGPU_EXPORT void wgpuDDMeshAdd(WGPUDDMesh mesh, WGPUDDMeshDescriptor* desc)
{
    WebGPU::protectedFromAPI(mesh)->addMesh(desc);
}

WGPU_EXPORT void wgpuDDMeshRender(WGPUDDMesh mesh)
{
    WebGPU::protectedFromAPI(mesh)->render();
}
