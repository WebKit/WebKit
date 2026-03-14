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
#import "WebKitMesh.h"

#import "ModelTypes.h"

#import <WebCore/ProcessIdentity.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/MathExtras.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/spi/cocoa/IOSurfaceSPI.h>
#import <wtf/threads/BinarySemaphore.h>

#import "WebKitSwiftSoftLink.h"

namespace WebModel {

#if ENABLE(GPU_PROCESS_MODEL)

static WKBridgeConstant convert(const Constant constant)
{
    switch (constant) {
    case Constant::kBool:
        return WKBridgeConstantBool;
    case Constant::kUchar:
        return WKBridgeConstantUchar;
    case Constant::kInt:
        return WKBridgeConstantInt;
    case Constant::kUint:
        return WKBridgeConstantUint;
    case Constant::kHalf:
        return WKBridgeConstantHalf;
    case Constant::kFloat:
        return WKBridgeConstantFloat;
    case Constant::kTimecode:
        return WKBridgeConstantTimecode;
    case Constant::kString:
        return WKBridgeConstantString;
    case Constant::kToken:
        return WKBridgeConstantToken;
    case Constant::kAsset:
        return WKBridgeConstantAsset;
    case Constant::kMatrix2f:
        return WKBridgeConstantMatrix2f;
    case Constant::kMatrix3f:
        return WKBridgeConstantMatrix3f;
    case Constant::kMatrix4f:
        return WKBridgeConstantMatrix4f;
    case Constant::kQuatf:
        return WKBridgeConstantQuatf;
    case Constant::kQuath:
        return WKBridgeConstantQuath;
    case Constant::kFloat2:
        return WKBridgeConstantFloat2;
    case Constant::kHalf2:
        return WKBridgeConstantHalf2;
    case Constant::kInt2:
        return WKBridgeConstantInt2;
    case Constant::kFloat3:
        return WKBridgeConstantFloat3;
    case Constant::kHalf3:
        return WKBridgeConstantHalf3;
    case Constant::kInt3:
        return WKBridgeConstantInt3;
    case Constant::kFloat4:
        return WKBridgeConstantFloat4;
    case Constant::kHalf4:
        return WKBridgeConstantHalf4;
    case Constant::kInt4:
        return WKBridgeConstantInt4;

    // semantic:
    case Constant::kPoint3f:
        return WKBridgeConstantPoint3f;
    case Constant::kPoint3h:
        return WKBridgeConstantPoint3h;
    case Constant::kNormal3f:
        return WKBridgeConstantNormal3f;
    case Constant::kNormal3h:
        return WKBridgeConstantNormal3h;
    case Constant::kVector3f:
        return WKBridgeConstantVector3f;
    case Constant::kVector3h:
        return WKBridgeConstantVector3h;
    case Constant::kColor3f:
        return WKBridgeConstantColor3f;
    case Constant::kColor3h:
        return WKBridgeConstantColor3h;
    case Constant::kColor4f:
        return WKBridgeConstantColor4f;
    case Constant::kColor4h:
        return WKBridgeConstantColor4h;
    case Constant::kTexCoord2h:
        return WKBridgeConstantTexCoord2h;
    case Constant::kTexCoord2f:
        return WKBridgeConstantTexCoord2f;
    case Constant::kTexCoord3h:
        return WKBridgeConstantTexCoord3h;
    case Constant::kTexCoord3f:
        return WKBridgeConstantTexCoord3f;
    }
}


static WKBridgeMeshPart *convert(const MeshPart& part)
{
    return [WebKit::allocWKBridgeMeshPartInstance() initWithIndexOffset:part.indexOffset indexCount:part.indexCount topology:static_cast<MTLPrimitiveType>(part.topology) materialIndex:part.materialIndex boundsMin:part.boundsMin boundsMax:part.boundsMax];
}

static NSArray<WKBridgeMeshPart *> *convert(const Vector<MeshPart>& parts)
{
    if (!parts.size())
        return nil;

    NSMutableArray<WKBridgeMeshPart *> *result = [NSMutableArray array];
    for (const auto& p : parts)
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
    for (const auto& v : data) {
        if (NSData *d = convert(v))
            [result addObject:d];
    }

    return result;
}

static NSArray<WKBridgeVertexAttributeFormat *> *convert(const Vector<VertexAttributeFormat>& formats)
{
    if (!formats.size())
        return nil;

    NSMutableArray<WKBridgeVertexAttributeFormat *> *result = [NSMutableArray array];
    for (const auto& format : formats)
        [result addObject:[WebKit::allocWKBridgeVertexAttributeFormatInstance() initWithSemantic:format.semantic format:format.format layoutIndex:format.layoutIndex offset:format.offset]];

    return result;
}

static NSArray<WKBridgeVertexLayout *> *convert(const Vector<VertexLayout>& layouts)
{
    if (!layouts.size())
        return nil;

    NSMutableArray<WKBridgeVertexLayout *> *result = [NSMutableArray array];
    for (const auto& layout : layouts)
        [result addObject:[WebKit::allocWKBridgeVertexLayoutInstance() initWithBufferIndex:layout.bufferIndex bufferOffset:layout.bufferOffset bufferStride:layout.bufferStride]];

    return result;
}

static WKBridgeMeshDescriptor *convert(const MeshDescriptor& descriptor)
{
    if (!descriptor.vertexBufferCount)
        return nil;

    return [WebKit::allocWKBridgeMeshDescriptorInstance() initWithVertexBufferCount:descriptor.vertexBufferCount
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
    for (const auto& s : v)
        [result addObject:s.createNSString().get()];

    return result;
}

static WKBridgeSkinningData *convert(const std::optional<SkinningData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeSkinningDataInstance() initWithInfluencePerVertexCount:data->influencePerVertexCount jointTransforms:convert(data->jointTransforms) inverseBindPoses:convert(data->inverseBindPoses) influenceJointIndices:convert(data->influenceJointIndices) influenceWeights:convert(data->influenceWeights) geometryBindTransform:data->geometryBindTransform];
}

static WKBridgeBlendShapeData *convert(const std::optional<BlendShapeData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeBlendShapeDataInstance() initWithWeights:convert(data->weights) positionOffsets:convert(data->positionOffsets) normalOffsets:convert(data->normalOffsets)];
}

