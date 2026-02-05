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
#import "Mesh.h"

#import "APIConversions.h"
#import "Instance.h"
#import "ModelTypes.h"
#import "TextureView.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#import <wtf/threads/BinarySemaphore.h>

namespace WebModel {

#if ENABLE(GPU_PROCESS_MODEL)

static WebBridgeMeshPart *convert(const MeshPart& part)
{
    return [[WebBridgeMeshPart alloc] initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:static_cast<MTLPrimitiveType>(part.topology) materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<WebBridgeMeshPart *> *convert(const Vector<MeshPart>& parts)
{
    if (!parts.size())
        return nil;

    NSMutableArray<WebBridgeMeshPart *> *result = [NSMutableArray array];
    for (auto& p : parts)
        [result addObject:convert(p)];

    return result;
}

template<typename T>
static NSData* convert(const Vector<T>& data)
{
    if (!data.size())
        return nil;

    return [[NSData alloc] initWithBytes:data.span().data() length:data.sizeInBytes()];
}

template<typename T>
static NSArray<NSData*> *convert(const Vector<Vector<T>>& data)
{
    if (!data.size())
        return nil;

    NSMutableArray<NSData*> *result = [NSMutableArray array];
    for (auto& v : data) {
        if (NSData *d = convert(v))
            [result addObject:d];
    }

    return result;
}

static NSArray<WebBridgeVertexAttributeFormat *> *convert(const Vector<VertexAttributeFormat>& formats)
{
    if (!formats.size())
        return nil;

    NSMutableArray<WebBridgeVertexAttributeFormat *> *result = [NSMutableArray array];
    for (auto& format : formats)
        [result addObject:[[WebBridgeVertexAttributeFormat alloc] initWithSemantic:format.semantic format:format.format layoutIndex:format.layoutIndex offset:format.offset]];

    return result;
}

static NSArray<WebBridgeVertexLayout *> *convert(const Vector<VertexLayout>& layouts)
{
    if (!layouts.size())
        return nil;

    NSMutableArray<WebBridgeVertexLayout *> *result = [NSMutableArray array];
    for (auto& layout : layouts)
        [result addObject:[[WebBridgeVertexLayout alloc] initWithBufferIndex:layout.bufferIndex bufferOffset:layout.bufferOffset bufferStride:layout.bufferStride]];

    return result;
}

static WebBridgeMeshDescriptor *convert(const MeshDescriptor& descriptor)
{
    if (!descriptor.vertexBufferCount)
        return nil;

    return [[WebBridgeMeshDescriptor alloc] initWithVertexBufferCount:descriptor.vertexBufferCount
        vertexCapacity:descriptor.vertexCapacity
        vertexAttributes:convert(descriptor.vertexAttributes)
        vertexLayouts:convert(descriptor.vertexLayouts)
        indexCapacity:descriptor.indexCapacity
        indexType:static_cast<MTLIndexType>(descriptor.indexType)];
}

static NSArray<NSString *> *convert(const Vector<String>& v)
{
    if (!v.size())
        return nil;

    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (auto& s : v)
        [result addObject:s.createNSString().get()];

    return result;
}

static WebBridgeSkinningData *convert(const std::optional<SkinningData>& data)
{
    if (!data)
        return nil;

    return [[WebBridgeSkinningData alloc] initWithInfluencePerVertexCount:data->influencePerVertexCount jointTransforms:convert(data->jointTransforms) inverseBindPoses:convert(data->inverseBindPoses) influenceJointIndices:convert(data->influenceJointIndices) influenceWeights:convert(data->influenceWeights) geometryBindTransform:data->geometryBindTransform];
}

static WebBridgeBlendShapeData *convert(const std::optional<BlendShapeData>& data)
{
    if (!data)
        return nil;

    return [[WebBridgeBlendShapeData alloc] initWithWeights:convert(data->weights) positionOffsets:convert(data->positionOffsets) normalOffsets:convert(data->normalOffsets)];
}

static WebBridgeRenormalizationData *convert(const std::optional<RenormalizationData>& data)
{
    if (!data)
        return nil;

    return [[WebBridgeRenormalizationData alloc] initWithVertexIndicesPerTriangle:convert(data->vertexIndicesPerTriangle) vertexAdjacencies:convert(data->vertexAdjacencies) vertexAdjacencyEndIndices:convert(data->vertexAdjacencyEndIndices)];
}

static WebBridgeDeformationData *convert(const std::optional<DeformationData>& data)
{
    if (!data)
        return nil;

    return [[WebBridgeDeformationData alloc] initWithSkinningData:convert(data->skinningData) blendShapeData:convert(data->blendShapeData) renormalizationData:convert(data->renormalizationData)];
}

static MTLTextureSwizzleChannels convert(ImageAssetSwizzle swizzle)
{
    return MTLTextureSwizzleChannels {
        .red = static_cast<MTLTextureSwizzle>(swizzle.red),
        .green = static_cast<MTLTextureSwizzle>(swizzle.green),
        .blue = static_cast<MTLTextureSwizzle>(swizzle.blue),
        .alpha = static_cast<MTLTextureSwizzle>(swizzle.alpha)
    };
}

static WebBridgeImageAsset* convert(const ImageAsset& imageAsset)
{
    MTLPixelFormat mtlPixelFormat = static_cast<MTLPixelFormat>(imageAsset.pixelFormat);
    using namespace WebGPU;

    return [[WebBridgeImageAsset alloc] initWithData:convert(imageAsset.data) width:imageAsset.width height:imageAsset.height depth:imageAsset.depth bytesPerPixel:imageAsset.bytesPerPixel ?: Texture::texelBlockSize(Texture::textureFormat(mtlPixelFormat)).value() textureType:static_cast<MTLTextureType>(imageAsset.textureType) pixelFormat:mtlPixelFormat mipmapLevelCount:imageAsset.mipmapLevelCount arrayLength:imageAsset.arrayLength textureUsage:static_cast<MTLTextureUsage>(imageAsset.textureUsage) swizzle:convert(imageAsset.swizzle)];
}

#endif

WTF_MAKE_TZONE_ALLOCATED_IMPL(Mesh);

Mesh::Mesh(const WebModelCreateMeshDescriptor& descriptor, WebGPU::Instance& instance)
    : m_instance(instance)
    , m_descriptor(descriptor)
{
    id<MTLDevice> device = instance.device();
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB width:descriptor.width height:descriptor.height mipmapped:NO];
    m_textures = [NSMutableArray array];
    for (RetainPtr ioSurface : descriptor.ioSurfaces)
        [m_textures addObject:[device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface.get() plane:0]];

#if ENABLE(GPU_PROCESS_MODEL)
    WebUSDConfiguration *configuration = [[WebUSDConfiguration alloc] initWithDevice:device];
    WebBridgeImageAsset *diffuseAsset = convert(descriptor.diffuseTexture);
    WebBridgeImageAsset *specularAsset = convert(descriptor.specularTexture);
    BinarySemaphore completion;
    [configuration createMaterialCompiler:[&completion] mutable {
        completion.signal();
    }];
    completion.wait();

    NSError *error;
    m_ddReceiver = [[WebBridgeReceiver alloc] initWithConfiguration:configuration diffuseAsset:diffuseAsset specularAsset:specularAsset error:&error];
    if (error)
        WTFLogAlways("Could not initialize USD renderer"); // NOLINT

    m_meshIdentifier = [[NSUUID alloc] init];
#endif
}

bool Mesh::isValid() const
{
    return true;
}

id<MTLTexture> Mesh::texture() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    ++m_currentTexture;
    if (m_currentTexture >= m_textures.count)
        m_currentTexture = 0;

    return m_textures.count ? m_textures[m_currentTexture] : nil;
#else
    return nil;
#endif
}

Mesh::~Mesh() = default;

void Mesh::render() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    processUpdates();
    if (!m_meshDataExists)
        return;

