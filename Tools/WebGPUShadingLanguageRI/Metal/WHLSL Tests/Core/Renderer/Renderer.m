/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "Renderer.h"

// This should mirror the structure that is used for the vertex shaders
typedef struct {
    vector_float2 position;
    vector_float2 uv;
} VertexStruct;

// A quad that fills the viewport
static VertexStruct vertexData[] =
{
    // 2D positions
    { { -1.0,  -1.0 }, { 0, 0 } },
    { {  1.0,  -1.0 }, { 1, 0 } },
    { { -1.0, 1.0 }, { 0, 1 } },
    { {  1.0, 1.0 }, { 1, 1 } }
};

@interface Renderer ()

@property id<MTLDevice> device;
@property NSUInteger targetWidth;
@property NSUInteger targetHeight;

@end

@implementation Renderer {
    id<MTLCommandQueue> _commandQueue;
    id<MTLLibrary> _library;

    MTLRenderPipelineDescriptor *_pipelineStateDescriptor;

    id<MTLRenderPipelineState> _pipelineState;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device
{
    self = [super init];
    if (self) {
        _device = device;
        _commandQueue = [_device newCommandQueue];
        self.transformMatrix = matrix_identity_float3x3;
    }
    return self;
}

- (void)setLibrary:(id<MTLLibrary>)library vertexShaderName:(NSString*)vertexShaderName fragmentShaderName:(NSString*)fragmentShaderName pixelFormat:(MTLPixelFormat)pixelFormat
{
    _library = library;
    id<MTLFunction> vertexFunction = [_library newFunctionWithName:vertexShaderName];
    id<MTLFunction> fragmentFunction = [_library newFunctionWithName:fragmentShaderName];

    // Configure a pipeline descriptor that is used to create a pipeline state
    _pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

    MTLVertexDescriptor *vertexDescriptor = [MTLVertexDescriptor vertexDescriptor];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    vertexDescriptor.attributes[0].offset = 0;

    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    vertexDescriptor.attributes[1].offset = offsetof(VertexStruct, uv);

    vertexDescriptor.layouts[0].stride = sizeof(VertexStruct);
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

    _pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;

    _pipelineStateDescriptor.label = @"Simple Pipeline";
    _pipelineStateDescriptor.vertexFunction = vertexFunction;
    _pipelineStateDescriptor.fragmentFunction = fragmentFunction;
    _pipelineStateDescriptor.colorAttachments[0].pixelFormat = pixelFormat;

    NSError *error = nil;
    _pipelineState = [_device newRenderPipelineStateWithDescriptor:_pipelineStateDescriptor error:&error];

    NSAssert(_pipelineState, @"Failed to created pipeline state: %@", error.localizedDescription);
}

- (void)setTargetWidth:(NSUInteger)width setTargetHeight:(NSUInteger)height
{
    _targetWidth = width;
    _targetHeight = height;
}

- (void)draw
{
    if (!_pipelineState)
        return;

    // Create a new command buffer for each render pass to the current drawable
    id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
    commandBuffer.label = @"MyCommand";

    MTLRenderPassDescriptor *renderPassDescriptor = [self renderPassDescriptor];

    NSAssert(renderPassDescriptor, @"nil MTLRenderPassDescriptor");

    renderPassDescriptor.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
    renderPassDescriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    renderPassDescriptor.colorAttachments[0].storeAction = MTLStoreActionStore;

    // Create a render command encoder so we can render into something
    id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    renderEncoder.label = @"MyRenderEncoder";

    [renderEncoder setViewport:(MTLViewport) { 0.0, 0.0, _targetWidth, _targetHeight, -1.0, 1.0 }];

    [renderEncoder setRenderPipelineState:_pipelineState];

    [renderEncoder setVertexBytes:vertexData length:sizeof(vertexData) atIndex:0];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];

    [renderEncoder endEncoding];

    [self configureCommandBufferBeforeCommit:commandBuffer];

    [commandBuffer commit];
    [commandBuffer waitUntilCompleted];
}

- (void)setTransformMatrix:(simd_float3x3)transformMatrix
{
    _transformMatrix = transformMatrix;
    simd_float3 v0 = matrix_multiply(_transformMatrix, vector3(0.0f, 0.0f, 1.0f));
    simd_float3 v1 = matrix_multiply(_transformMatrix, vector3(1.0f, 0.0f, 1.0f));
    simd_float3 v2 = matrix_multiply(_transformMatrix, vector3(0.0f, 1.0f, 1.0f));
    simd_float3 v3 = matrix_multiply(_transformMatrix, vector3(1.0f, 1.0f, 1.0f));

    vertexData[0].uv = vector2(v0.x, v0.y);
    vertexData[1].uv = vector2(v1.x, v1.y);
    vertexData[2].uv = vector2(v2.x, v2.y);
    vertexData[3].uv = vector2(v3.x, v3.y);
}

- (MTLRenderPassDescriptor *)renderPassDescriptor
{
    return nil;
}

- (void)configureCommandBufferBeforeCommit:(id<MTLCommandBuffer>)commandBuffer
{
}

@end