static WKBridgeRenormalizationData *convert(const std::optional<RenormalizationData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeRenormalizationDataInstance() initWithVertexIndicesPerTriangle:convert(data->vertexIndicesPerTriangle) vertexAdjacencies:convert(data->vertexAdjacencies) vertexAdjacencyEndIndices:convert(data->vertexAdjacencyEndIndices)];
}

static WKBridgeDeformationData *convert(const std::optional<DeformationData>& data)
{
    if (!data)
        return nil;

    return [WebKit::allocWKBridgeDeformationDataInstance() initWithSkinningData:convert(data->skinningData) blendShapeData:convert(data->blendShapeData) renormalizationData:convert(data->renormalizationData)];
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

static uint32_t texelBlockSize(MTLPixelFormat format)
{
    switch (format) {
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
        return 1;
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
        return 2;
    case MTLPixelFormatR32Float:
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG16Snorm:
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatBGRA8Unorm:
    case MTLPixelFormatBGRA8Unorm_sRGB:
    case MTLPixelFormatRGB10A2Unorm:
    case MTLPixelFormatRG11B10Float:
    case MTLPixelFormatRGB9E5Float:
    case MTLPixelFormatRGB10A2Uint:
        return 4;
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
        return 8;
    case MTLPixelFormatRGBA32Float:
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
        return 16;
    case MTLPixelFormatStencil8:
        return 1;
    case MTLPixelFormatDepth16Unorm:
        return 2;
    case MTLPixelFormatDepth32Float:
        return 4;
    case MTLPixelFormatDepth32Float_Stencil8:
        ASSERT_NOT_REACHED();
        return 0;
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC1_RGBA_sRGB:
        return 8;
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC2_RGBA_sRGB:
        return 16;
    case MTLPixelFormatBC3_RGBA:
    case MTLPixelFormatBC3_RGBA_sRGB:
        return 16;
    case MTLPixelFormatBC4_RUnorm:
    case MTLPixelFormatBC4_RSnorm:
        return 8;
    case MTLPixelFormatBC5_RGUnorm:
    case MTLPixelFormatBC5_RGSnorm:
        return 16;
    case MTLPixelFormatBC6H_RGBUfloat:
    case MTLPixelFormatBC6H_RGBFloat:
        return 16;
    case MTLPixelFormatBC7_RGBAUnorm:
    case MTLPixelFormatBC7_RGBAUnorm_sRGB:
        return 16;
    case MTLPixelFormatETC2_RGB8:
    case MTLPixelFormatETC2_RGB8_sRGB:
    case MTLPixelFormatETC2_RGB8A1:
    case MTLPixelFormatETC2_RGB8A1_sRGB:
        return 8;
    case MTLPixelFormatEAC_R11Unorm:
    case MTLPixelFormatEAC_R11Snorm:
        return 8;
    case MTLPixelFormatEAC_RGBA8:
    case MTLPixelFormatEAC_RGBA8_sRGB:
    case MTLPixelFormatEAC_RG11Unorm:
    case MTLPixelFormatEAC_RG11Snorm:
        return 16;
    case MTLPixelFormatASTC_4x4_sRGB:
    case MTLPixelFormatASTC_4x4_LDR:
    case MTLPixelFormatASTC_5x4_sRGB:
    case MTLPixelFormatASTC_5x4_LDR:
    case MTLPixelFormatASTC_5x5_sRGB:
    case MTLPixelFormatASTC_5x5_LDR:
    case MTLPixelFormatASTC_6x5_sRGB:
    case MTLPixelFormatASTC_6x5_LDR:
    case MTLPixelFormatASTC_6x6_sRGB:
    case MTLPixelFormatASTC_6x6_LDR:
    case MTLPixelFormatASTC_8x5_sRGB:
    case MTLPixelFormatASTC_8x5_LDR:
    case MTLPixelFormatASTC_8x6_sRGB:
    case MTLPixelFormatASTC_8x6_LDR:
    case MTLPixelFormatASTC_8x8_sRGB:
    case MTLPixelFormatASTC_8x8_LDR:
    case MTLPixelFormatASTC_10x5_sRGB:
    case MTLPixelFormatASTC_10x5_LDR:
    case MTLPixelFormatASTC_10x6_sRGB:
    case MTLPixelFormatASTC_10x6_LDR:
    case MTLPixelFormatASTC_10x8_sRGB:
    case MTLPixelFormatASTC_10x8_LDR:
    case MTLPixelFormatASTC_10x10_sRGB:
    case MTLPixelFormatASTC_10x10_LDR:
    case MTLPixelFormatASTC_12x10_sRGB:
    case MTLPixelFormatASTC_12x10_LDR:
    case MTLPixelFormatASTC_12x12_sRGB:
    case MTLPixelFormatASTC_12x12_LDR:
        return 16;
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static WKBridgeImageAsset* convert(const ImageAsset& imageAsset)
{
    MTLPixelFormat mtlPixelFormat = static_cast<MTLPixelFormat>(imageAsset.pixelFormat);

    return [WebKit::allocWKBridgeImageAssetInstance() initWithData:convert(imageAsset.data) width:imageAsset.width height:imageAsset.height depth:imageAsset.depth bytesPerPixel:imageAsset.bytesPerPixel ?: texelBlockSize(mtlPixelFormat) textureType:static_cast<MTLTextureType>(imageAsset.textureType) pixelFormat:mtlPixelFormat mipmapLevelCount:imageAsset.mipmapLevelCount arrayLength:imageAsset.arrayLength textureUsage:static_cast<MTLTextureUsage>(imageAsset.textureUsage) swizzle:convert(imageAsset.swizzle)];
}

static WKBridgeDataType convert(DataType type)
{
    switch (type) {
    case DataType::kBool:
        return WKBridgeDataTypeBool;
    case DataType::kUchar:
        return WKBridgeDataTypeUchar;
    case DataType::kInt:
        return WKBridgeDataTypeInt;
    case DataType::kUint:
        return WKBridgeDataTypeUint;
    case DataType::kInt2:
        return WKBridgeDataTypeInt2;
    case DataType::kInt3:
        return WKBridgeDataTypeInt3;
    case DataType::kInt4:
        return WKBridgeDataTypeInt4;
    case DataType::kFloat:
        return WKBridgeDataTypeFloat;
    case DataType::kColor3f:
        return WKBridgeDataTypeColor3f;
    case DataType::kColor3h:
        return WKBridgeDataTypeColor3h;
    case DataType::kColor4f:
        return WKBridgeDataTypeColor4f;
    case DataType::kColor4h:
        return WKBridgeDataTypeColor4h;
    case DataType::kFloat2:
        return WKBridgeDataTypeFloat2;
    case DataType::kFloat3:
        return WKBridgeDataTypeFloat3;
    case DataType::kFloat4:
        return WKBridgeDataTypeFloat4;
    case DataType::kHalf:
        return WKBridgeDataTypeHalf;
    case DataType::kHalf2:
        return WKBridgeDataTypeHalf2;
    case DataType::kHalf3:
        return WKBridgeDataTypeHalf3;
    case DataType::kHalf4:
        return WKBridgeDataTypeHalf4;
    case DataType::kMatrix2f:
        return WKBridgeDataTypeMatrix2f;
    case DataType::kMatrix3f:
        return WKBridgeDataTypeMatrix3f;
    case DataType::kMatrix4f:
        return WKBridgeDataTypeMatrix4f;
    case DataType::kMatrix2h:
        return WKBridgeDataTypeMatrix2h;
    case DataType::kMatrix3h:
        return WKBridgeDataTypeMatrix3h;
    case DataType::kMatrix4h:
        return WKBridgeDataTypeMatrix4h;
    case DataType::kQuat:
        return WKBridgeDataTypeQuat;
    case DataType::kSurfaceShader:
        return WKBridgeDataTypeSurfaceShader;
    case DataType::kGeometryModifier:
        return WKBridgeDataTypeGeometryModifier;
    case DataType::kPostLightingShader:
        return WKBridgeDataTypePostLightingShader;
    case DataType::kString:
        return WKBridgeDataTypeString;
    case DataType::kToken:
        return WKBridgeDataTypeToken;
    case DataType::kAsset:
        return WKBridgeDataTypeAsset;
    }
}

static NSArray<WKBridgeValueString *> *convert(const Vector<Variant<String, double>>& constantValues)
{
    NSMutableArray<WKBridgeValueString *> *result = [NSMutableArray array];
    for (const auto& c : constantValues) {
        [result addObject:WTF::switchOn(c, [&](const String& s) -> WKBridgeValueString * {
            return [WebKit::allocWKBridgeValueStringInstance() initWithString:s.createNSString().get()];
        }, [&] (double d) -> WKBridgeValueString * {
            return [WebKit::allocWKBridgeValueStringInstance() initWithNumber:[[NSNumber alloc] initWithDouble:d]];
        })];
    }

    return result;
}

static WKBridgeConstantContainer *convert(const ConstantContainer& constant)
{
    return [WebKit::allocWKBridgeConstantContainerInstance() initWithConstant:convert(constant.constant) constantValues:convert(constant.constantValues) name:constant.name.createNSString().get()];
}

static NSArray<WKBridgeInputOutput *> *convert(const Vector<InputOutput>& inputOutputs)
{
    NSMutableArray<WKBridgeInputOutput *> *result = [NSMutableArray array];
    for (const auto& io : inputOutputs) {
        WKBridgeDataType semanticType = WKBridgeDataTypeAsset;
        BOOL hasSemanticType = NO;
        if (io.semanticType) {
            semanticType = convert(*io.semanticType);
            hasSemanticType = YES;
        }

        WKBridgeConstantContainer *defaultValue = nil;
        if (io.defaultValue)
            defaultValue = convert(*io.defaultValue);

        [result addObject:[WebKit::allocWKBridgeInputOutputInstance() initWithType:convert(io.type)
            name:io.name.createNSString().get()
            semanticType:semanticType
            hasSemanticType:hasSemanticType
            defaultValue:defaultValue]];
    }

    return result;
}

static NSArray<WKBridgeEdge *> *convert(const Vector<Edge>& edges)
{
    NSMutableArray<WKBridgeEdge *> *result = [NSMutableArray array];
    for (const auto& e : edges) {
        [result addObject:[WebKit::allocWKBridgeEdgeInstance() initWithOutputNode:e.outputNode.createNSString().get()
            outputPort:e.outputPort.createNSString().get()
            inputNode:e.inputNode.createNSString().get()
            inputPort:e.inputPort.createNSString().get()]];
    }

    return result;
}

static WKBridgeNodeType convert(NodeType bridgeNodeType)
{
    switch (bridgeNodeType) {
    case NodeType::Builtin:
        return WKBridgeNodeTypeBuiltin;
    case NodeType::Constant:
        return WKBridgeNodeTypeConstant;
    case NodeType::Arguments:
        return WKBridgeNodeTypeArguments;
    case NodeType::Results:
        return WKBridgeNodeTypeResults;
    }
}

static WKBridgeBuiltin *convert(const Builtin& builtin)
{
    return [WebKit::allocWKBridgeBuiltinInstance() initWithDefinition:builtin.definition.createNSString().get() name:builtin.name.createNSString().get()];
}

static WKBridgeNode *convert(const Node& node)
{
    return [WebKit::allocWKBridgeNodeInstance() initWithBridgeNodeType:convert(node.bridgeNodeType) builtin:convert(node.builtin) constant:convert(node.constant)];
}

static NSArray<WKBridgeNode *> *convert(const Vector<Node>& nodes)
{
    NSMutableArray<WKBridgeNode *> *result = [NSMutableArray array];
    for (const auto& node : nodes)
        [result addObject:convert(node)];
    return result;
}

static WKBridgeMaterialGraph *convert(const MaterialGraph& material)
{
    return [WebKit::allocWKBridgeMaterialGraphInstance() initWithNodes:convert(material.nodes) edges:convert(material.edges) arguments:convert(material.arguments) results:convert(material.results) inputs:convert(material.inputs) outputs:convert(material.outputs)];
}

#endif

} // namespace WebModel

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebMesh);

WebMesh::WebMesh(const WebModelCreateMeshDescriptor& descriptor)
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm_sRGB width:descriptor.width height:descriptor.height mipmapped:NO];
    m_textures = [NSMutableArray array];
    for (RetainPtr ioSurface : descriptor.ioSurfaces)
        [m_textures addObject:[device newTextureWithDescriptor:textureDescriptor iosurface:ioSurface.get() plane:0]];

