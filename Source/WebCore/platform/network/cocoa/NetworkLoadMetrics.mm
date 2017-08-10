/*
 * Copyright (C) 2015 Apple, Inc.  All rights reserved.
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
#import "NetworkLoadMetrics.h"

#import <pal/spi/cocoa/NSURLConnectionSPI.h>

namespace WebCore {

static double timingValue(NSDictionary *timingData, NSString *key)
{
    if (id object = [timingData objectForKey:key])
        return [object doubleValue];
    return 0.0;
}
    
void copyTimingData(NSDictionary *timingData, NetworkLoadMetrics& timing)
{
    if (!timingData)
        return;

    // This is not the navigationStart time in monotonic time, but the other times are relative to this time
    // and only the differences between times are stored.
    double referenceStart = timingValue(timingData, @"_kCFNTimingDataFetchStart");

    double domainLookupStart = timingValue(timingData, @"_kCFNTimingDataDomainLookupStart");
    double domainLookupEnd = timingValue(timingData, @"_kCFNTimingDataDomainLookupEnd");
    double connectStart = timingValue(timingData, @"_kCFNTimingDataConnectStart");
    double secureConnectionStart = timingValue(timingData, @"_kCFNTimingDataSecureConnectionStart");
    double connectEnd = timingValue(timingData, @"_kCFNTimingDataConnectEnd");
    double requestStart = timingValue(timingData, @"_kCFNTimingDataRequestStart");
    double responseStart = timingValue(timingData, @"_kCFNTimingDataResponseStart");

    timing.domainLookupStart = Seconds(domainLookupStart <= 0 ? -1 : domainLookupStart - referenceStart);
    timing.domainLookupEnd = Seconds(domainLookupEnd <= 0 ? -1 : domainLookupEnd - referenceStart);
    timing.connectStart = Seconds(connectStart <= 0 ? -1 : connectStart - referenceStart);
    timing.secureConnectionStart = Seconds(secureConnectionStart <= 0 ? -1 : secureConnectionStart - referenceStart);
    timing.connectEnd = Seconds(connectEnd <= 0 ? -1 : connectEnd - referenceStart);
    timing.requestStart = Seconds(requestStart <= 0 ? 0 : requestStart - referenceStart);
    timing.responseStart = Seconds(responseStart <= 0 ? 0 : responseStart - referenceStart);

    // NOTE: responseEnd is not populated in this code path.
}

#if !HAVE(TIMINGDATAOPTIONS)
void setCollectsTimingData()
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        [NSURLConnection _setCollectsTimingData:YES];
        [NSURLConnection _collectTimingDataWithOptions:TimingDataCollectionNStatsOff | TimingDataCollectionConnectionDataOff];
    });
}
#endif
    
}