    if (id<MTLTexture> modelBacking = texture())
        [m_ddReceiver renderWithTexture:modelBacking];
#endif
}

void Mesh::update(WebBridgeUpdateMesh* descriptor)
{
    if (!descriptor)
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    if (!m_batchedUpdates)
        m_batchedUpdates = [NSMutableDictionary dictionary];

    [m_batchedUpdates setObject:descriptor forKey:descriptor.identifier];
#endif
}

void Mesh::processUpdates() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    for (WebBridgeUpdateMesh *descriptor in m_batchedUpdates.allValues) {
        BinarySemaphore completion;
        RELEASE_ASSERT(m_ddReceiver);
        [m_ddReceiver updateMesh:descriptor completionHandler:[&] mutable {
            completion.signal();
        }];
        completion.wait();
        m_meshDataExists = true;
    }
    [m_batchedUpdates removeAllObjects];
#endif
}

void Mesh::updateTexture(WebBridgeUpdateTexture* descriptor)
{
    if (!descriptor)
        return;

#if ENABLE(GPU_PROCESS_MODEL)
    RELEASE_ASSERT(m_ddReceiver);
    [m_ddReceiver updateTexture:descriptor];
#endif
}

void Mesh::updateMaterial(const WebModel::UpdateMaterialDescriptor& originalDescriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WebBridgeUpdateMaterial *descriptor = [[WebBridgeUpdateMaterial alloc] initWithMaterialGraph:WebModel::convert(originalDescriptor.materialGraph) identifier:originalDescriptor.identifier.createNSString().get() geometryModifierFunctionReference:nil surfaceShaderFunctionReference:nil shaderGraphModule:nil];
    if (!descriptor)
        return;

    RELEASE_ASSERT(m_ddReceiver);
    BinarySemaphore completion;
    [m_ddReceiver updateMaterial:descriptor completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#else
    UNUSED_PARAM(originalDescriptor);
#endif
}

void Mesh::setTransform(const simd_float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_transform = transform;
    [m_ddReceiver setTransform:transform];
#else
    UNUSED_PARAM(transform);
#endif
}

void Mesh::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_ddReceiver setCameraDistance:distance];
    render();
