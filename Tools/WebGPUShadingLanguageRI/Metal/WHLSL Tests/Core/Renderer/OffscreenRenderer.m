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

#import "OffscreenRenderer.h"

@interface OffscreenRenderer ()
{
    id<MTLTexture> _outputTexture;
}

@end

@implementation OffscreenRenderer

- (void)setLibrary:(id<MTLLibrary>)library vertexShaderName:(NSString*)vertexShader fragmentShaderName:(NSString*)fragmentShaderName
{
    [super setLibrary:library vertexShaderName:vertexShader fragmentShaderName:fragmentShaderName pixelFormat:self.outputPixelFormat];
}

- (void)setTargetWidth:(NSUInteger)width setTargetHeight:(NSUInteger)height
{
    [super setTargetWidth:width setTargetHeight:height];
    MTLTextureDescriptor *outputDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:self.outputPixelFormat width:self.targetWidth height:self.targetHeight mipmapped:NO];
    outputDescriptor.usage = MTLTextureUsageRenderTarget;
    outputDescriptor.storageMode = MTLStorageModeManaged;
    _outputTexture = [self.device newTextureWithDescriptor:outputDescriptor];
}

- (MTLRenderPassDescriptor*)renderPassDescriptor
{
    MTLRenderPassDescriptor *renderPassDescriptor = [MTLRenderPassDescriptor renderPassDescriptor];
    renderPassDescriptor.colorAttachments[0].texture = _outputTexture;
    return renderPassDescriptor;
}

- (void)configureCommandBufferBeforeCommit:(id<MTLCommandBuffer>)commandBuffer
{
    id <MTLBlitCommandEncoder> blitEncoder = [commandBuffer blitCommandEncoder];
    [blitEncoder synchronizeResource:_outputTexture];
    [blitEncoder endEncoding];
}

- (void)readTextureDataToBuffer:(uint8_t *)buffer bytesPerRow:(NSUInteger)bytesPerRow
{
    // The texture isn't mipmapped, so we use the default level of 0
    [_outputTexture getBytes:buffer bytesPerRow:bytesPerRow fromRegion:MTLRegionMake2D(0, 0, self.targetWidth, self.targetHeight) mipmapLevel:0];
}

- (int)intValue
{
    // We always have 4 bytes per pixel
    NSUInteger length = self.targetWidth * self.targetHeight * 4;
    uint8_t *buffer = calloc(length, sizeof(uint8_t));
    [self readTextureDataToBuffer:(uint8_t*)buffer bytesPerRow:4 * self.targetWidth];
    int intValue = ((int*)buffer)[0];
    free(buffer);
    return intValue;
}

- (unsigned)uintValue
{
    // We always have 4 bytes per pixel
    NSUInteger length = self.targetWidth * self.targetHeight * 4;
    uint8_t *buffer = calloc(length, sizeof(uint8_t));
    [self readTextureDataToBuffer:(uint8_t*)buffer bytesPerRow:4 * self.targetWidth];
    unsigned uintValue = ((unsigned*)buffer)[0];
    free(buffer);
    return uintValue;
}

- (float)floatValue
{
    // We always have 4 bytes per pixel
    NSUInteger length = self.targetWidth * self.targetHeight * 4;
    uint8_t *buffer = calloc(length, sizeof(uint8_t));
    [self readTextureDataToBuffer:(uint8_t*)buffer bytesPerRow:4 * self.targetWidth];
    float floatValue = ((float*)buffer)[0];
    free(buffer);
    return floatValue;
}

@end
