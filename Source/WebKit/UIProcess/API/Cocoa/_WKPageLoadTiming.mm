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
#import "_WKPageLoadTimingInternal.h"

#import "WebPageLoadTiming.h"
#import <WebCore/WebCoreObjCExtras.h>

static NSDate *nsDateFromMonotonicTime(WallTime time)
{
    if (!time)
        return nil;
    return [NSDate dateWithTimeIntervalSince1970:time.secondsSinceEpoch().value()];
}

@implementation _WKPageLoadTiming {
    WallTime _navigationStart;
    WallTime _firstMeaningfulPaint;
    WallTime _documentFinishedLoading;
    WallTime _allSubresourcesFinishedLoading;
}

- (instancetype)_initWithTiming:(const WebKit::WebPageLoadTiming&)timing
{
    if (!(self = [super init]))
        return nil;

    _navigationStart = timing.navigationStart();
    _firstMeaningfulPaint = timing.firstMeaningfulPaint();
    _documentFinishedLoading = timing.documentFinishedLoading();
    _allSubresourcesFinishedLoading = timing.allSubresourcesFinishedLoading();

    return self;
}

- (NSDate *)navigationStart
{
    return nsDateFromMonotonicTime(_navigationStart);
}

- (NSDate *)firstMeaningfulPaint
{
    return nsDateFromMonotonicTime(_firstMeaningfulPaint);
}

- (NSDate *)documentFinishedLoading
{
    return nsDateFromMonotonicTime(_documentFinishedLoading);
}

- (NSDate *)allSubresourcesFinishedLoading
{
    return nsDateFromMonotonicTime(_allSubresourcesFinishedLoading);
}

@end
