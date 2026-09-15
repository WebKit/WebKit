//
//  MTLRenderCommandEncoder.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>
#import <Metal/MTLTypes.h>
#import <Metal/MTLCommandEncoder.h>
#import <Metal/MTLCommandBuffer.h>
#import <Metal/MTLRenderPass.h>
#import <Metal/MTLFence.h>
#import <Metal/MTLStageInputOutputDescriptor.h>


NS_ASSUME_NONNULL_BEGIN
@protocol MTLDevice;
@protocol MTLFunction;
@protocol MTLBuffer;
@protocol MTLSamplerState;

@protocol MTLDepthStencilState;
@protocol MTLTexture;
@protocol MTLRenderPipelineState;

@class MTLLogicalToPhysicalColorAttachmentMap;

typedef NS_ENUM(NSUInteger, MTLPrimitiveType) {
    MTLPrimitiveTypePoint = 0,
    MTLPrimitiveTypeLine = 1,
    MTLPrimitiveTypeLineStrip = 2,
    MTLPrimitiveTypeTriangle = 3,
    MTLPrimitiveTypeTriangleStrip = 4,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLVisibilityResultMode) {
    MTLVisibilityResultModeDisabled = 0,
    MTLVisibilityResultModeBoolean = 1,
    MTLVisibilityResultModeCounting API_AVAILABLE(macos(10.11), ios(9.0)) = 2,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef struct {
    NSUInteger x, y, width, height;
} MTLScissorRect;

typedef struct {
    double originX, originY, width, height, znear, zfar;
} MTLViewport;

typedef NS_ENUM(NSUInteger, MTLCullMode) {
    MTLCullModeNone = 0,
    MTLCullModeFront = 1,
    MTLCullModeBack = 2,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLWinding) {
    MTLWindingClockwise = 0,
    MTLWindingCounterClockwise = 1,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef NS_ENUM(NSUInteger, MTLDepthClipMode) {
    MTLDepthClipModeClip = 0,
    MTLDepthClipModeClamp = 1,
} API_AVAILABLE(macos(10.11), ios(9.0));

typedef NS_ENUM(NSUInteger, MTLTriangleFillMode) {
    MTLTriangleFillModeFill = 0,
    MTLTriangleFillModeLines = 1,
} API_AVAILABLE(macos(10.11), ios(8.0));

typedef struct {
    uint32_t vertexCount;
    uint32_t instanceCount;
    uint32_t vertexStart;
    uint32_t baseInstance;
} MTLDrawPrimitivesIndirectArguments;

typedef struct {
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t indexStart;
    int32_t  baseVertex;
    uint32_t baseInstance;
} MTLDrawIndexedPrimitivesIndirectArguments;

typedef struct {
    uint32_t viewportArrayIndexOffset;
    uint32_t renderTargetArrayIndexOffset;
} MTLVertexAmplificationViewMapping API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

typedef struct {
    uint32_t patchCount;
    uint32_t instanceCount;
    uint32_t patchStart;
    uint32_t baseInstance;
} MTLDrawPatchIndirectArguments;

typedef struct {
    /* NOTE: edgeTessellationFactor and insideTessellationFactor are interpreted as half (16-bit floats) */
    uint16_t edgeTessellationFactor[4];
    uint16_t insideTessellationFactor[2];
} MTLQuadTessellationFactorsHalf;

typedef struct {
    /* NOTE: edgeTessellationFactor and insideTessellationFactor are interpreted as half (16-bit floats) */
    uint16_t edgeTessellationFactor[3];
    uint16_t insideTessellationFactor;
} MTLTriangleTessellationFactorsHalf;

/*!
 @abstract Generic render stage enum
 @brief Can also be used for points at which a fence may be waited on or signaled.
 @constant MTLRenderStageVertex   All vertex work prior to rasterization has completed.
 @constant MTLRenderStageFragment All rendering work has completed.
 */
typedef NS_OPTIONS(NSUInteger, MTLRenderStages)
{
    MTLRenderStageVertex   = (1UL << 0),
    MTLRenderStageFragment = (1UL << 1),
    MTLRenderStageTile API_AVAILABLE(macos(12.0), ios(15.0)) = (1UL << 2),
    MTLRenderStageObject API_AVAILABLE(macos(13.0), ios(16.0))  = (1UL << 3),
    MTLRenderStageMesh API_AVAILABLE(macos(13.0), ios(16.0))  = (1UL << 4),
} API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @protocol MTLRenderCommandEncoder
 @discussion MTLRenderCommandEncoder is a container for graphics rendering state and the code to translate the state into a command format that the device can execute. 
 */
API_AVAILABLE(macos(10.11), ios(8.0))

@protocol MTLRenderCommandEncoder <MTLCommandEncoder>

/*!
 @method setRenderPipelineState
 @brief Sets the current render pipeline state object.
 */
- (void)setRenderPipelineState:(id <MTLRenderPipelineState>)pipelineState;

/* Vertex Resources */

/*!
 @method setVertexBytes:length:atIndex:
 @brief Set the data (by copy) for a given vertex buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setVertexBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setVertexBuffer:offset:atIndex:
 @brief Set a global buffer for all vertex shaders at the given bind point index.
 */
- (void)setVertexBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;

/*!
 @method setVertexBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all vertex shaders at the given bind point index.
 */
- (void)setVertexBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setVertexBuffers:offsets:withRange:
 @brief Set an array of global buffers for all vertex shaders with the given bind point range.
 */
- (void)setVertexBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range;

/*!
  @brief
    sets vertex buffer at specified index with provided offset and stride. Only
    call this when the buffer-index is part of the vertexDescriptor and has set
    its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setVertexBuffer:(nullable id<MTLBuffer>)buffer
                  offset:(NSUInteger)offset
         attributeStride:(NSUInteger)stride
                 atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    sets an array of vertex buffers with provided offsets and strides with the
    given bind point range.
    only call this when at least one buffer is part of the
    vertexDescriptor, other buffers must set their value relative to the
    `attributeStrides` array to `MTLAttributeStrideStatic`
*/
- (void) setVertexBuffers:(id<MTLBuffer> const __nullable [__nonnull])buffers
                  offsets:(NSUInteger const [__nonnull])offsets
         attributeStrides:(NSUInteger const [__nonnull])strides
                withRange:(NSRange)range
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    only call this when the buffer-index is part of the vertexDescriptor and
    has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setVertexBufferOffset:(NSUInteger)offset
               attributeStride:(NSUInteger)stride
                       atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));
