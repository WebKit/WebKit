/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "PlatformTimeRanges.h"

#if PLATFORM(COCOA)

#import <AVFoundation/AVFoundation.h>
#import <pal/avfoundation/MediaTimeAVFoundation.h>

#import <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

RetainPtr<NSArray> makeNSArray(const PlatformTimeRanges& timeRanges)
{
    RetainPtr ranges = adoptNS([[NSMutableArray alloc] initWithCapacity:timeRanges.length()]);

    for (unsigned i = 0; i < timeRanges.length(); ++i) {
        bool startValid;
        MediaTime start = timeRanges.start(i, startValid);
        RELEASE_ASSERT(startValid);

        bool endValid;
        MediaTime end = timeRanges.end(i, endValid);
        RELEASE_ASSERT(endValid);

        [ranges addObject:[NSValue valueWithCMTimeRange:PAL::CMTimeRangeMake(PAL::toCMTime(start), PAL::toCMTime(end - start))]];
    }

    return adoptNS([ranges copy]);
}

} // namespace WebCore

#endif // PLATFORM(COCOA)
