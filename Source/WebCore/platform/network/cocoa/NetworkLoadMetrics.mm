/*
 * Copyright (C) 2015 Apple, Inc. All rights reserved.
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

#import "ResourceHandle.h"
#import <pal/spi/cocoa/NSURLConnectionSPI.h>

namespace WebCore {

static MonotonicTime dateToMonotonicTime(NSDate *date)
{
    if (auto interval = date.timeIntervalSince1970)
        return WallTime::fromRawSeconds(interval).approximateMonotonicTime();
    return { };
}

static Box<NetworkLoadMetrics> packageTimingData(MonotonicTime redirectStart, NSDate *fetchStart, NSDate *domainLookupStart, NSDate *domainLookupEnd, NSDate *connectStart, NSDate *secureConnectionStart, NSDate *connectEnd, NSDate *requestStart, NSDate *responseStart, bool reusedTLSConnection, NSString *protocol, uint16_t redirectCount, bool failsTAOCheck, bool hasCrossOriginRedirect)
{

    auto timing = Box<NetworkLoadMetrics>::create();

    timing->redirectStart = redirectStart;
    timing->fetchStart = dateToMonotonicTime(fetchStart);
    timing->domainLookupStart = dateToMonotonicTime(domainLookupStart);
    timing->domainLookupEnd = dateToMonotonicTime(domainLookupEnd);
    timing->connectStart = dateToMonotonicTime(connectStart);
    if (reusedTLSConnection && [protocol isEqualToString:@"https"])
        timing->secureConnectionStart = reusedTLSConnectionSentinel;
    else
        timing->secureConnectionStart = dateToMonotonicTime(secureConnectionStart);
    timing->connectEnd = dateToMonotonicTime(connectEnd);
    timing->requestStart = dateToMonotonicTime(requestStart);
    // Sometimes, likely because of <rdar://90997689>, responseStart is before requestStart. If this happens, use the later of the two.
    timing->responseStart = std::max(timing->requestStart, dateToMonotonicTime(responseStart));
    timing->redirectCount = redirectCount;
    timing->failsTAOCheck = failsTAOCheck;
    timing->hasCrossOriginRedirect = hasCrossOriginRedirect;

    // NOTE: responseEnd is not populated in this code path.

    return timing;
}

Box<NetworkLoadMetrics> copyTimingData(NSURLSessionTaskMetrics *incompleteMetrics, const NetworkLoadMetrics& metricsFromTask)
{
    RetainPtr<NSArray<NSURLSessionTaskTransactionMetrics *>> transactionMetrics = incompleteMetrics.transactionMetrics;
    RetainPtr<NSURLSessionTaskTransactionMetrics> metrics = transactionMetrics.get().lastObject;
    return packageTimingData(
        dateToMonotonicTime(transactionMetrics.get().firstObject.fetchStartDate),
        metrics.get().fetchStartDate,
        metrics.get().domainLookupStartDate,
        metrics.get().domainLookupEndDate,
        metrics.get().connectStartDate,
        metrics.get().secureConnectionStartDate,
        metrics.get().connectEndDate,
        metrics.get().requestStartDate,
        metrics.get().responseStartDate,
        metrics.get().reusedConnection,
        metrics.get().response.URL.scheme,
        incompleteMetrics.redirectCount,
        metricsFromTask.failsTAOCheck,
        metricsFromTask.hasCrossOriginRedirect
    );
}

Box<NetworkLoadMetrics> copyTimingData(NSURLConnection *connection, const ResourceHandle& handle)
{
    RetainPtr<NSDictionary> timingData = [connection _timingData];

    auto timingValue = [&](NSString *key) -> RetainPtr<NSDate> {
        if (RetainPtr<NSNumber> number = [timingData objectForKey:key]) {
            if (double doubleValue = number.get().doubleValue)
                return adoptNS([[NSDate alloc] initWithTimeIntervalSinceReferenceDate:doubleValue]);
        }
        return { };
    };

    auto data = packageTimingData(
        handle.startTimeBeforeRedirects(),
        timingValue(@"_kCFNTimingDataFetchStart").get(),
        timingValue(@"_kCFNTimingDataDomainLookupStart").get(),
        timingValue(@"_kCFNTimingDataDomainLookupEnd").get(),
        timingValue(@"_kCFNTimingDataConnectStart").get(),
        timingValue(@"_kCFNTimingDataSecureConnectionStart").get(),
        timingValue(@"_kCFNTimingDataConnectEnd").get(),
        timingValue(@"_kCFNTimingDataRequestStart").get(),
        timingValue(@"_kCFNTimingDataResponseStart").get(),
        timingValue(@"_kCFNTimingDataConnectionReused").get(),
        connection.currentRequest.URL.scheme,
        handle.redirectCount(),
        handle.failsTAOCheck(),
        handle.hasCrossOriginRedirect()
    );

    if (!data->fetchStart)
        data->fetchStart = data->redirectStart;

    return data;
}
    
}