/*!
  @brief
    only call this when the buffer-index is part of the vertexDescriptor and
    has set its stride to `MTLBufferLayoutStrideDynamic`
*/
- (void) setVertexBytes:(void const *)bytes
                 length:(NSUInteger)length
        attributeStride:(NSUInteger)stride
                atIndex:(NSUInteger)index
API_AVAILABLE(macos(14.0), ios(17.0));

/*!
 @method setVertexTexture:atIndex:
 @brief Set a global texture for all vertex shaders at the given bind point index.
 */
- (void)setVertexTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;

/*!
 @method setVertexTextures:withRange:
 @brief Set an array of global textures for all vertex shaders with the given bind point range.
 */
- (void)setVertexTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range;

/*!
 @method setVertexSamplerState:atIndex:
 @brief Set a global sampler for all vertex shaders at the given bind point index.
 */
- (void)setVertexSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;

/*!
 @method setVertexSamplerStates:withRange:
 @brief Set an array of global samplers for all vertex shaders with the given bind point range.
 */
- (void)setVertexSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range;

/*!
 @method setVertexSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all vertex shaders at the given bind point index.
 */
- (void)setVertexSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index;

/*!
 @method setVertexSamplerStates:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all vertex shaders with the given bind point range.
 */
- (void)setVertexSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range;

/*!
 @method setVertexVisibleFunctionTable:atBufferIndex:
 @brief Set a global visible function table for all vertex shaders at the given buffer bind point index.
 */
- (void)setVertexVisibleFunctionTable:(nullable id <MTLVisibleFunctionTable>)functionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setVertexVisibleFunctionTables:withBufferRange:
 @brief Set an array of global visible function tables for all vertex shaders with the given buffer bind point range.
 */
- (void)setVertexVisibleFunctionTables:(const id <MTLVisibleFunctionTable> __nullable [__nonnull])functionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setVertexIntersectionFunctionTable:atBufferIndex:
 @brief Set a global intersection function table for all vertex shaders at the given buffer bind point index.
 */
- (void)setVertexIntersectionFunctionTable:(nullable id <MTLIntersectionFunctionTable>)intersectionFunctionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setVertexIntersectionFunctionTables:withBufferRange:
 @brief Set an array of global intersection function tables for all vertex shaders with the given buffer bind point range.
 */
- (void)setVertexIntersectionFunctionTables:(const id <MTLIntersectionFunctionTable> __nullable [__nonnull])intersectionFunctionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setVertexAccelerationStructure:atBufferIndex:
 @brief Set a global acceleration structure for all vertex shaders at the given buffer bind point index.
 */
- (void)setVertexAccelerationStructure:(nullable id <MTLAccelerationStructure>)accelerationStructure atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setViewport:
 @brief Set the viewport, which is used to transform vertexes from normalized device coordinates to window coordinates.  Fragments that lie outside of the viewport are clipped, and optionally clamped for fragments outside of znear/zfar.
 */
- (void)setViewport:(MTLViewport)viewport;

/*!
 @method setViewports:
 @brief Specifies an array of viewports, which are used to transform vertices from normalized device coordinates to window coordinates based on [[ viewport_array_index ]] value specified in the vertex shader.
 */
- (void)setViewports:(const MTLViewport [__nonnull])viewports count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(12.0), tvos(14.5));

/*!
 @method setFrontFacingWinding:
 @brief The winding order of front-facing primitives.
 */
- (void)setFrontFacingWinding:(MTLWinding)frontFacingWinding;

/*!
 @method setVertexAmplificationCount:
 @brief Specifies the vertex amplification count and associated view mappings for each amplification ID.
 @param count the amplification count. The maximum value is currently 2.
 @param viewMappings an array of mapping elements.
 @discussion Each mapping element describes how to route the corresponding amplification ID to a specific viewport and render target array index by using offsets from the base array index provided by the [[render_target_array_index]] and/or [[viewport_array_index]] output attributes in the vertex shader. This allows a modicum of programmability for each amplified vertex to be routed to a different [[render_target_array_index]] and [[viewport_array_index]] even though these attribytes cannot be amplified themselves.
 */
- (void)setVertexAmplificationCount:(NSUInteger)count viewMappings:(nullable const MTLVertexAmplificationViewMapping *)viewMappings API_AVAILABLE(macos(10.15.4), ios(13.0), macCatalyst(13.4), tvos(16.0));

/*!
 @method setCullMode:
 @brief Controls if primitives are culled when front facing, back facing, or not culled at all.
 */
- (void)setCullMode:(MTLCullMode)cullMode;

/*!
@method setDepthClipMode:
@brief Controls what is done with fragments outside of the near or far planes.
*/
- (void)setDepthClipMode:(MTLDepthClipMode)depthClipMode API_AVAILABLE(macos(10.11), ios(11.0));

/*!
 @method setDepthBias:slopeScale:clamp:
 @brief Depth Bias.
 */
- (void)setDepthBias:(float)depthBias slopeScale:(float)slopeScale clamp:(float)clamp;

