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

#if PLATFORM(COCOA)

namespace WebKit {

CoreIPCNSValue::Value CoreIPCNSValue::valueFromNSValue(NSValue *nsValue)
{
    if (!shouldWrapValue(nsValue))
        return CoreIPCSecureCoding { nsValue };

    if (!strcmp(nsValue.objCType, @encode(NSRange)))
        return WrappedNSValue { nsValue.rangeValue };

#if PLATFORM(MAC)
    if (!strcmp(nsValue.objCType, @encode(NSRect)))
        return WrappedNSValue { nsValue.rectValue };
#endif

    RELEASE_ASSERT_NOT_REACHED();
}

CoreIPCNSValue::CoreIPCNSValue(NSValue *value)
    : m_value(valueFromNSValue(value))
{
}

RetainPtr<id> CoreIPCNSValue::toID() const
{
    RetainPtr<id> result;

    auto nsValueFromWrapped = [](const WrappedNSValue& wrappedValue) {
        RetainPtr<id> result;

        WTF::switchOn(wrappedValue, [&](const NSRange& range) {
            result = [NSValue valueWithRange:range];
#if PLATFORM(MAC)
        }, [&](const NSRect& rect) {
            result = [NSValue valueWithRect:rect];
#endif
        });

        return result;
    };

    WTF::switchOn(m_value,
        [&](const CoreIPCNSValue::WrappedNSValue& wrappedValue) {
            result = nsValueFromWrapped(wrappedValue);
        }, [&](const CoreIPCSecureCoding& secureCoding) {
            result = secureCoding.toID();
        }
    );

    return result;
}

bool CoreIPCNSValue::shouldWrapValue(NSValue *value)
{
    if (!strcmp(value.objCType, @encode(NSRange)))
        return true;
#if PLATFORM(MAC)
    if (!strcmp(value.objCType, @encode(NSRect)))
        return true;
#endif

    return false;
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