#else
    UNUSED_PARAM(distance);
#endif
}

void Mesh::setEnvironmentMap(WebBridgeImageAsset *imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_ddReceiver setEnvironmentMap:imageAsset];
#else
    UNUSED_PARAM(imageAsset);
#endif
}

void Mesh::play(bool play)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_ddReceiver setPlaying:play];
#else
    UNUSED_PARAM(play);
#endif
}

}

namespace WebGPU {
Ref<WebModel::Mesh> Instance::createModelBacking(const WebModelCreateMeshDescriptor& descriptor)
{
    return WebModel::Mesh::create(descriptor, *this);
}
}

#pragma mark WGPU Stubs

void webModelMeshReference(WebMesh mesh)
{
    WebGPU::fromAPI(mesh).ref();
}

void webModelMeshRelease(WebMesh mesh)
{
    WebGPU::fromAPI(mesh).deref();
}

WGPU_EXPORT void webModelMeshUpdate(WebMesh mesh, const WebModel::UpdateMeshDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WebBridgeUpdateMesh *o = [[WebBridgeUpdateMesh alloc] initWithIdentifier:descriptor.identifier.createNSString().get()
        updateType:static_cast<WebBridgeDataUpdateType>(descriptor.updateType)
        descriptor:convert(descriptor.descriptor)
        parts:convert(descriptor.parts)
        indexData:WebModel::convert(descriptor.indexData)
        vertexData:WebModel::convert(descriptor.vertexData)
        instanceTransforms:WebModel::convert(descriptor.instanceTransforms)
        instanceTransformsCount:descriptor.instanceTransforms.size()
        materialPrims:WebModel::convert(descriptor.materialPrims)
        deformationData:WebModel::convert(descriptor.deformationData)];

    WebGPU::protectedFromAPI(mesh)->update(o);
#else
    UNUSED_PARAM(mesh);
    UNUSED_PARAM(descriptor);
#endif
}

WGPU_EXPORT void webModelMeshRender(WebMesh mesh)
{
    WebGPU::protectedFromAPI(mesh)->render();
}

WGPU_EXPORT void webModelMeshSetTransform(WebMesh mesh, const simd_float4x4& transform)
{
    WebGPU::protectedFromAPI(mesh)->setTransform(transform);
}

WGPU_EXPORT void webModelMeshTextureUpdate(WebMesh mesh, const WebModel::UpdateTextureDescriptor& descriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WebBridgeUpdateTexture *o = [[WebBridgeUpdateTexture alloc] initWithImageAsset:convert(descriptor.imageAsset) identifier:descriptor.identifier.createNSString().get() hashString:descriptor.hashString.createNSString().get()];

    WebGPU::protectedFromAPI(mesh)->updateTexture(o);
#else
    UNUSED_PARAM(mesh);
    UNUSED_PARAM(descriptor);
#endif
}

WGPU_EXPORT void webModelMeshMaterialUpdate(WebMesh mesh, const WebModel::UpdateMaterialDescriptor& descriptor)
{
    WebGPU::protectedFromAPI(mesh)->updateMaterial(descriptor);
}

WGPU_EXPORT void webModelMeshSetEnvironmentMap(WebMesh mesh, const WebModel::ImageAsset& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WebGPU::protectedFromAPI(mesh)->setEnvironmentMap(WebModel::convert(imageAsset));
#else
    UNUSED_PARAM(mesh);
    UNUSED_PARAM(imageAsset);
#endif
}

WGPU_EXPORT void webModelMeshSetCameraDistance(WebMesh mesh, float distance)
{
    WebGPU::protectedFromAPI(mesh)->setCameraDistance(distance);
}

WGPU_EXPORT void webModelMeshPlay(WebMesh mesh, bool play)
{
    WebGPU::protectedFromAPI(mesh)->play(play);
}