/// Configures the minimum and maximum bounds for depth bounds testing.
///
/// The render command encoder disables depth bounds testing by default.
/// The render command encoder also disables depth bounds testing when all of the following properties equal a specific value:
/// - The `minBound` property is equal to `0.0f`.
/// - The `maxBound` property is equal to `1.0f`.
/// Both `minBound` and `maxBound` need to be within `[0.0f, 1.0f]`, and `minBound` needs to be less than or equal to `maxBound`.
/// - Parameters:
///   - minBound: A minimum bound for depth testing, which discards fragments with a stored depth that is less than `minBound`.
///   - maxBound: A maximum bound for depth testing, which discards fragments with a stored depth that is greater than `maxBound`.
- (void)setDepthTestMinBound:(float)minBound maxBound:(float)maxBound API_AVAILABLE(macos(26.0), ios(26.0));

/*!
 @method setScissorRect:
 @brief Specifies a rectangle for a fragment scissor test.  All fragments outside of this rectangle are discarded.
 */
- (void)setScissorRect:(MTLScissorRect)rect;


/*!
 @method setScissorRects:
 @brief Specifies an array of rectangles for a fragment scissor test. The specific rectangle used is based on the [[ viewport_array_index ]] value output by the vertex shader. Fragments that lie outside the scissor rectangle are discarded.
 */
- (void)setScissorRects:(const MTLScissorRect [__nonnull])scissorRects count:(NSUInteger)count API_AVAILABLE(macos(10.13), ios(12.0), tvos(14.5));

/*!
 @method setTriangleFillMode:
 @brief Set how to rasterize triangle and triangle strip primitives.
 */
- (void)setTriangleFillMode:(MTLTriangleFillMode)fillMode;

/* Fragment Resources */

/*!
 @method setFragmentBytes:length:atIndex:
 @brief Set the data (by copy) for a given fragment buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setFragmentBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setFragmentBuffer:offset:atIndex:
 @brief Set a global buffer for all fragment shaders at the given bind point index.
 */
- (void)setFragmentBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index;

/*!
 @method setFragmentBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all fragment shaders at the given bind point index.
 */
- (void)setFragmentBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(10.11), ios(8.3));

/*!
 @method setFragmentBuffers:offsets:withRange:
 @brief Set an array of global buffers for all fragment shaders with the given bind point range.
 */
- (void)setFragmentBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range;

/*!
 @method setFragmentTexture:atIndex:
 @brief Set a global texture for all fragment shaders at the given bind point index.
 */
- (void)setFragmentTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index;

/*!
 @method setFragmentTextures:withRange:
 @brief Set an array of global textures for all fragment shaders with the given bind point range.
 */
- (void)setFragmentTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range;

/*!
 @method setFragmentSamplerState:atIndex:
 @brief Set a global sampler for all fragment shaders at the given bind point index.
 */
- (void)setFragmentSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index;

/*!
 @method setFragmentSamplerStates:withRange:
 @brief Set an array of global samplers for all fragment shaders with the given bind point range.
 */
- (void)setFragmentSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range;

/*!
 @method setFragmentSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all fragment shaders at the given bind point index.
 */
- (void)setFragmentSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index;

/*!
 @method setFragmentSamplerStates:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all fragment shaders with the given bind point range.
 */
- (void)setFragmentSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range;

/*!
 @method setFragmentVisibleFunctionTable:atBufferIndex:
 @brief Set a global visible function table for all fragment shaders at the given buffer bind point index.
 */
- (void)setFragmentVisibleFunctionTable:(nullable id <MTLVisibleFunctionTable>)functionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setFragmentVisibleFunctionTables:withBufferRange:
 @brief Set an array of global visible function tables for all fragment shaders with the given buffer bind point range.
 */
- (void)setFragmentVisibleFunctionTables:(const id <MTLVisibleFunctionTable> __nullable [__nonnull])functionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setFragmentIntersectionFunctionTable:atBufferIndex:
 @brief Set a global intersection function table for all fragment shaders at the given buffer bind point index.
 */
- (void)setFragmentIntersectionFunctionTable:(nullable id <MTLIntersectionFunctionTable>)intersectionFunctionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setFragmentIntersectionFunctionTables:withBufferRange:
 @brief Set an array of global intersection function tables for all fragment shaders with the given buffer bind point range.
 */
- (void)setFragmentIntersectionFunctionTables:(const id <MTLIntersectionFunctionTable> __nullable [__nonnull])intersectionFunctionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setFragmentAccelerationStructure:atBufferIndex:
 @brief Set a global acceleration structure for all fragment shaders at the given buffer bind point index.
 */
- (void)setFragmentAccelerationStructure:(nullable id <MTLAccelerationStructure>)accelerationStructure atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/* Constant Blend Color */
/*!
 @method setBlendColorRed:green:blue:alpha:
 @brief Set the constant blend color used across all blending on all render targets
 */
- (void)setBlendColorRed:(float)red green:(float)green blue:(float)blue alpha:(float)alpha;

/*! 
 @method setDepthStencilState:
 @brief Set the DepthStencil state object.
 */
- (void)setDepthStencilState:(nullable id <MTLDepthStencilState>)depthStencilState;

/*! 
 @method setStencilReferenceValue:
 @brief Set the stencil reference value for both the back and front stencil buffers.
 */
- (void)setStencilReferenceValue:(uint32_t)referenceValue;

/*!
 @method setStencilFrontReferenceValue:backReferenceValue:
 @brief Set the stencil reference value for the back and front stencil buffers independently.
 */
- (void)setStencilFrontReferenceValue:(uint32_t)frontReferenceValue backReferenceValue:(uint32_t)backReferenceValue API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method setVisibilityResultMode:offset:
 @abstract Monitor if samples pass the depth and stencil tests.
 @param mode Controls if the counter is disabled or moniters passing samples.
 @param offset The offset relative to the occlusion query buffer provided when the command encoder was created.  offset must be a multiple of 8.
 */