#if ENABLE(GPU_PROCESS_MODEL)
    WKBridgeUSDConfiguration *configuration = [WebKit::allocWKBridgeUSDConfigurationInstance() initWithDevice:device memoryOwner:descriptor.processIdentity ? descriptor.processIdentity->taskIdToken() : 0];
    WKBridgeImageAsset *diffuseAsset = WebModel::convert(descriptor.diffuseTexture);
    WKBridgeImageAsset *specularAsset = WebModel::convert(descriptor.specularTexture);
    if (configuration) {
        BinarySemaphore completion;
        [configuration createMaterialCompiler:[&completion] mutable {
            completion.signal();
        }];
        completion.wait();
    }

    NSError *error;
    m_receiver = [WebKit::allocWKBridgeReceiverInstance() initWithConfiguration:configuration diffuseAsset:diffuseAsset specularAsset:specularAsset error:&error];
    if (error)
        WTFLogAlways("Could not initialize USD renderer"); // NOLINT

    m_meshIdentifier = [[NSUUID alloc] init];
#endif
}

bool WebMesh::isValid() const
{
    return true;
}

WebMesh::~WebMesh() = default;

void WebMesh::render() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    processUpdates();
    if (!m_meshDataExists)
        return;

    auto texture = ^{
        ++m_currentTexture;
        if (m_currentTexture >= [m_textures count])
            m_currentTexture = 0;

        return [m_textures count] ? [m_textures objectAtIndex:m_currentTexture] : nil;
    };

    if (id<MTLTexture> modelBacking = texture())
        [m_receiver renderWithTexture:modelBacking];
