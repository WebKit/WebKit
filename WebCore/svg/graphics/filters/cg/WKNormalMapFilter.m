/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WKNormalMapFilter.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)

static CIKernel *convolveKernel = nil;
static CIKernel *normalMapKernel = nil;

@implementation WKNormalMapFilter

+ (void)initialize
{
    [CIFilter registerFilterName:@"WKNormalMap"
                     constructor:self
                 classAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
                     @"WebKit Normal Map", kCIAttributeFilterDisplayName,
                     [NSArray arrayWithObjects: kCICategoryBlur, kCICategoryVideo,
                         kCICategoryStillImage, kCICategoryNonSquarePixels, nil], kCIAttributeFilterCategories,       
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeMin,
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputSurfaceScale",
                     nil]];
}

+ (CIFilter *)filterWithName:(NSString *)name
{
    return [[[self alloc] init] autorelease];
}

- (id)init
{
    if (!normalMapKernel) {
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        NSString *kernelFile = [bundle pathForResource:@"WKNormalMapFilter" ofType:@"cikernel"];
        NSString *code = [NSString stringWithContentsOfFile:kernelFile encoding:NSUTF8StringEncoding error:0];
        NSArray *kernels = [CIKernel kernelsWithString:code];
        convolveKernel = [[kernels objectAtIndex:0] retain];
        normalMapKernel = [[kernels objectAtIndex:1] retain];
    }
    return [super init];
}

- (NSArray *)xConvolveArgsWithBumpMap:(CISampler *)bumpMap {
    return [NSArray arrayWithObjects:
        bumpMap,
        [NSNumber numberWithFloat:4],
        [NSNumber numberWithFloat:0],
        [CIVector vectorWithX:1 Y:2 Z:1],
        [CIVector vectorWithX:0 Y:0 Z:0],
        [CIVector vectorWithX:-1 Y:-2 Z:-1],
        nil];
}

- (NSArray *)yConvolveArgsWithBumpMap:(CISampler *)bumpMap {
    return [NSArray arrayWithObjects: 
        bumpMap,
        [NSNumber numberWithFloat:4],
        [NSNumber numberWithFloat:0],
        [CIVector vectorWithX:1 Y:0 Z:-1],
        [CIVector vectorWithX:2 Y:0 Z:-2],
        [CIVector vectorWithX:1 Y:0 Z:-1],
        nil];
}

- (CIImage *)outputImage
{
    CISampler *image = [CISampler samplerWithImage:inputImage];
    NSDictionary *applyOptions = [NSDictionary dictionaryWithObjectsAndKeys:[image definition], kCIApplyOptionDefinition, nil];
    
    CIImage *convolveX = [self apply:convolveKernel arguments:[self xConvolveArgsWithBumpMap:image] options:applyOptions];
    CIImage *convolveY = [self apply:convolveKernel arguments:[self yConvolveArgsWithBumpMap:image] options:applyOptions];
    CISampler *samplerX = [CISampler samplerWithImage:convolveX];
    CISampler *samplerY = [CISampler samplerWithImage:convolveY];
    
    NSArray *normalMapArgs = [NSArray arrayWithObjects:samplerX, samplerY, image, inputSurfaceScale, nil];
    return [self apply:normalMapKernel arguments:normalMapArgs options:applyOptions];
}

@end

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