- (void)setVisibilityResultMode:(MTLVisibilityResultMode)mode offset:(NSUInteger)offset;

/*!
 @method setColorStoreAction:atIndex:
 @brief If the the store action for a given color attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setColorStoreAction:atIndex: must be used to finalize the store action before endEncoding is called.
 @param storeAction The desired store action for the given color attachment.  This may be set to any value other than MTLStoreActionUnknown.
 @param colorAttachmentIndex The index of the color attachment
*/
- (void)setColorStoreAction:(MTLStoreAction)storeAction atIndex:(NSUInteger)colorAttachmentIndex API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setDepthStoreAction:
 @brief If the the store action for the depth attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setDepthStoreAction: must be used to finalize the store action before endEncoding is called.
*/
- (void)setDepthStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setStencilStoreAction:
 @brief If the the store action for the stencil attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setStencilStoreAction: must be used to finalize the store action before endEncoding is called.
*/
- (void)setStencilStoreAction:(MTLStoreAction)storeAction API_AVAILABLE(macos(10.12), ios(10.0));

/*!
 @method setColorStoreActionOptions:atIndex:
 @brief If the the store action for a given color attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setColorStoreActionOptions:atIndex: may be used to finalize the store action options before endEncoding is called.
 @param storeActionOptions The desired store action options for the given color attachment.
 @param colorAttachmentIndex The index of the color attachment
 */
- (void)setColorStoreActionOptions:(MTLStoreActionOptions)storeActionOptions atIndex:(NSUInteger)colorAttachmentIndex API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/*!
 @method setDepthStoreActionOptions:
 @brief If the the store action for the depth attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setDepthStoreActionOptions: may be used to finalize the store action options before endEncoding is called.
 */
- (void)setDepthStoreActionOptions:(MTLStoreActionOptions)storeActionOptions API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/*!
 @method setStencilStoreActionOptions:
 @brief If the the store action for the stencil attachment was set to MTLStoreActionUnknown when the render command encoder was created,
 setStencilStoreActionOptions: may be used to finalize the store action options before endEncoding is called.
 */
- (void)setStencilStoreActionOptions:(MTLStoreActionOptions)storeActionOptions API_DEPRECATED("Store action options have no effect on Apple Silicon", macos(10.13, 27.0), ios(11.0, 27.0));

/* Object Resources */

/*!
 @method setObjectBytes:length:atIndex:
 @brief Set the data (by copy) for a given object shader buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setObjectBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectBuffer:offset:atIndex:
 @brief Set a global buffer for all object shaders at the given bind point index.
 */
- (void)setObjectBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all object shaders at the given bind point index.
 */
- (void)setObjectBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectBuffers:offsets:withRange:
 @brief Set an array of global buffers for all object shaders with the given bind point range.
 */
- (void)setObjectBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectTexture:atIndex:
 @brief Set a global texture for all object shaders at the given bind point index.
 */
- (void)setObjectTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectTextures:withRange:
 @brief Set an array of global textures for all object shaders with the given bind point range.
 */
- (void)setObjectTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectSamplerState:atIndex:
 @brief Set a global sampler for all object shaders at the given bind point index.
 */
- (void)setObjectSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectSamplerStates:withRange:
 @brief Set an array of global samplers for all object shaders with the given bind point range.
 */
- (void)setObjectSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all object shaders at the given bind point index.
 */
- (void)setObjectSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectSamplerStates:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all object shaders with the given bind point range.
 */
- (void)setObjectSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setObjectThreadgroupMemoryLength:atIndex:
 @brief Set the threadgroup memory byte length at the binding point specified by the index for all object shaders.
 */
- (void)setObjectThreadgroupMemoryLength:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/* Mesh Resources */

/*!
 @method setMeshBytes:length:atIndex:
 @brief Set the data (by copy) for a given mesh shader buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setMeshBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshBuffer:offset:atIndex:
 @brief Set a global buffer for all mesh shaders at the given bind point index.
 */
- (void)setMeshBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all mesh shaders at the given bind point index.
 */
- (void)setMeshBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshBuffers:offsets:withRange:
 @brief Set an array of global buffers for all mesh shaders with the given bind point range.
 */
- (void)setMeshBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshTexture:atIndex:
 @brief Set a global texture for all mesh shaders at the given bind point index.
 */
- (void)setMeshTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshTextures:withRange:
 @brief Set an array of global textures for all mesh shaders with the given bind point range.
 */
- (void)setMeshTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshSamplerState:atIndex:
 @brief Set a global sampler for all mesh shaders at the given bind point index.
 */
- (void)setMeshSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index
API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshSamplerStates:withRange:
 @brief Set an array of global samplers for all mesh shaders with the given bind point range.
 */
- (void)setMeshSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all mesh shaders at the given bind point index.
 */
- (void)setMeshSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method setMeshSamplerStates:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all mesh shaders with the given bind point range.
 */
- (void)setMeshSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range API_AVAILABLE(macos(13.0), ios(16.0));

/* Drawing computed geometry using object / mesh shaders */

/*!
 @method drawMeshThreadgroups:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:
 @abstract Enqueue a grid of object (if present) or mesh shader threadgroups.
 @discussion The dimensions of the threadgroups and the grid are specified directly.
 @param threadgroupsPerGrid The number of threadgroups in the object (if present) or mesh shader grid.
 @param threadsPerObjectThreadgroup The number of threads in one object shader threadgroup. Ignored if object shader is not present.
 @param threadsPerMeshThreadgroup The number of threads in one mesh shader threadgroup.
*/
- (void)drawMeshThreadgroups:(MTLSize)threadgroupsPerGrid
 threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
   threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method drawMeshThreads:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:
 @abstract Enqueue a grid of object (if present) of mesh shader threadgroups.
 @discussion The dimensions of the threadgroups and the grid are specified directly.
 The dimensions of threadsPerGrid does not have to be a multiple of threadsPerThreadgroup for object shaders.
 For mesh shaders, threadsPerGrid is rounded down to the neareset multiple of threadsPerMeshThreadgroup (in each dimension).
 @param threadsPerGrid The number of threads in the object (if present) or mesh shader grid
 @param threadsPerObjectThreadgroup The number of threads in one object shader threadgroup. Ignored if object shader is not present.
 @param threadsPerMeshThreadgroup The number of threads in one mesh shader threadgroup.
