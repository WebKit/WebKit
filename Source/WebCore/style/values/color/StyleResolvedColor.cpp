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
#include "StyleResolvedColor.h"

#include "ColorSerialization.h"
#include "StyleColor.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

// MARK: - Conversion.

Color toStyleColor(const CSS::ResolvedColor& unresolved, ColorResolutionState&)
{
    return Color { ResolvedColor { unresolved.value } };
}

// MARK: - Serialization

void serializationForCSSTokenization(StringBuilder& builder, const CSS::SerializationContext&, const ResolvedColor& absoluteColor)
{
    builder.append(WebCore::serializationForCSS(absoluteColor.color));
}

String serializationForCSSTokenization(const CSS::SerializationContext&, const ResolvedColor& absoluteColor)
{
    return WebCore::serializationForCSS(absoluteColor.color);
}

// MARK: - TextStream

WTF::TextStream& operator<<(WTF::TextStream& ts, const ResolvedColor& absoluteColor)
{
    return ts << "absoluteColor("_s << absoluteColor.color.debugDescription() << ')';
}

} // namespace Style
} // namespace WebCore
