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
#import "WKDiffuseLightingFilter.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)

static CIKernel *diffuseLightingFilter = nil;
@implementation WKDiffuseLightingFilter

+ (void)initialize
{
    [CIFilter registerFilterName:@"WKDiffuseLighting"  
                     constructor:self
                 classAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
                     @"WebKit Diffuse Lighting", kCIAttributeFilterDisplayName,
                     [NSArray arrayWithObjects:kCICategoryStylize, kCICategoryVideo,
                         kCICategoryStillImage, kCICategoryNonSquarePixels,nil], kCIAttributeFilterCategories,
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [CIColor colorWithRed:1.0f green:1.0f blue:1.0f alpha:1.0f],
                         kCIAttributeDefault, nil], @"inputLightingColor",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeMin,
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputSurfaceScale",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeMin,
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputDiffuseConstant",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeMin,
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputKernelUnitLengthX",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeMin,
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputKernelUnitLengthY",
                     nil]];
}

+ (CIFilter *)filterWithName:(NSString *)name
{
    return [[[self alloc] init] autorelease];
}

- (id)init
{
    if (!diffuseLightingFilter) {
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        NSString *kernelFile = [bundle pathForResource:@"WKDiffuseLightingFilter" ofType:@"cikernel"];
        NSString *code = [NSString stringWithContentsOfFile:kernelFile encoding:NSUTF8StringEncoding error:0];
        NSArray *kernels = [CIKernel kernelsWithString:code];
        diffuseLightingFilter = [[kernels objectAtIndex:0] retain];
    }
    return [super init];
}

- (CIImage *)outputImage
{
    return [self apply:diffuseLightingFilter, inputNormalMap, inputLightVectors, inputLightingColor, inputSurfaceScale, inputDiffuseConstant,
        inputKernelUnitLengthX, inputKernelUnitLengthY, nil];
}

@end

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