*/
- (void)     drawMeshThreads:(MTLSize)threadsPerGrid
 threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
   threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
API_AVAILABLE(macos(13.0), ios(16.0));

/*!
 @method drawMeshThreadgroupsWithIndirectBuffer:indirectBufferOffset:threadsPerObjectThreadgroup:threadsPerMeshThreadgroup:
 @abstract Enqueue a grid of object (if present) or mesh shader threadgroups.
 @discussion The dimensions of the threadgroups are specified directly, the dimensions of the grid, in threadgroups, are read from a buffer by the GPU.
 @param indirectBuffer A buffer object that the device will read the grid size from, see MTLDispatchThreadgroupsIndirectArguments.
 @param indirectBufferOffset Byte offset within @a indirectBuffer to read arguments from.  @a indirectBufferOffset must be a multiple of 4.
 @param threadsPerObjectThreadgroup The number of threads in one object shader threadgroup. Ignored if object shader is not present.
 @param threadsPerMeshThreadgroup The number of threads in one mesh shader threadgroup.
*/
- (void)drawMeshThreadgroupsWithIndirectBuffer:(id<MTLBuffer>)indirectBuffer
                          indirectBufferOffset:(NSUInteger)indirectBufferOffset
                   threadsPerObjectThreadgroup:(MTLSize)threadsPerObjectThreadgroup
                     threadsPerMeshThreadgroup:(MTLSize)threadsPerMeshThreadgroup
API_AVAILABLE(macos(13.0), ios(16.0));

/* Drawing */

/*!
 @method drawPrimitives:vertexStart:vertexCount:instanceCount:
 @brief Draw primitives without an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param vertexStart For each instance, the first index to draw
 @param vertexCount For each instance, the number of indexes to draw
 @param instanceCount The number of instances drawn.
 */
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount;

/*!
 @method drawPrimitives:vertexStart:vertexCount:
 @brief Draw primitives without an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param vertexStart For each instance, the first index to draw
 @param vertexCount For each instance, the number of indexes to draw
 */
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount;

/*!
 @method drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:
 @brief Draw primitives with an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param indexCount The number of indexes to read from the index buffer for each instance.
 @param indexType The type if indexes, either 16 bit integer or 32 bit integer.
 @param indexBuffer A buffer object that the device will read indexes from.
 @param indexBufferOffset Byte offset within @a indexBuffer to start reading indexes from.  @a indexBufferOffset must be a multiple of the index size.
 @param instanceCount The number of instances drawn.
 */
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount;

/*!
 @method drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:
 @brief Draw primitives with an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param indexCount The number of indexes to read from the index buffer for each instance.
 @param indexType The type if indexes, either 16 bit integer or 32 bit integer.
 @param indexBuffer A buffer object that the device will read indexes from.
 @param indexBufferOffset Byte offset within @a indexBuffer to start reading indexes from.  @a indexBufferOffset must be a multiple of the index size.
 */
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset;

/*!
 @method drawPrimitives:vertexStart:vertexCount:instanceCount:baseInstance:
 @brief Draw primitives without an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param vertexStart For each instance, the first index to draw
 @param vertexCount For each instance, the number of indexes to draw
 @param instanceCount The number of instances drawn.
 @param baseInstance Offset for instance_id.
 */
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType vertexStart:(NSUInteger)vertexStart vertexCount:(NSUInteger)vertexCount instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method drawIndexedPrimitives:indexCount:indexType:indexBuffer:indexBufferOffset:instanceCount:baseVertex:baseInstance:
 @brief Draw primitives with an index list.
 @param primitiveType The type of primitives that elements are assembled into.
 @param indexCount The number of indexes to read from the index buffer for each instance.
 @param indexType The type if indexes, either 16 bit integer or 32 bit integer.
 @param indexBuffer A buffer object that the device will read indexes from.
 @param indexBufferOffset Byte offset within @a indexBuffer to start reading indexes from.  @a indexBufferOffset must be a multiple of the index size.
 @param instanceCount The number of instances drawn.
 @param baseVertex Offset for vertex_id. NOTE: this can be negative
 @param baseInstance Offset for instance_id.
 */
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexCount:(NSUInteger)indexCount indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset instanceCount:(NSUInteger)instanceCount baseVertex:(NSInteger)baseVertex baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method drawPrimitives:indirectBuffer:indirectBufferOffset:
 @brief Draw primitives without an index list using an indirect buffer see MTLDrawPrimitivesIndirectArguments.
 @param primitiveType The type of primitives that elements are assembled into.
 @param indirectBuffer A buffer object that the device will read drawPrimitives arguments from, see MTLDrawPrimitivesIndirectArguments.
 @param indirectBufferOffset Byte offset within @a indirectBuffer to start reading indexes from.  @a indirectBufferOffset must be a multiple of 4.
 */
- (void)drawPrimitives:(MTLPrimitiveType)primitiveType indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method drawIndexedPrimitives:indexType:indexBuffer:indexBufferOffset:indirectBuffer:indirectBufferOffset:
 @brief Draw primitives with an index list using an indirect buffer see MTLDrawIndexedPrimitivesIndirectArguments.
 @param primitiveType The type of primitives that elements are assembled into.
 @param indexType The type if indexes, either 16 bit integer or 32 bit integer.
 @param indexBuffer A buffer object that the device will read indexes from.
 @param indexBufferOffset Byte offset within @a indexBuffer to start reading indexes from.  @a indexBufferOffset must be a multiple of the index size.
 @param indirectBuffer A buffer object that the device will read drawIndexedPrimitives arguments from, see MTLDrawIndexedPrimitivesIndirectArguments.
 @param indirectBufferOffset Byte offset within @a indirectBuffer to start reading indexes from.  @a indirectBufferOffset must be a multiple of 4.
 */
