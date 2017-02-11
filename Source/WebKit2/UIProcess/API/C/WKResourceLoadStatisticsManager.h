/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once

#include <WebKit/WKBase.h>
#include <wtf/text/WTFString.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    WK_EXPORT WKTypeID WKResourceLoadStatisticsManagerGetTypeID();
    
    WK_EXPORT void WKResourceLoadStatisticsManagerSetPrevalentResource(WKStringRef hostName, bool value);
    WK_EXPORT bool WKResourceLoadStatisticsManagerIsPrevalentResource(WKStringRef hostName);
    WK_EXPORT void WKResourceLoadStatisticsManagerSetHasHadUserInteraction(WKStringRef hostName, bool value);
    WK_EXPORT bool WKResourceLoadStatisticsManagerIsHasHadUserInteraction(WKStringRef hostName);
    WK_EXPORT void WKResourceLoadStatisticsManagerSetTimeToLiveUserInteraction(double seconds);
    WK_EXPORT void WKResourceLoadStatisticsManagerSetReducedTimestampResolution(double seconds);
    WK_EXPORT void WKResourceLoadStatisticsManagerFireDataModificationHandler();
    WK_EXPORT void WKResourceLoadStatisticsManagerSetNotifyPagesWhenDataRecordsWereScanned(bool value);
    WK_EXPORT void WKResourceLoadStatisticsManagerSetShouldClassifyResourcesBeforeDataRecordsRemoval(bool value);
    WK_EXPORT void WKResourceLoadStatisticsManagerSetMinimumTimeBetweeenDataRecordsRemoval(double seconds);
    WK_EXPORT void WKResourceLoadStatisticsManagerResetToConsistentState();

#ifdef __cplusplus
}
#endif
