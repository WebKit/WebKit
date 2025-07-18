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
#import "CoreIPCNSValue.h"

#import "CoreIPCNSCFObject.h"
#import "CoreIPCTypes.h"
#import <wtf/ObjCRuntimeExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKAppKitStubs.h>
#endif

#if PLATFORM(COCOA)

namespace WebKit {

CoreIPCNSValue::CoreIPCNSValue(CoreIPCNSValue&&) = default;

CoreIPCNSValue::~CoreIPCNSValue() = default;

CoreIPCNSValue::CoreIPCNSValue(Value&& value)
    : m_value(WTFMove(value)) { }

auto CoreIPCNSValue::valueFromNSValue(NSValue *nsValue) -> Value
{
    if (nsValueHasObjCType<NSRange>(nsValue))
        return IPCRange { nsValue.rangeValue.location, nsValue.rangeValue.length };

    if (nsValueHasObjCType<CGRect>(nsValue))
        return nsValue.rectValue;

    return makeUniqueRef<CoreIPCNSCFObject>(nsValue);
}

CoreIPCNSValue::CoreIPCNSValue(NSValue *value)
    : m_value(valueFromNSValue(value))
{
}

RetainPtr<id> CoreIPCNSValue::toID() const
{
    RetainPtr<id> result;

    auto nsValueFromWrapped = [](const Value& wrappedValue) {
        RetainPtr<id> result;

        WTF::switchOn(wrappedValue, [&](const IPCRange& range) {
            result = [NSValue valueWithRange:NSMakeRange(range.location, range.length)];
        }, [&](const DoubleRect& rect) {
            result = [NSValue valueWithRect:rect.toCG()];
        }, [&](const UniqueRef<CoreIPCNSCFObject>& object) {
            result = object->toID();
        });

        return result;
    };

    return nsValueFromWrapped(m_value);
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
