/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#import "config.h"
#import "SourceAlphaCoreImageApplier.h"

#if USE(CORE_IMAGE)

#import "FilterImage.h"
#import <CoreImage/CIFilter.h>
#import <CoreImage/CoreImage.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SourceAlphaCoreImageApplier);

bool SourceAlphaCoreImageApplier::apply(const Filter&, std::span<const Ref<FilterImage>> inputs, FilterImage& result) const
{
    ASSERT(inputs.size() == 1);
    Ref input = inputs[0].get();

    RetainPtr inputImage = input->ciImage();
    if (!inputImage)
        return false;

    RetainPtr alphaFilter = [CIFilter filterWithName:@"CIColorMatrix"];
    [alphaFilter setValue:inputImage.get() forKey:kCIInputImageKey];

    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputRVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputGVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputBVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:1] forKey:@"inputAVector"];
    [alphaFilter setValue:[CIVector vectorWithX:0 Y:0 Z:0 W:0] forKey:@"inputBiasVector"];

    result.setCIImage(alphaFilter.get().outputImage);

    return true;
}

} // namespace WebCore

#endif // USE(CORE_IMAGE)
