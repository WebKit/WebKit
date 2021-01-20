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

#if HAVE(TOUCH_BAR)

#include "ArgumentCoders.h"
#include <wtf/EnumTraits.h>
#include <wtf/text/WTFString.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebCore {
class HTMLMenuItemElement;
}

namespace WebKit {
    
// Based on NSTouchBarItem types.
enum ItemType {
    Button
};

struct TouchBarMenuItemData {
    explicit TouchBarMenuItemData() = default;
    explicit TouchBarMenuItemData(const WebCore::HTMLMenuItemElement&);
    explicit TouchBarMenuItemData(const TouchBarMenuItemData&) = default;
    
    void encode(IPC::Encoder&) const;
    static Optional<TouchBarMenuItemData> decode(IPC::Decoder&);
    
    ItemType type { ItemType::Button };
    String identifier;
    float priority { 0.0 };
    bool validTouchBarDisplay { true };
};
    
// Touch Bar Menu Items will be ordered based on priority.
inline bool operator<(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return lhs.priority < rhs.priority;
}

inline bool operator>(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return rhs < lhs;
}

inline bool operator<=(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return !(lhs > rhs);
}

inline bool operator>=(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return !(lhs < rhs);
}

inline bool operator==(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return lhs.type == rhs.type
        && lhs.identifier == rhs.identifier
        && lhs.priority == rhs.priority;
}

inline bool operator!=(const TouchBarMenuItemData& lhs, const TouchBarMenuItemData& rhs)
{
    return !(lhs == rhs);
}

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::ItemType> {
    using values = EnumValues<
        WebKit::ItemType,
        WebKit::ItemType::Button
    >;
};

} // namespace WTF

#endif // HAVE(TOUCH_BAR)
