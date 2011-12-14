/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "WebApplicationCacheQuotaManager.h"

#import "WebSecurityOriginInternal.h"
#import <WebCore/ApplicationCacheStorage.h>

using namespace WebCore;

@implementation WebApplicationCacheQuotaManager

- (id)initWithOrigin:(WebSecurityOrigin *)origin
{
    self = [super init];
    if (!self)
        return nil;

    _origin = origin;
    return self;
}

- (WebSecurityOrigin *)origin
{
    return _origin;
}

- (unsigned long long)usage
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    long long usage;
    if (cacheStorage().usageForOrigin([_origin _core], usage))
        return usage;
    return 0;
#else
    return 0;
#endif
}

- (unsigned long long)quota
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    long long quota;
    if (cacheStorage().quotaForOrigin([_origin _core], quota))
        return quota;
    return 0;
#else
    return 0;
#endif
}

- (void)setQuota:(unsigned long long)quota
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    cacheStorage().storeUpdatedQuotaForOrigin([_origin _core], quota);
#endif
}

@end
