/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "CoreIPCWebKitSecureCoding.h"

#if ENABLE(DATA_DETECTION)
#if PLATFORM(COCOA)

#import "CoreIPCDictionary.h"
#import "CoreIPCNSCFObject.h"
#import <pal/cocoa/DataDetectorsCoreSoftLink.h>

@interface NSObject ()
- (NSDictionary *)_webKitPropertyListData;
- (id)_initWithWebKitPropertyListData:(NSDictionary *)plist;
@end

namespace WebKit {

static Class webKitSecureCodingClassFromNSType(IPC::NSType type)
{
    switch (type) {
    case IPC::NSType::DDScannerResult:
        return PAL::getDDScannerResultClass();
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static bool shouldWrapWithPropertyList(id object)
{
    auto objectClass = [object class];
    return [objectClass instancesRespondToSelector:@selector(_webKitPropertyListData)]
        && [objectClass instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)];
}

CoreIPCWebKitSecureCoding::Value CoreIPCWebKitSecureCoding::valueFromNSSecureCoding(NSObject<NSSecureCoding> *object)
{
    if (shouldWrapWithPropertyList(object))
        return CoreIPCDictionary { [object _webKitPropertyListData] };
    return CoreIPCSecureCoding { object };
}

CoreIPCWebKitSecureCoding::CoreIPCWebKitSecureCoding(NSObject<NSSecureCoding> *object)
    : m_type(IPC::typeFromObject(object))
    , m_value(valueFromNSSecureCoding(object))
{
}

RetainPtr<id> CoreIPCWebKitSecureCoding::toID() const
{
    RetainPtr<id> result;

    WTF::switchOn(m_value,
        [&](const CoreIPCDictionary& dictionary) {
            ASSERT([webKitSecureCodingClassFromNSType(m_type) instancesRespondToSelector:@selector(_initWithWebKitPropertyListData:)]);
            result = adoptNS([[webKitSecureCodingClassFromNSType(m_type) alloc] _initWithWebKitPropertyListData:dictionary.toID().get()]);
        }, [&](const CoreIPCSecureCoding& secureCoding) {
            result = secureCoding.toID();
        }
    );

    return result;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
#endif // ENABLE(DATA_DETECTION)
