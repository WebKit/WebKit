/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLRenderer+Private.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"

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

static inline int offsetForRotation(RTCVideoRotation rotation) {
  switch (rotation) {
    case RTCVideoRotation_0:
      return 0;
    case RTCVideoRotation_90:
      return 16;
    case RTCVideoRotation_180:
      return 32;
    case RTCVideoRotation_270:
      return 48;
  }
  return 0;
}

// The max number of command buffers in flight (submitted to GPU).
// For now setting it up to 1.
// In future we might use triple buffering method if it improves performance.
static const NSInteger kMaxInflightBuffers = 1;

@implementation RTCMTLRenderer {
  __kindof MTKView *_view;

  // Controller.
  dispatch_semaphore_t _inflight_semaphore;

  // Renderer.
  id<MTLDevice> _device;
  id<MTLCommandQueue> _commandQueue;
  id<MTLLibrary> _defaultLibrary;
  id<MTLRenderPipelineState> _pipelineState;

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
    success = YES;
  }
  return success;
}
#pragma mark - Inheritance

- (id<MTLDevice>)currentMetalDevice {
  return _device;
}

- (NSString *)shaderSource {
  RTC_NOTREACHED() << "Virtual method not implemented in subclass.";
  return nil;
}

- (void)uploadTexturesToRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder {
  RTC_NOTREACHED() << "Virtual method not implemented in subclass.";
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  _offset = offsetForRotation(frame.rotation);
  return YES;
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
      [_device newLibraryWithSource:[self shaderSource] options:NULL error:&libraryError];

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
  id<MTLFunction> fragmentFunction = [_defaultLibrary newFunctionWithName:fragmentFunctionName];

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

  MTLRenderPassDescriptor *renderPassDescriptor = _view.currentRenderPassDescriptor;
  if (renderPassDescriptor) {  // Valid drawable.
    id<MTLRenderCommandEncoder> renderEncoder =
        [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
    renderEncoder.label = renderEncoderLabel;

    // Set context state.
    [renderEncoder pushDebugGroup:renderEncoderDebugGroup];
    [renderEncoder setRenderPipelineState:_pipelineState];
    [renderEncoder setVertexBuffer:_vertexBuffer offset:_offset * sizeof(float) atIndex:0];
    [self uploadTexturesToRenderEncoder:renderEncoder];

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
  @autoreleasepool {
    if ([self setupTexturesForFrame:frame]) {
      [self render];
    }
  }
}

@end