#endif
}

void WebMesh::update(const WebModel::UpdateMeshDescriptor& input)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WKBridgeUpdateMesh *descriptor = [WebKit::allocWKBridgeUpdateMeshInstance() initWithIdentifier:input.identifier.createNSString().get()
        updateType:static_cast<WKBridgeDataUpdateType>(input.updateType)
        descriptor:WebModel::convert(input.descriptor)
        parts:WebModel::convert(input.parts)
        indexData:WebModel::convert(input.indexData)
        vertexData:WebModel::convert(input.vertexData)
        instanceTransforms:WebModel::convert(input.instanceTransforms)
        instanceTransformsCount:input.instanceTransforms.size()
        materialPrims:WebModel::convert(input.materialPrims)
        deformationData:WebModel::convert(input.deformationData)];

    if (!descriptor)
        return;

    if (!m_batchedUpdates)
        m_batchedUpdates = [NSMutableDictionary dictionary];

    [m_batchedUpdates setObject:descriptor forKey:descriptor.identifier];
#else
    UNUSED_PARAM(input);
#endif
}

void WebMesh::processUpdates() const
{
#if ENABLE(GPU_PROCESS_MODEL)
    for (WKBridgeUpdateMesh *descriptor in [m_batchedUpdates allValues]) {
        BinarySemaphore completion;
        RELEASE_ASSERT(m_receiver);
        [m_receiver updateMesh:descriptor completionHandler:[&] mutable {
            completion.signal();
        }];
        completion.wait();
        m_meshDataExists = true;
    }
    [m_batchedUpdates removeAllObjects];
#endif
}

