/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#import "TransferString.h"

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <wtf/spi/cocoa/NSObjCRuntimeSPI.h>

NS_DIRECT_MEMBERS
@interface _WKTransferStringWrapper : NSObject {
    IPC::TransferString _string;
}
- (instancetype)initWithString:(IPC::TransferString&&)string;
- (IPC::TransferString&)string;
@end

@implementation _WKTransferStringWrapper

- (instancetype)initWithString:(IPC::TransferString&&)string
{
    if (!(self = [super init]))
        return nil;

    _string = WTF::move(string);
    return self;
}

- (IPC::TransferString&)string
{
    return _string;
}

@end

static void *transferStringWrapperKey = &transferStringWrapperKey;

namespace IPC {

std::optional<TransferString> TransferString::createCached(NSString *string)
{
    RetainPtr wrapper = (_WKTransferStringWrapper *)objc_getAssociatedObject(string, transferStringWrapperKey);
    if (!wrapper) {
        auto result = TransferString::create((__bridge CFStringRef)string);
        if (!result)
            return std::nullopt;

        // Caching only makes sense if we can re-send a previously created shared memory handle.
        bool shouldCache = WTF::switchOn(result->m_storage,
            [](const String&) { return false; },
            [](const RetainPtr<CFStringRef>& string) { return false; },
            [](const SharedSpan8& handle) { return true; },
            [](const SharedSpan16& handle) { return true; }
        );
        if (!shouldCache)
            return result;

        wrapper = adoptNS([[_WKTransferStringWrapper alloc] initWithString:WTF::move(*result)]);
        objc_setAssociatedObject(string, transferStringWrapperKey, wrapper.get(), OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    }

    return TransferString { [wrapper string].toIPCData() };
}

}
