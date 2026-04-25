/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SVGPropertyTraits.h"

#include "CSSParser.h"
#include "CSSPropertyParserConsumer+ColorInlines.h"
#include "ColorSerialization.h"
#include "ContainerNodeInlines.h"
#include "RenderElement.h"
#include "RenderObjectStyle.h"
#include "RenderStyle+GettersInlines.h"
#include "SVGElement.h"
#include "StyleColor.h"
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

namespace {

class SVGStyleColorResolutionDelegate final : public CSS::PlatformColorResolutionDelegate {
public:
    explicit SVGStyleColorResolutionDelegate(Ref<SVGElement> element)
        : m_element { WTF::move(element) }
    {
    }

    Color currentColor() const override;

    const Ref<SVGElement> m_element;
};

Color SVGStyleColorResolutionDelegate::currentColor() const
{
    if (CheckedPtr renderer = m_element->renderer())
        return protect(renderer->style())->visitedDependentColor();
    return { };
}

} // anonymous namespace

Color SVGPropertyTraits<Color>::fromString(SVGElement& targetElement, const String& string)
{
    using namespace CSSPropertyParserHelpers;

    Ref document = targetElement.document();
    auto& cssParserContext = document->cssParserContext();

    auto trimmedString = string.trim(deprecatedIsSpaceOrNewline);

    auto color = parseColorRawSimple(trimmedString, cssParserContext);
    if (color.isValid())
        return color;

    SVGStyleColorResolutionDelegate delegate(targetElement);
    CSSColorParsingOptions options;
    CSS::PlatformColorResolutionState state {
        .delegate = &delegate
    };
    return parseColorRawGeneral(trimmedString, cssParserContext, document, options, state);
}

int SVGPropertyTraits<int>::fromString(SVGElement&, const String& string)
{
    return parseInteger<int>(string).value_or(0);
}

String SVGPropertyTraits<Color>::toString(const Color& type)
{
    return serializationForHTML(type);
}

} // namespace WebCore
