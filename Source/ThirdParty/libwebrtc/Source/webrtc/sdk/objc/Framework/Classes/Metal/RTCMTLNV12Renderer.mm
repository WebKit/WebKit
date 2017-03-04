/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLNV12Renderer.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#include "webrtc/api/video/video_rotation.h"

#define MTL_STRINGIFY(s) @ #s

// As defined in shaderSource.
static NSString *const vertexFunctionName = @"vertexPassthrough";
static NSString *const fragmentFunctionName = @"fragmentColorConversion";

static NSString *const pipelineDescriptorLabel = @"RTCPipeline";
static NSString *const commandBufferLabel = @"RTCCommandBuffer";
static NSString *const renderEncoderLabel = @"RTCEncoder";
static NSString *const renderEncoderDebugGroup = @"RTCDrawFrame";

static const float cubeVertexData[64] = {
    -1.0, -1.0, 0.0, 1.0, 1.0, -1.0, 1.0, 1.0, -1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0, 0.0,

    // rotation = 90, offset = 16.
    -1.0, -1.0, 1.0, 1.0, 1.0, -1.0, 1.0, 0.0, -1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 0.0, 0.0,

    // rotation = 180, offset = 32.
    -1.0, -1.0, 1.0, 0.0, 1.0, -1.0, 0.0, 0.0, -1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 1.0,

    // rotation = 270, offset = 48.
    -1.0, -1.0, 0.0, 0.0, 1.0, -1.0, 0.0, 1.0, -1.0, 1.0, 1.0, 0.0, 1.0, 1.0, 1.0, 1.0,
};

static inline int offsetForRotation(webrtc::VideoRotation rotation) {
  switch (rotation) {
    case webrtc::kVideoRotation_0:
      return 0;
    case webrtc::kVideoRotation_90:
      return 16;
    case webrtc::kVideoRotation_180:
      return 32;
    case webrtc::kVideoRotation_270:
      return 48;
  }
  return 0;
}

static NSString *const shaderSource = MTL_STRINGIFY(
    using namespace metal; typedef struct {
      packed_float2 position;
      packed_float2 texcoord;
    } Vertex;

    typedef struct {
      float4 position[[position]];
      float2 texcoord;
    } Varyings;

    vertex Varyings vertexPassthrough(device Vertex * verticies[[buffer(0)]],
                                      unsigned int vid[[vertex_id]]) {
      Varyings out;
      device Vertex &v = verticies[vid];
      out.position = float4(float2(v.position), 0.0, 1.0);
      out.texcoord = v.texcoord;

      return out;
    }

    // Receiving YCrCb textures.
    fragment half4 fragmentColorConversion(
        Varyings in[[stage_in]], texture2d<float, access::sample> textureY[[texture(0)]],
        texture2d<float, access::sample> textureCbCr[[texture(1)]]) {
      constexpr sampler s(address::clamp_to_edge, filter::linear);
      float y;
      float2 uv;
      y = textureY.sample(s, in.texcoord).r;
      uv = textureCbCr.sample(s, in.texcoord).rg - float2(0.5, 0.5);

      // Conversion for YUV to rgb from http://www.fourcc.org/fccyvrgb.php
      float4 out = float4(y + 1.403 * uv.y, y - 0.344 * uv.x - 0.714 * uv.y, y + 1.770 * uv.x, 1.0);

      return half4(out);
    });

// The max number of command buffers in flight (submitted to GPU).
// For now setting it up to 1.
// In future we might use triple buffering method if it improves performance.
static const NSInteger kMaxInflightBuffers = 1;

@implementation RTCMTLNV12Renderer {
  __kindof MTKView *_view;

  // Controller.
  dispatch_semaphore_t _inflight_semaphore;

  // Renderer.
  id<MTLDevice> _device;
  id<MTLCommandQueue> _commandQueue;
  id<MTLLibrary> _defaultLibrary;
  id<MTLRenderPipelineState> _pipelineState;

  // Textures.
  CVMetalTextureCacheRef _textureCache;
  id<MTLTexture> _yTexture;
  id<MTLTexture> _CrCbTexture;

  // Buffers.
  id<MTLBuffer> _vertexBuffer;

  // RTC Frame parameters.
  int _offset;
}

- (instancetype)init {
  if (self = [super init]) {
    // _offset of 0 is equal to rotation of 0.
    _offset = 0;
    _inflight_semaphore = dispatch_semaphore_create(kMaxInflightBuffers);
  }

  return self;
}

- (BOOL)addRenderingDestination:(__kindof MTKView *)view {
  return [self setupWithView:view];
}

#pragma mark - Private

- (BOOL)setupWithView:(__kindof MTKView *)view {
  BOOL success = NO;
  if ([self setupMetal]) {
    [self setupView:view];
    [self loadAssets];
    [self setupBuffers];
    [self initializeTextureCache];
    success = YES;
  }
  return success;
}

#pragma mark - GPU methods

- (BOOL)setupMetal {
  // Set the view to use the default device.
  _device = MTLCreateSystemDefaultDevice();
  if (!_device) {
    return NO;
  }

  // Create a new command queue.
  _commandQueue = [_device newCommandQueue];

  // Load metal library from source.
  NSError *libraryError = nil;

  id<MTLLibrary> sourceLibrary =
      [_device newLibraryWithSource:shaderSource options:NULL error:&libraryError];

  if (libraryError) {
    RTCLogError(@"Metal: Library with source failed\n%@", libraryError);
    return NO;
  }

  if (!sourceLibrary) {
    RTCLogError(@"Metal: Failed to load library. %@", libraryError);
    return NO;
  }
  _defaultLibrary = sourceLibrary;

  return YES;
}

