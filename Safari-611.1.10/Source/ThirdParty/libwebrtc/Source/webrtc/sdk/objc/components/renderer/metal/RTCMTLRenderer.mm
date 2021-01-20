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

#import "base/RTCLogging.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"

#include "api/video/video_rotation.h"
#include "rtc_base/checks.h"

// As defined in shaderSource.
static NSString *const vertexFunctionName = @"vertexPassthrough";
static NSString *const fragmentFunctionName = @"fragmentColorConversion";

static NSString *const pipelineDescriptorLabel = @"RTCPipeline";
static NSString *const commandBufferLabel = @"RTCCommandBuffer";
static NSString *const renderEncoderLabel = @"RTCEncoder";
static NSString *const renderEncoderDebugGroup = @"RTCDrawFrame";

// Computes the texture coordinates given rotation and cropping.
static inline void getCubeVertexData(int cropX,
                                     int cropY,
                                     int cropWidth,
                                     int cropHeight,
                                     size_t frameWidth,
                                     size_t frameHeight,
                                     RTCVideoRotation rotation,
                                     float *buffer) {
  // The computed values are the adjusted texture coordinates, in [0..1].
  // For the left and top, 0.0 means no cropping and e.g. 0.2 means we're skipping 20% of the
  // left/top edge.
  // For the right and bottom, 1.0 means no cropping and e.g. 0.8 means we're skipping 20% of the
  // right/bottom edge (i.e. render up to 80% of the width/height).
  float cropLeft = cropX / (float)frameWidth;
  float cropRight = (cropX + cropWidth) / (float)frameWidth;
  float cropTop = cropY / (float)frameHeight;
  float cropBottom = (cropY + cropHeight) / (float)frameHeight;

  // These arrays map the view coordinates to texture coordinates, taking cropping and rotation
  // into account. The first two columns are view coordinates, the last two are texture coordinates.
  switch (rotation) {
    case RTCVideoRotation_0: {
      float values[16] = {-1.0, -1.0, cropLeft, cropBottom,
                           1.0, -1.0, cropRight, cropBottom,
                          -1.0,  1.0, cropLeft, cropTop,
                           1.0,  1.0, cropRight, cropTop};
      memcpy(buffer, &values, sizeof(values));
    } break;
    case RTCVideoRotation_90: {
      float values[16] = {-1.0, -1.0, cropRight, cropBottom,
                           1.0, -1.0, cropRight, cropTop,
                          -1.0,  1.0, cropLeft, cropBottom,
                           1.0,  1.0, cropLeft, cropTop};
      memcpy(buffer, &values, sizeof(values));
    } break;
    case RTCVideoRotation_180: {
      float values[16] = {-1.0, -1.0, cropRight, cropTop,
                           1.0, -1.0, cropLeft, cropTop,
                          -1.0,  1.0, cropRight, cropBottom,
                           1.0,  1.0, cropLeft, cropBottom};
      memcpy(buffer, &values, sizeof(values));
    } break;
    case RTCVideoRotation_270: {
      float values[16] = {-1.0, -1.0, cropLeft, cropTop,
                           1.0, -1.0, cropLeft, cropBottom,
                          -1.0, 1.0, cropRight, cropTop,
                           1.0, 1.0, cropRight, cropBottom};
      memcpy(buffer, &values, sizeof(values));
    } break;
  }
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

  // Values affecting the vertex buffer. Stored for comparison to avoid unnecessary recreation.
  int _oldFrameWidth;
  int _oldFrameHeight;
  int _oldCropWidth;
  int _oldCropHeight;
  int _oldCropX;
  int _oldCropY;
  RTCVideoRotation _oldRotation;
}

@synthesize rotationOverride = _rotationOverride;

- (instancetype)init {
  if (self = [super init]) {
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
    _view = view;
    view.device = _device;
    view.preferredFramesPerSecond = 30;
    view.autoResizeDrawable = NO;

    [self loadAssets];

    float vertexBufferArray[16] = {0};
    _vertexBuffer = [_device newBufferWithBytes:vertexBufferArray
                                         length:sizeof(vertexBufferArray)
                                        options:MTLResourceCPUCacheModeWriteCombined];
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

- (void)getWidth:(int *)width
          height:(int *)height
       cropWidth:(int *)cropWidth
      cropHeight:(int *)cropHeight
           cropX:(int *)cropX
           cropY:(int *)cropY
         ofFrame:(nonnull RTCVideoFrame *)frame {
  RTC_NOTREACHED() << "Virtual method not implemented in subclass.";
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  // Apply rotation override if set.
  RTCVideoRotation rotation;
  NSValue *rotationOverride = self.rotationOverride;
  if (rotationOverride) {
#if defined(__IPHONE_11_0) && defined(__IPHONE_OS_VERSION_MAX_ALLOWED) && \
    (__IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_11_0)
    if (@available(iOS 11, *)) {
      [rotationOverride getValue:&rotation size:sizeof(rotation)];
    } else
#endif
    {
      [rotationOverride getValue:&rotation];
    }
  } else {
    rotation = frame.rotation;
  }

  int frameWidth, frameHeight, cropWidth, cropHeight, cropX, cropY;
  [self getWidth:&frameWidth
          height:&frameHeight
       cropWidth:&cropWidth
      cropHeight:&cropHeight
           cropX:&cropX
           cropY:&cropY
         ofFrame:frame];

  // Recompute the texture cropping and recreate vertexBuffer if necessary.
  if (cropX != _oldCropX || cropY != _oldCropY || cropWidth != _oldCropWidth ||
      cropHeight != _oldCropHeight || rotation != _oldRotation || frameWidth != _oldFrameWidth ||
      frameHeight != _oldFrameHeight) {
    getCubeVertexData(cropX,
                      cropY,
                      cropWidth,
                      cropHeight,
                      frameWidth,
                      frameHeight,
                      rotation,
                      (float *)_vertexBuffer.contents);
    _oldCropX = cropX;
    _oldCropY = cropY;
    _oldCropWidth = cropWidth;
    _oldCropHeight = cropHeight;
    _oldRotation = rotation;
    _oldFrameWidth = frameWidth;
    _oldFrameHeight = frameHeight;
  }

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
  NSString *shaderSource = [self shaderSource];

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

- (void)render {
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
    [renderEncoder setVertexBuffer:_vertexBuffer offset:0 atIndex:0];
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
    // Wait until the inflight (curently sent to GPU) command buffer
    // has completed the GPU work.
    dispatch_semaphore_wait(_inflight_semaphore, DISPATCH_TIME_FOREVER);

    if ([self setupTexturesForFrame:frame]) {
      [self render];
    } else {
      dispatch_semaphore_signal(_inflight_semaphore);
    }
  }
}

@end