- (void)drawIndexedPrimitives:(MTLPrimitiveType)primitiveType indexType:(MTLIndexType)indexType indexBuffer:(id <MTLBuffer>)indexBuffer indexBufferOffset:(NSUInteger)indexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.11), ios(9.0));

/*!
 @method textureBarrier
 @brief Ensure that following fragment shaders can read textures written by previous draw calls (in particular the framebuffer)
 */
- (void)textureBarrier API_DEPRECATED_WITH_REPLACEMENT("memoryBarrierWithScope:MTLBarrierScopeRenderTargets", macos(10.11, 10.14)) API_UNAVAILABLE(ios);

/*!
 @method updateFence:afterStages:
 @abstract Update the fence to capture all GPU work so far enqueued by this encoder for the given stages.
 @discussion Unlike <st>updateFence:</st>, this method will update the fence when the given stage(s) complete, allowing for commands to overlap in execution.
 On iOS, render command encoder fence updates are always delayed until the end of the encoder.
 */
- (void)updateFence:(id <MTLFence>)fence afterStages:(MTLRenderStages)stages API_AVAILABLE(macos(10.13), ios(10.0));

/*!
 @method waitForFence:beforeStages:
 @abstract Prevent further GPU work until the fence is reached for the given stages.
 @discussion Unlike <st>waitForFence:</st>, this method will only block commands assoicated with the given stage(s), allowing for commands to overlap in execution.
 On iOS, render command encoder fence waits always occur the beginning of the encoder.
 */
- (void)waitForFence:(id <MTLFence>)fence beforeStages:(MTLRenderStages)stages API_AVAILABLE(macos(10.13), ios(10.0));

-(void)setTessellationFactorBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset instanceStride:(NSUInteger)instanceStride API_AVAILABLE(macos(10.12), ios(10.0));

-(void)setTessellationFactorScale:(float)scale API_AVAILABLE(macos(10.12), ios(10.0));

-(void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.12), ios(10.0));

-(void)drawPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.12), ios(12.0), tvos(14.5));

-(void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchStart:(NSUInteger)patchStart patchCount:(NSUInteger)patchCount patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset instanceCount:(NSUInteger)instanceCount baseInstance:(NSUInteger)baseInstance API_AVAILABLE(macos(10.12), ios(10.0));

-(void)drawIndexedPatches:(NSUInteger)numberOfPatchControlPoints patchIndexBuffer:(nullable id <MTLBuffer>)patchIndexBuffer patchIndexBufferOffset:(NSUInteger)patchIndexBufferOffset controlPointIndexBuffer:(id <MTLBuffer>)controlPointIndexBuffer controlPointIndexBufferOffset:(NSUInteger)controlPointIndexBufferOffset indirectBuffer:(id <MTLBuffer>)indirectBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.12), ios(12.0), tvos(14.5));

/*!
 @property tileWidth:
 @abstract The width of the tile for this render pass.
 */
@property (readonly) NSUInteger tileWidth API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));


/*!
 @property tileHeight:
 @abstract The height of the tile for this render pass.
 */
@property (readonly) NSUInteger tileHeight API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/* Tile Resources */

/*!
 @method setTileBytes:length:atIndex:
 @brief Set the data (by copy) for a given tile buffer binding point.  This will remove any existing MTLBuffer from the binding point.
 */
- (void)setTileBytes:(const void *)bytes length:(NSUInteger)length atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileBuffer:offset:atIndex:
 @brief Set a global buffer for all tile shaders at the given bind point index.
 */
- (void)setTileBuffer:(nullable id <MTLBuffer>)buffer offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileBufferOffset:atIndex:
 @brief Set the offset within the current global buffer for all tile shaders at the given bind point index.
 */
- (void)setTileBufferOffset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileBuffers:offsets:withRange:
 @brief Set an array of global buffers for all tile shaders with the given bind point range.
 */
- (void)setTileBuffers:(const id <MTLBuffer> __nullable [__nonnull])buffers offsets:(const NSUInteger [__nonnull])offsets withRange:(NSRange)range API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileTexture:atIndex:
 @brief Set a global texture for all tile shaders at the given bind point index.
 */
- (void)setTileTexture:(nullable id <MTLTexture>)texture atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileTextures:withRange:
 @brief Set an array of global textures for all tile shaders with the given bind point range.
 */
- (void)setTileTextures:(const id <MTLTexture> __nullable [__nonnull])textures withRange:(NSRange)range API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileSamplerState:atIndex:
 @brief Set a global sampler for all tile shaders at the given bind point index.
 */
- (void)setTileSamplerState:(nullable id <MTLSamplerState>)sampler atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileSamplerStates:withRange:
 @brief Set an array of global samplers for all fragment shaders with the given bind point range.
 */
- (void)setTileSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers withRange:(NSRange)range API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setTileSamplerState:lodMinClamp:lodMaxClamp:atIndex:
 @brief Set a global sampler for all tile shaders at the given bind point index.
 */
- (void)setTileSamplerState:(nullable id <MTLSamplerState>)sampler lodMinClamp:(float)lodMinClamp lodMaxClamp:(float)lodMaxClamp atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));


/*!
 @method setTileSamplerStates:lodMinClamps:lodMaxClamps:withRange:
 @brief Set an array of global samplers for all tile shaders with the given bind point range.
 */
