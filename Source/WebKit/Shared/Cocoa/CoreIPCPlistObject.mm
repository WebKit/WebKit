/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#import "CoreIPCPlistObject.h"

#if PLATFORM(COCOA)

#import "ArgumentCodersCocoa.h"
#import "CoreIPCData.h"
#import "CoreIPCDate.h"
#import "CoreIPCNumber.h"
#import "CoreIPCPlistArray.h"
#import "CoreIPCPlistDictionary.h"
#import "CoreIPCString.h"
#import "GeneratedWebKitSecureCoding.h"
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebKit {

bool CoreIPCPlistObject::isPlistType(id value)
{
    if ([value isKindOfClass:NSString.class]
        || [value isKindOfClass:NSArray.class]
        || [value isKindOfClass:NSData.class]
        || [value isKindOfClass:NSNumber.class]
        || [value isKindOfClass:NSDate.class]
        || [value isKindOfClass:NSDictionary.class])
        return true;
    return false;
}

static PlistValue valueFromID(id object)
{
    switch (IPC::typeFromObject(object)) {
    case IPC::NSType::Array:
        return CoreIPCPlistArray((NSArray *)object);
    case IPC::NSType::Data:
        return CoreIPCData((NSData *)object);
    case IPC::NSType::Date:
        return CoreIPCDate(bridge_cast((NSDate *)object));
    case IPC::NSType::Dictionary:
        return CoreIPCPlistDictionary((NSDictionary *)object);
    case IPC::NSType::Number:
        return CoreIPCNumber(bridge_cast((NSNumber *)object));
    case IPC::NSType::String:
        return CoreIPCString((NSString *)object);
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

CoreIPCPlistObject::CoreIPCPlistObject(id object)
    : m_value(makeUniqueRefWithoutFastMallocCheck<PlistValue>(valueFromID(object)))
{
}

CoreIPCPlistObject::CoreIPCPlistObject(UniqueRef<PlistValue>&& value)
    : m_value(WTFMove(value))
{
}

RetainPtr<id> CoreIPCPlistObject::toID() const
{
    return WTF::switchOn(m_value.get(), [&](auto& object) {
        return object.toID();
    });
}

} // namespace WebKit

namespace IPC {

void ArgumentCoder<UniqueRef<WebKit::PlistValue>>::encode(Encoder& encoder, const UniqueRef<WebKit::PlistValue>& object)
{
    encoder << object.get();
}

std::optional<UniqueRef<WebKit::PlistValue>> ArgumentCoder<UniqueRef<WebKit::PlistValue>>::decode(Decoder& decoder)
{
    auto object = decoder.decode<WebKit::PlistValue>();
    if (!object)
        return std::nullopt;
    return makeUniqueRefWithoutFastMallocCheck<WebKit::PlistValue>(WTFMove(*object));
}

} // namespace IPC

#endif // PLATFORM(COCOA)
