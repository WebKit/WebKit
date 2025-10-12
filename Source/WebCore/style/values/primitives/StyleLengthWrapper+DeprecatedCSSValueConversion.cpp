/*
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "StyleLengthWrapper+DeprecatedCSSValueConversion.h"

#include "ContainerNodeInlines.h"
#include "NodeDocument.h"

namespace WebCore {
namespace Style {

std::optional<CSSToLengthConversionData> deprecatedLengthConversionCreateCSSToLengthConversionData(RefPtr<Element> element)
{
    CheckedPtr elementRenderer = element->renderer();
    if (!elementRenderer)
        return std::nullopt;
    CheckedPtr elementParentRenderer = elementRenderer->parent();
    Ref document = element->document();
    CheckedPtr documentElement = document->documentElement();
    if (!documentElement)
        return std::nullopt;

    // FIXME: Investigate container query units
    return CSSToLengthConversionData {
        elementRenderer->style(),
        documentElement->renderer() ? &documentElement->renderer()->style() : nullptr,
        elementParentRenderer ? &elementParentRenderer->style() : nullptr,
        document->renderView()
    };
}

} // namespace Style
} // namespace WebCore