- (void)setTileSamplerStates:(const id <MTLSamplerState> __nullable [__nonnull])samplers lodMinClamps:(const float [__nonnull])lodMinClamps lodMaxClamps:(const float [__nonnull])lodMaxClamps withRange:(NSRange)range API_AVAILABLE(ios(11.0), tvos(14.5), macos(11.0), macCatalyst(14.0));

/*!
 @method setTileVisibleFunctionTable:atBufferIndex:
 @brief Set a global visible function table for all tile shaders at the given buffer bind point index.
 */
- (void)setTileVisibleFunctionTable:(nullable id <MTLVisibleFunctionTable>)functionTable atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setTileVisibleFunctionTables:withBufferRange:
 @brief Set an array of global visible function tables for all tile shaders with the given buffer bind point range.
 */
- (void)setTileVisibleFunctionTables:(const id <MTLVisibleFunctionTable> __nullable [__nonnull])functionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setTileIntersectionFunctionTable:atBufferIndex:
 @brief Set a global intersection function table for all tile shaders at the given buffer bind point index.
 */
- (void)setTileIntersectionFunctionTable:(nullable id <MTLIntersectionFunctionTable>)intersectionFunctionTable atBufferIndex:(NSUInteger)bufferIndex
    API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setTileIntersectionFunctionTables:withBufferRange:
 @brief Set an array of global intersection function tables for all tile shaders with the given buffer bind point range.
 */
- (void)setTileIntersectionFunctionTables:(const id <MTLIntersectionFunctionTable> __nullable [__nonnull])intersectionFunctionTables withBufferRange:(NSRange)range API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method setTileAccelerationStructure:atBufferIndex:
 @brief Set a global acceleration structure for all tile shaders at the given buffer bind point index.
 */
- (void)setTileAccelerationStructure:(nullable id <MTLAccelerationStructure>)accelerationStructure atBufferIndex:(NSUInteger)bufferIndex API_AVAILABLE(macos(12.0), ios(15.0), tvos(16.0));

/*!
 @method dispatchThreadsPerTile:
 @brief dispatch threads to perform a mid-render compute operation.
 */
- (void)dispatchThreadsPerTile:(MTLSize)threadsPerTile API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));

/*!
 @method setThreadgroupMemoryLength:offset:atIndex:
 @brief Set the size of the threadgroup memory argument at the given bind point index and offset.
 */
- (void)setThreadgroupMemoryLength:(NSUInteger)length offset:(NSUInteger)offset atIndex:(NSUInteger)index API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(11.0), tvos(14.5));


/*!
 * @method useResource:usage:
 * @abstract Declare that a resource may be accessed by the render pass through an argument buffer
 * @discussion This method does not protect against data hazards; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resource through an argument buffer. However, this method may cause color attachments to become decompressed. Therefore, this method should be called until as late as possible within a render command encoder. Declaring a minimal usage (i.e. read-only) may prevent color attachments from becoming decompressed on some devices.
 
     Note that calling useResource does not retain the resource. It is the responsiblity of the user to retain the resource until
     the command buffer has been executed.
 */
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage API_DEPRECATED_WITH_REPLACEMENT("Use useResource:usage:stages: instead", macos(10.13, 13.0), ios(11.0, 16.0));

/*!
 * @method useResources:count:usage:
 * @abstract Declare that an array of resources may be accessed through an argument buffer by the render pass
 * @discussion This method does not protect against data hazards; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resources through an argument buffer. However, this method may cause color attachments to become decompressed. Therefore, this method should be called until as late as possible within a render command encoder. Declaring a minimal usage (i.e. read-only) may prevent color attachments from becoming decompressed on some devices.
 
   Note that calling useResources does not retain the resources. It is the responsiblity of the user to retain the resources until
   the command buffer has been executed.
*/
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage API_DEPRECATED_WITH_REPLACEMENT("Use useResources:count:usage:stages: instead", macos(10.13, 13.0), ios(11.0, 16.0));

/*!
 * @method useResources:usage:stage
 * @abstract Declare that a resource may be accessed by the render pass through an argument buffer
 * @discussion For hazard tracked resources, this method protects against data hazards. This method must be called before encoding any draw commands which may access the resource through an argument buffer. However, this method may cause color attachments to become decompressed. Therefore, this method should be called until as late as possible within a render command encoder. Declaring a minimal usage (i.e. read-only) may prevent color attachments from becoming decompressed on some devices.
 
    Note that calling useResource does not retain the resource. It is the responsiblity of the user to retain the resource until
    the command buffer has been executed.
*/
- (void)useResource:(id <MTLResource>)resource usage:(MTLResourceUsage)usage stages:(MTLRenderStages) stages API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 * @method useResources:count:usage:stages
 * @abstract Declare that an array of resources may be accessed through an argument buffer by the render pass
 * @discussion For hazard tracked resources, this method protects against data hazards.  This method must be called before encoding any draw commands which may access the resources through an argument buffer. However, this method may cause color attachments to become decompressed. Therefore, this method should be called until as late as possible within a render command encoder. Declaring a minimal usage (i.e. read-only) may prevent color attachments from becoming decompressed on some devices.
 
   Note that calling useResources does not retain the resources. It is the responsiblity of the user to retain the resources until
   the command buffer has been executed.
