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

static Box<NetworkLoadMetrics> packageTimingData(double fetchStart, double domainLookupStart, double domainLookupEnd, double connectStart, double secureConnectionStart, double connectEnd, double requestStart, double responseStart, bool reusedTLSConnection, NSString *protocol)
{
    auto timing = Box<NetworkLoadMetrics>::create();

    timing->fetchStart = Seconds(fetchStart);
    timing->domainLookupStart = Seconds(domainLookupStart <= 0 ? -1 : domainLookupStart - fetchStart);
    timing->domainLookupEnd = Seconds(domainLookupEnd <= 0 ? -1 : domainLookupEnd - fetchStart);
    timing->connectStart = Seconds(connectStart <= 0 ? -1 : connectStart - fetchStart);
    if (reusedTLSConnection && [protocol isEqualToString:@"https"])
        timing->secureConnectionStart = reusedTLSConnectionSentinel;
    else
        timing->secureConnectionStart = Seconds(secureConnectionStart <= 0 ? -1 : secureConnectionStart - fetchStart);
    timing->connectEnd = Seconds(connectEnd <= 0 ? -1 : connectEnd - fetchStart);
    timing->requestStart = Seconds(requestStart <= 0 ? 0 : requestStart - fetchStart);
    timing->responseStart = Seconds(responseStart <= 0 ? 0 : responseStart - fetchStart);

    // NOTE: responseEnd is not populated in this code path.

    return timing;
}

Box<NetworkLoadMetrics> copyTimingData(NSURLSessionTaskTransactionMetrics *incompleteMetrics)
{
    return packageTimingData(
        incompleteMetrics.fetchStartDate.timeIntervalSince1970,
        incompleteMetrics.domainLookupStartDate.timeIntervalSince1970,
        incompleteMetrics.domainLookupEndDate.timeIntervalSince1970,
        incompleteMetrics.connectStartDate.timeIntervalSince1970,
        incompleteMetrics.secureConnectionStartDate.timeIntervalSince1970,
        incompleteMetrics.connectEndDate.timeIntervalSince1970,
        incompleteMetrics.requestStartDate.timeIntervalSince1970,
        incompleteMetrics.responseStartDate.timeIntervalSince1970,
        incompleteMetrics.reusedConnection,
        incompleteMetrics.response.URL.scheme
    );
}

Box<NetworkLoadMetrics> copyTimingData(NSURLConnection *connection)
{
    NSDictionary *timingData = [connection _timingData];
    if (!timingData)
        return nullptr;

    auto timingValue = [](NSDictionary *timingData, NSString *key) {
        if (id object = [timingData objectForKey:key])
            return [object doubleValue];
        return 0.0;
    };

    return packageTimingData(
        timingValue(timingData, @"_kCFNTimingDataFetchStart"),
        timingValue(timingData, @"_kCFNTimingDataDomainLookupStart"),
        timingValue(timingData, @"_kCFNTimingDataDomainLookupEnd"),
        timingValue(timingData, @"_kCFNTimingDataConnectStart"),
        timingValue(timingData, @"_kCFNTimingDataSecureConnectionStart"),
        timingValue(timingData, @"_kCFNTimingDataConnectEnd"),
        timingValue(timingData, @"_kCFNTimingDataRequestStart"),
        timingValue(timingData, @"_kCFNTimingDataResponseStart"),
        timingValue(timingData, @"_kCFNTimingDataConnectionReused"),
        connection.currentRequest.URL.scheme
    );
}
    
}
