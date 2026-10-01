/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if PLATFORM(COCOA)

#import <objc/runtime.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/WTFString.h>

// The DOM identity a WebDriver-synthesized key event should report, taken from the WebDriver key
// table. Several WebDriver keys have no equivalent on Apple keyboards, so their 'key' and 'code'
// cannot be derived from a platform key code; and on iOS no key code is supplied at all. The
// automation session attaches an identity to the platform event (an NSEvent on macOS, a WebEvent
// on iOS), and the event factory reports it in place of the derived values.
namespace WebKit::AutomationKeyIdentity {

struct Identity {
    String key;
    String code;
    bool isKeypad { false };
};

// Inline, so that every translation unit shares one address for the key.
inline constexpr char associatedObjectKey { };

static NSString * const keyKey = @"key";
static NSString * const codeKey = @"code";
static NSString * const isKeypadKey = @"isKeypad";

inline void setIdentity(id event, const Identity& identity)
{
    objc_setAssociatedObject(event, &associatedObjectKey, @{
        keyKey: identity.key.createNSString().get(),
        codeKey: identity.code.createNSString().get(),
        isKeypadKey: @(identity.isKeypad),
    }, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}

inline std::optional<Identity> identity(id event)
{
    RetainPtr dictionary = dynamic_objc_cast<NSDictionary>(objc_getAssociatedObject(event, &associatedObjectKey));
    if (!dictionary)
        return std::nullopt;

    return Identity {
        dynamic_objc_cast<NSString>([dictionary objectForKey:keyKey]),
        dynamic_objc_cast<NSString>([dictionary objectForKey:codeKey]),
        static_cast<bool>([dynamic_objc_cast<NSNumber>([dictionary objectForKey:isKeypadKey]) boolValue]),
    };
}

} // namespace WebKit::AutomationKeyIdentity

#endif // PLATFORM(COCOA)