- (void)setupView:(__kindof MTKView *)view {
  view.device = _device;

  view.preferredFramesPerSecond = 30;
  view.autoResizeDrawable = NO;

  // We need to keep reference to the view as it's needed down the rendering pipeline.
  _view = view;
}

- (void)loadAssets {
  id<MTLFunction> vertexFunction = [_defaultLibrary newFunctionWithName:vertexFunctionName];
  id<MTLFunction> fragmentFunction =
      [_defaultLibrary newFunctionWithName:fragmentFunctionName];

  MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
  pipelineDescriptor.label = pipelineDescriptorLabel;
  pipelineDescriptor.vertexFunction = vertexFunction;
  pipelineDescriptor.fragmentFunction = fragmentFunction;
  pipelineDescriptor.colorAttachments[0].pixelFormat = _view.colorPixelFormat;
  pipelineDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
  NSError *error = nil;
  _pipelineState = [_device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];

  if (!_pipelineState) {
    RTCLogError(@"Metal: Failed to create pipeline state. %@", error);
  }
}

- (void)setupBuffers {
  _vertexBuffer = [_device newBufferWithBytes:cubeVertexData
                                       length:sizeof(cubeVertexData)
                                      options:MTLResourceOptionCPUCacheModeDefault];
}

- (void)initializeTextureCache {
  CVReturn status =
      CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, _device, nil, &_textureCache);
  if (status != kCVReturnSuccess) {
    RTCLogError(@"Metal: Failed to initialize metal texture cache. Return status is %d", status);
  }
}

- (void)render {
  // Wait until the inflight (curently sent to GPU) command buffer
  // has completed the GPU work.
  dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);

  id<MTLCommandBuffer> commandBuffer = [_commandQueue commandBuffer];
  commandBuffer.label = commandBufferLabel;

  __block dispatch_semaphore_t block_semaphore = _inflight_semaphore;
  [commandBuffer addCompletedHandler:^(id<MTLCommandBuffer> _Nonnull) {
    // GPU work completed.
    dispatch_semaphore_signal(block_semaphore);
  }];

  MTLRenderPassDescriptor *_renderPassDescriptor = _view.currentRenderPassDescriptor;
  if (_renderPassDescriptor) {  // Valid drawable.
    id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:_renderPassDescriptor];
    renderEncoder.label = renderEncoderLabel;

    // Set context state.
    [renderEncoder pushDebugGroup:renderEncoderDebugGroup];
    [renderEncoder setRenderPipelineState:_pipelineState];
    [renderEncoder setVertexBuffer:_vertexBuffer offset:_offset * sizeof(float) atIndex:0];
    [renderEncoder setFragmentTexture:_yTexture atIndex:0];
    [renderEncoder setFragmentTexture:_CrCbTexture atIndex:1];

    [renderEncoder drawPrimitives:MTLPrimitiveTypeTriangleStrip
                      vertexStart:0
                      vertexCount:4
                    instanceCount:1];
    [renderEncoder popDebugGroup];
    [renderEncoder endEncoding];

    [commandBuffer presentDrawable:_view.currentDrawable];
  }

  // CPU work is completed, GPU work can be started.
  [commandBuffer commit];
}

#pragma mark - RTCMTLRenderer

- (void)drawFrame:(RTCVideoFrame *)frame {
  [self setupTexturesForFrame:frame];
  @autoreleasepool {
    [self render];
  }
}

- (void)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  CVPixelBufferRef pixelBuffer = frame.nativeHandle;

  id<MTLTexture> lumaTexture = nil;
  id<MTLTexture> chromaTexture = nil;
  CVMetalTextureRef outTexture = nullptr;

  // Luma (y) texture.
  int lumaWidth = CVPixelBufferGetWidthOfPlane(pixelBuffer, 0);
  int lumaHeight = CVPixelBufferGetHeightOfPlane(pixelBuffer, 0);

  int indexPlane = 0;
  CVReturn result = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _textureCache, pixelBuffer, nil, MTLPixelFormatR8Unorm, lumaWidth,
      lumaHeight, indexPlane, &outTexture);

  if (result == kCVReturnSuccess) {
    lumaTexture = CVMetalTextureGetTexture(outTexture);
  }

  // Same as CFRelease except it can be passed NULL without crashing.
  CVBufferRelease(outTexture);
  outTexture = nullptr;

  // Chroma (CrCb) texture.
  indexPlane = 1;
  result = CVMetalTextureCacheCreateTextureFromImage(
      kCFAllocatorDefault, _textureCache, pixelBuffer, nil, MTLPixelFormatRG8Unorm, lumaWidth / 2,
      lumaHeight / 2, indexPlane, &outTexture);
  if (result == kCVReturnSuccess) {
    chromaTexture = CVMetalTextureGetTexture(outTexture);
  }
  CVBufferRelease(outTexture);

  if (lumaTexture != nil && chromaTexture != nil) {
    _yTexture = lumaTexture;
    _CrCbTexture = chromaTexture;
    _offset = offsetForRotation((webrtc::VideoRotation)frame.rotation);
  }
}

@end
