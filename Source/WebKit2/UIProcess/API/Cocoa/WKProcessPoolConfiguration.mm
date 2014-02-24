/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKProcessPoolConfigurationPrivate.h"

#if WK_API_ENABLED

#import <wtf/RetainPtr.h>

@implementation WKProcessPoolConfiguration {
    RetainPtr<NSURL> _injectedBundleURL;
}

- (NSURL *)_injectedBundleURL
{
    return _injectedBundleURL.get();
}

- (void)_setInjectedBundleURL:(NSURL *)injectedBundleURL
{
    _injectedBundleURL = adoptNS([injectedBundleURL copy]);
}

- (NSString *)description
{
    NSString *description = [NSString stringWithFormat:@"<%@: %p; maximumProcessCount = %lu", NSStringFromClass(self.class), self, static_cast<unsigned long>(_maximumProcessCount)];
    if (_injectedBundleURL)
        return [description stringByAppendingFormat:@"; injectedBundleURL: \"%@\">", _injectedBundleURL.get()];

    return [description stringByAppendingString:@">"];
}

- (id)copyWithZone:(NSZone *)zone
{
    WKProcessPoolConfiguration *configuration = [[[self class] allocWithZone:zone] init];

    configuration.maximumProcessCount = self.maximumProcessCount;
    configuration._injectedBundleURL = self._injectedBundleURL;

    return configuration;
}

@end

#endif
