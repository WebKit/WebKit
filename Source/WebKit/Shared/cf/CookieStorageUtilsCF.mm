/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#import "CookieStorageUtilsCF.h"

#import <pal/spi/cocoa/NSURLConnectionSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/cf/VectorCF.h>

namespace WebKit {

RetainPtr<CFHTTPCookieStorageRef> cookieStorageFromIdentifyingData(const Vector<uint8_t>& data)
{
    ASSERT(!data.isEmpty());
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto cookieStorageData = adoptCF(CFDataCreate(kCFAllocatorDefault, data.data(), data.size()));
    auto cookieStorage = adoptCF(CFHTTPCookieStorageCreateFromIdentifyingData(kCFAllocatorDefault, cookieStorageData.get()));
    ASSERT(cookieStorage);

    CFHTTPCookieStorageScheduleWithRunLoop(cookieStorage.get(), [NSURLConnection resourceLoaderRunLoop], kCFRunLoopCommonModes);

    return cookieStorage;
}

Vector<uint8_t> identifyingDataFromCookieStorage(CFHTTPCookieStorageRef cookieStorage)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanAccessRawCookies));

    auto cfData = adoptCF(CFHTTPCookieStorageCreateIdentifyingData(kCFAllocatorDefault, cookieStorage));
    return vectorFromCFData(cfData.get());
}

} // namespace WebKit
