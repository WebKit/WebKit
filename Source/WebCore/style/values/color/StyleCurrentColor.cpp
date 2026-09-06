/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "config.h"
#include "StyleCurrentColor.h"

#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

CurrentColor::CurrentColor(Property property)
    : m_property(property)
{
}

// MARK: - Serialization

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext&, const CurrentColor& currentColor)
{
    switch (currentColor.property()) {
    case CurrentColor::Property::Color:
        builder.append("currentcolor"_s);
        return;
    case CurrentColor::Property::AccentColor:
        builder.append("AccentColor"_s);
        return;
    default:
        ASSERT_NOT_REACHED();
        builder.append("<invalid color>"_s);
        break;
    }
}

WTF::String serializationForCSSTokenization(const CSS::SerializationContext&, const CurrentColor& currentColor)
{
    switch (currentColor.property()) {
    case CurrentColor::Property::Color:
        return "currentcolor"_s;
    case CurrentColor::Property::AccentColor:
        return "AccentColor"_s;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return "<invalid color>"_s;
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const CurrentColor& currentColor)
{
    switch (currentColor.property()) {
    case CurrentColor::Property::Color:
        return ts << "currentcolor"_s;
    case CurrentColor::Property::AccentColor:
        return ts << "AccentColor"_s;
    default:
        break;
    }

    ASSERT_NOT_REACHED();
    return ts << "<invalid color>"_s;
}

} // namespace Style
} // namespace WebCore
