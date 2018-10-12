/*
 *  Copyright 2017 The WebRTC Project Authors. All rights reserved.
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

#import "RTCMTLRenderer+Private.h"
#import "base/RTCLogging.h"
#import "base/RTCVideoFrame.h"
#import "base/RTCVideoFrameBuffer.h"
#import "components/video_frame_buffer/RTCCVPixelBuffer.h"

#include "rtc_base/checks.h"

static NSString *const shaderSource = MTL_STRINGIFY(
    using namespace metal;

    typedef struct {
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

@implementation RTCMTLNV12Renderer {
  // Textures.
  CVMetalTextureCacheRef _textureCache;
  id<MTLTexture> _yTexture;
  id<MTLTexture> _CrCbTexture;
}

- (BOOL)addRenderingDestination:(__kindof MTKView *)view {
  if ([super addRenderingDestination:view]) {
    return [self initializeTextureCache];
  }
  return NO;
}

- (BOOL)initializeTextureCache {
  CVReturn status = CVMetalTextureCacheCreate(kCFAllocatorDefault, nil, [self currentMetalDevice],
                                              nil, &_textureCache);
  if (status != kCVReturnSuccess) {
    RTCLogError(@"Metal: Failed to initialize metal texture cache. Return status is %d", status);
    return NO;
  }

  return YES;
}

- (NSString *)shaderSource {
  return shaderSource;
}

- (void)getWidth:(nonnull int *)width
          height:(nonnull int *)height
       cropWidth:(nonnull int *)cropWidth
      cropHeight:(nonnull int *)cropHeight
           cropX:(nonnull int *)cropX
           cropY:(nonnull int *)cropY
         ofFrame:(nonnull RTCVideoFrame *)frame {
  RTCCVPixelBuffer *pixelBuffer = (RTCCVPixelBuffer *)frame.buffer;
  *width = CVPixelBufferGetWidth(pixelBuffer.pixelBuffer);
  *height = CVPixelBufferGetHeight(pixelBuffer.pixelBuffer);
  *cropWidth = pixelBuffer.cropWidth;
  *cropHeight = pixelBuffer.cropHeight;
  *cropX = pixelBuffer.cropX;
  *cropY = pixelBuffer.cropY;
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  RTC_DCHECK([frame.buffer isKindOfClass:[RTCCVPixelBuffer class]]);
  if (![super setupTexturesForFrame:frame]) {
    return NO;
  }
  CVPixelBufferRef pixelBuffer = ((RTCCVPixelBuffer *)frame.buffer).pixelBuffer;

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
    return YES;
  }
  return NO;
}

- (void)uploadTexturesToRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder {
  [renderEncoder setFragmentTexture:_yTexture atIndex:0];
  [renderEncoder setFragmentTexture:_CrCbTexture atIndex:1];
}

- (void)dealloc {
  if (_textureCache) {
    CFRelease(_textureCache);
  }
}
@end
