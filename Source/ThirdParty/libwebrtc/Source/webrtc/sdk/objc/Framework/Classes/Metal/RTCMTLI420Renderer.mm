/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCMTLI420Renderer.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#import "WebRTC/RTCLogging.h"
#import "WebRTC/RTCVideoFrame.h"

#import "RTCMTLRenderer+Private.h"

#define MTL_STRINGIFY(s) @ #s

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

    fragment half4 fragmentColorConversion(
        Varyings in[[stage_in]], texture2d<float, access::sample> textureY[[texture(0)]],
        texture2d<float, access::sample> textureU[[texture(1)]],
        texture2d<float, access::sample> textureV[[texture(2)]]) {
      constexpr sampler s(address::clamp_to_edge, filter::linear);
      float y;
      float u;
      float v;
      float r;
      float g;
      float b;
      // Conversion for YUV to rgb from http://www.fourcc.org/fccyvrgb.php
      y = textureY.sample(s, in.texcoord).r;
      u = textureU.sample(s, in.texcoord).r;
      v = textureV.sample(s, in.texcoord).r;
      u = u - 0.5;
      v = v - 0.5;
      r = y + 1.403 * v;
      g = y - 0.344 * u - 0.714 * v;
      b = y + 1.770 * u;

      float4 out = float4(r, g, b, 1.0);

      return half4(out);
    });

@implementation RTCMTLI420Renderer {
  // Textures.
  id<MTLTexture> _yTexture;
  id<MTLTexture> _uTexture;
  id<MTLTexture> _vTexture;

  MTLTextureDescriptor *_descriptor;
  MTLTextureDescriptor *_chromaDescriptor;

  int _width;
  int _height;
  int _chromaWidth;
  int _chromaHeight;
}

#pragma mark - Virtual

- (NSString *)shaderSource {
  return shaderSource;
}

- (BOOL)setupTexturesForFrame:(nonnull RTCVideoFrame *)frame {
  [super setupTexturesForFrame:frame];

  id<MTLDevice> device = [self currentMetalDevice];
  if (!device) {
    return NO;
  }

  // Luma (y) texture.
  if (!_descriptor || (_width != frame.width && _height != frame.height)) {
    _width = frame.width;
    _height = frame.height;
    _descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                                     width:_width
                                                                    height:_height
                                                                 mipmapped:NO];
    _descriptor.usage = MTLTextureUsageShaderRead;
    _yTexture = [device newTextureWithDescriptor:_descriptor];
  }

  // Chroma (u,v) textures
  [_yTexture replaceRegion:MTLRegionMake2D(0, 0, _width, _height)
               mipmapLevel:0
                 withBytes:frame.dataY
               bytesPerRow:frame.strideY];

  if (!_chromaDescriptor ||
      (_chromaWidth != frame.width / 2 && _chromaHeight != frame.height / 2)) {
    _chromaWidth = frame.width / 2;
    _chromaHeight = frame.height / 2;
    _chromaDescriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatR8Unorm
                                                           width:_chromaWidth
                                                          height:_chromaHeight
                                                       mipmapped:NO];
    _chromaDescriptor.usage = MTLTextureUsageShaderRead;
    _uTexture = [device newTextureWithDescriptor:_chromaDescriptor];
    _vTexture = [device newTextureWithDescriptor:_chromaDescriptor];
  }

  [_uTexture replaceRegion:MTLRegionMake2D(0, 0, _chromaWidth, _chromaHeight)
               mipmapLevel:0
                 withBytes:frame.dataU
               bytesPerRow:frame.strideU];
  [_vTexture replaceRegion:MTLRegionMake2D(0, 0, _chromaWidth, _chromaHeight)
               mipmapLevel:0
                 withBytes:frame.dataV
               bytesPerRow:frame.strideV];

  return (_uTexture != nil) && (_yTexture != nil) && (_vTexture != nil);
}

- (void)uploadTexturesToRenderEncoder:(id<MTLRenderCommandEncoder>)renderEncoder {
  [renderEncoder setFragmentTexture:_yTexture atIndex:0];
  [renderEncoder setFragmentTexture:_uTexture atIndex:1];
  [renderEncoder setFragmentTexture:_vTexture atIndex:2];
}

@end