void WebMesh::updateTexture(const WebModel::UpdateTextureDescriptor& input)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WKBridgeUpdateTexture *descriptor = [WebKit::allocWKBridgeUpdateTextureInstance() initWithImageAsset:WebModel::convert(input.imageAsset) identifier:input.identifier.createNSString().get() hashString:input.hashString.createNSString().get()];

    if (!descriptor)
        return;

    RELEASE_ASSERT(m_receiver);
    [m_receiver updateTexture:descriptor];
#endif
}

void WebMesh::updateMaterial(const WebModel::UpdateMaterialDescriptor& originalDescriptor)
{
#if ENABLE(GPU_PROCESS_MODEL)
    WKBridgeUpdateMaterial *descriptor = [WebKit::allocWKBridgeUpdateMaterialInstance() initWithMaterialGraph:WebModel::convert(originalDescriptor.materialGraph) identifier:originalDescriptor.identifier.createNSString().get()];
    if (!descriptor)
        return;

    RELEASE_ASSERT(m_receiver);
    BinarySemaphore completion;
    [m_receiver updateMaterial:descriptor completionHandler:[&] mutable {
        completion.signal();
    }];
    completion.wait();
#else
    UNUSED_PARAM(originalDescriptor);
#endif
}

void WebMesh::setTransform(const simd_float4x4& transform)
{
#if ENABLE(GPU_PROCESS_MODEL)
    m_transform = transform;
    [m_receiver setTransform:transform];
#else
    UNUSED_PARAM(transform);
#endif
}

void WebMesh::setCameraDistance(float distance)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setCameraDistance:distance];
    render();
#else
    UNUSED_PARAM(distance);
#endif
}

void WebMesh::setBackgroundColor(const simd_float3& color)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setBackgroundColor:color];
#else
    UNUSED_PARAM(color);
#endif
}

void WebMesh::setEnvironmentMap(const WebModel::ImageAsset& imageAsset)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setEnvironmentMap:WebModel::convert(imageAsset)];
#else
    UNUSED_PARAM(imageAsset);
#endif
}

void WebMesh::play(bool play)
{
#if ENABLE(GPU_PROCESS_MODEL)
    [m_receiver setPlaying:play];
#else
    UNUSED_PARAM(play);
#endif
}

} // namespace WebKit