*/
- (void)useResources:(const id <MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count usage:(MTLResourceUsage)usage stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 * @method useHeap:
 * @abstract Declare that the resources allocated from a heap may be accessed by the render pass through an argument buffer
 * @discussion This method does not protect against data hazards; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resources allocated from the heap through an argument buffer. This method may cause all of the color attachments allocated from the heap to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 */
- (void)useHeap:(id <MTLHeap>)heap API_DEPRECATED_WITH_REPLACEMENT("Use useHeap:stages: instead", macos(10.13, 13.0), ios(11.0, 16.0));

/*!
 * @method useHeaps:count:
 * @abstract Declare that the resources allocated from an array of heaps may be accessed by the render pass through an argument buffer
 * @discussion This method does not protect against data hazards; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resources allocated from the heaps through an argument buffer. This method may cause all of the color attachments allocated from the heaps to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 */
- (void)useHeaps:(const id <MTLHeap> __nonnull[__nonnull])heaps count:(NSUInteger)count API_DEPRECATED_WITH_REPLACEMENT("Use useHeaps:count:stages: instead", macos(10.13, 13.0), ios(11.0, 16.0));

/*!
 * @method useHeap:stages:
 * @abstract Declare that the resources allocated from a heap may be accessed by the render pass through an argument buffer
 * @discussion If the heap is tracked, this method protects against hazard tracking; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resources allocated from the heap through an argument buffer. This method may cause all of the color attachments allocated from the heap to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 */
- (void)useHeap:(id <MTLHeap>)heap stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0));

/*!
 * @method useHeaps:count:stages:
 * @abstract Declare that the resources allocated from an array of heaps may be accessed by the render pass through an argument buffer
 * @discussion This method does not protect against data hazards; these hazards must be addressed using an MTLFence. This method must be called before encoding any draw commands which may access the resources allocated from the heaps through an argument buffer. This method may cause all of the color attachments allocated from the heaps to become decompressed. Therefore, it is recommended that the useResource:usage: or useResources:count:usage: methods be used for color attachments instead, with a minimal (i.e. read-only) usage.
 */
- (void)useHeaps:(const id <MTLHeap> __nonnull[__nonnull])heaps count:(NSUInteger)count stages:(MTLRenderStages)stages API_AVAILABLE(macos(10.15), ios(13.0));


/*!
 * @method executeCommandsInBuffer:withRange:
 * @abstract Execute commands in the buffer within the range specified.
 * @discussion The same indirect command buffer may be executed any number of times within the same encoder.
 */
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandBuffer withRange:(NSRange)executionRange API_AVAILABLE(macos(10.14), ios(12.0));

/*!
 * @method executeCommandsInBuffer:indirectBuffer:indirectBufferOffset:
 * @abstract Execute commands in the buffer within the range specified by the indirect range buffer.
 * @param indirectRangeBuffer An indirect buffer from which the device reads the execution range parameter, as laid out in the MTLIndirectCommandBufferExecutionRange structure.
 * @param indirectBufferOffset The byte offset within indirectBuffer where the execution range parameter is located. Must be a multiple of 4 bytes.
 * @discussion The same indirect command buffer may be executed any number of times within the same encoder.
 */
- (void)executeCommandsInBuffer:(id<MTLIndirectCommandBuffer>)indirectCommandbuffer indirectBuffer:(id<MTLBuffer>)indirectRangeBuffer indirectBufferOffset:(NSUInteger)indirectBufferOffset API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(13.0));


/*!
 * @method memoryBarrierWithScope:afterStages:beforeStages:
 * @abstract Make stores to memory encoded before the barrier coherent with loads from memory encoded after the barrier.
 * @discussion The barrier makes stores coherent that 1) are to a resource with a type in the given scope, and 2) happen at (or before) the stage given by afterStages. Only affects loads that happen at (or after) the stage given by beforeStages.
 */
-(void)memoryBarrierWithScope:(MTLBarrierScope)scope afterStages:(MTLRenderStages)after beforeStages:(MTLRenderStages)before API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(16.0));

/*!
 * @method memoryBarrierWithResources:count:afterStages:beforeStages:
 * @abstract Make stores to memory encoded before the barrier coherent with loads from memory encoded after the barrier.
 * @discussion The barrier makes stores coherent that 1) are to resources in given array, and 2) happen at (or before) the stage given by afterStages. Only affects loads that happen at (or after) the stage give by beforeStages.
 */
-(void)memoryBarrierWithResources:(const id<MTLResource> __nonnull[__nonnull])resources count:(NSUInteger)count afterStages:(MTLRenderStages)after beforeStages:(MTLRenderStages)before API_AVAILABLE(macos(10.14), macCatalyst(13.0), ios(16.0));



/*!
 @method sampleCountersInBuffer:atSampleIndex:withBarrier:
 @abstract Sample hardware counters at this point in the render encoder and
 store the counter sample into the sample buffer at the specified index.
 @param sampleBuffer The sample buffer to sample into
 @param sampleIndex The index into the counter buffer to write the sample.
 @param barrier Insert a barrier before taking the sample.  Passing
 YES will ensure that all work encoded before this operation in the encoder is
 complete but does not isolate the work with respect to other encoders.  Passing
 NO will allow the sample to be taken concurrently with other operations in this
 encoder.
 In general, passing YES will lead to more repeatable counter results but
 may negatively impact performance.  Passing NO will generally be higher performance
 but counter results may not be repeatable.
 */
-(void)sampleCountersInBuffer:(id<MTLCounterSampleBuffer>)sampleBuffer
                atSampleIndex:(NSUInteger)sampleIndex
                  withBarrier:(BOOL)barrier API_AVAILABLE(macos(10.15), ios(14.0));


/// Sets the mapping from logical shader color output to physical render pass color attachments.
///
/// Use this method to define how the physical color attachments you specify via ``MTLRenderPassDescriptor/colorAttachments``
/// map to the logical color output the fragment shader writes to.
///
/// To use this feature, make sure to set ``MTLRenderPassDescriptor/supportColorAttachmentMapping`` to
/// <doc://com.apple.documentation/documentation/swift/true>.
///
/// - Parameter mapping: Mapping from logical shader outputs to physical outputs.
- (void)setColorAttachmentMap:(nullable MTLLogicalToPhysicalColorAttachmentMap *)mapping
API_AVAILABLE(macos(26.0), ios(26.0));



@end
NS_ASSUME_NONNULL_END
