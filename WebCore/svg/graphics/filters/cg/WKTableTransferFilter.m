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
#import "WKTableTransferFilter.h"

#if ENABLE(SVG) && ENABLE(SVG_FILTERS)

static CIKernel *tableTransferFilter = nil;

@implementation WKTableTransferFilter
+ (void)initialize
{
    [CIFilter registerFilterName:@"WKTableTransfer"
                     constructor:self
                 classAttributes:[NSDictionary dictionaryWithObjectsAndKeys:
                     @"WebKit Table Transfer", kCIAttributeFilterDisplayName,
                     [NSArray arrayWithObjects:kCICategoryStylize, kCICategoryVideo,
                         kCICategoryStillImage, kCICategoryNonSquarePixels,nil], kCIAttributeFilterCategories,              
                     [NSDictionary dictionaryWithObjectsAndKeys:
                         kCIAttributeTypeGradient, kCIAttributeType,
                         nil], @"inputTable",   
                     nil]];
}

+ (CIFilter *)filterWithName:(NSString *)name
{
    return [[[self alloc] init] autorelease];
}

- (id)init
{
    if (!tableTransferFilter) {
        NSBundle *bundle = [NSBundle bundleForClass:[self class]];
        NSString *kernelFile = [bundle pathForResource:@"WKTableTransferFilter" ofType:@"cikernel"];
        NSString *code = [NSString stringWithContentsOfFile:kernelFile encoding:NSUTF8StringEncoding error:0];
        NSArray *kernels = [CIKernel kernelsWithString:code];
        tableTransferFilter = [[kernels objectAtIndex:0] retain];
    }
    return [super init];
}

- (CIImage *)outputImage
{
    CISampler *inputSampler = [CISampler samplerWithImage: inputImage];
    CISampler *tableSampler = [CISampler samplerWithImage: inputTable keysAndValues:kCISamplerFilterMode, kCISamplerFilterLinear, kCISamplerWrapMode, kCISamplerWrapClamp, nil];
    NSArray *args = [NSArray arrayWithObjects:inputSampler, tableSampler, inputSelector, 
        [NSNumber numberWithDouble:[inputTable extent].size.width - 1.0f], @"definition", [inputSampler definition], nil];
    return [self apply:tableTransferFilter arguments:args options:nil];
}

@end

#endif // ENABLE(SVG) && ENABLE(SVG_FILTERS)
