/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ExtendedStyleColor.h"

#include <wtf/text/TextStream.h>

namespace WebCore {

namespace Style {

ExtendedStyleColor::ExtendedStyleColor(ExtendedColorKind&& color)
    : m_color { WTFMove(color) }
{
}

WebCore::Color ExtendedStyleColor::resolveColor(const WebCore::Color& currentColor) const
{
    return WTF::switchOn(m_color,
        [&](const auto& kind) {
            return Style::resolveColor(kind, currentColor);
        }
    );
}

bool ExtendedStyleColor::containsCurrentColor() const
{
    return WTF::switchOn(m_color,
        [&](const auto& kind) {
            return Style::containsCurrentColor(kind);
        }
    );
}

String serializationForCSS(const ExtendedStyleColor& color)
{
    return WTF::switchOn(color.m_color,
        [](const auto& kind) {
            return serializationForCSS(kind);
        }
    );
}

void serializationForCSS(StringBuilder& builder, const ExtendedStyleColor& color)
{
    return WTF::switchOn(color.m_color,
        [&](const auto& kind) {
            serializationForCSS(builder, kind);
        }
    );
}

WTF::TextStream& operator<<(WTF::TextStream& out, const ExtendedStyleColor& color)
{
    out << "ExtendedStyleColor[";
    WTF::switchOn(color.m_color,
        [&](const auto& kind) {
            out << kind;
        }
    );
    out << "]";
    return out;
}

} // namespace Style

} // namespace WebCore
