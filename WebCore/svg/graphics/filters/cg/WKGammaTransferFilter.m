/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "WKGammaTransferFilter.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)

static CIKernel *gammaTransferFilter = nil;

@implementation WKGammaTransferFilter
+ (void)initialize
{
    [CIFilter registerFilterName:@"WKGammaTransfer"
                     constructor:self
                 classAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
                     @"WebKit Gamma Transfer", kCIAttributeFilterDisplayName,
                     [NSArray arrayWithObjects:kCICategoryStylize, kCICategoryVideo,
                         kCICategoryStillImage, kCICategoryNonSquarePixels,nil], kCIAttributeFilterCategories,
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputAmplitude",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:1.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:1.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputExponent",
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         [NSNumber numberWithDouble:0.0], kCIAttributeDefault,
                         [NSNumber numberWithDouble:0.0], kCIAttributeIdentity,
                         kCIAttributeTypeScalar, kCIAttributeType,
                         nil], @"inputOffset",
                     nil]];
}

+ (CIFilter *)filterWithName:(NSString *)name
{
    return [[[self alloc] init] autorelease];
}

- (id)init
{
    if (!gammaTransferFilter) {
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        NSString *kernelFile = [bundle pathForResource:@"WKGammaTransferFilter" ofType:@"cikernel"];
        NSString *code = [NSString stringWithContentsOfFile:kernelFile encoding:NSUTF8StringEncoding error:0];
        NSArray *kernels = [CIKernel kernelsWithString:code];
        gammaTransferFilter = [[kernels objectAtIndex:0] retain];
    }
    return [super init];
}

- (CIImage *)outputImage
{
    CISampler *inputSampler = [CISampler samplerWithImage: inputImage];
    return [self apply:gammaTransferFilter, inputSampler, inputAmplitude, inputExponent, inputOffset, @"definition", [inputSampler definition], nil];
}

@end

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
