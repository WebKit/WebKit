/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
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
#include "MathMLPresentationElement.h"

#if ENABLE(MATHML)

#include "ElementIterator.h"
#include "HTMLHtmlElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTTPParsers.h"
#include "MathMLMathElement.h"
#include "MathMLNames.h"
#include "RenderMathMLBlock.h"
#include "RenderTableCell.h"
#include "SVGSVGElement.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLPresentationElement);

using namespace MathMLNames;

MathMLPresentationElement::MathMLPresentationElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
}

Ref<MathMLPresentationElement> MathMLPresentationElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLPresentationElement(tagName, document));
}

RenderPtr<RenderElement> MathMLPresentationElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition& insertionPosition)
{
    if (hasTagName(mtableTag))
        return createRenderer<RenderMathMLTable>(*this, WTFMove(style));

    return MathMLElement::createElementRenderer(WTFMove(style), insertionPosition);
}

bool MathMLPresentationElement::isPhrasingContent(const Node& node)
{
    // Phrasing content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#phrasing-content.

    if (!node.isElementNode())
        return node.isTextNode();

    if (is<MathMLElement>(node)) {
        auto& mathmlElement = downcast<MathMLElement>(node);
        return is<MathMLMathElement>(mathmlElement);
    }

    if (is<SVGElement>(node)) {
        auto& svgElement = downcast<SVGElement>(node);
        return is<SVGSVGElement>(svgElement);
    }

    if (is<HTMLElement>(node)) {
        auto& htmlElement = downcast<HTMLElement>(node);
        return htmlElement.hasTagName(HTMLNames::aTag)
            || htmlElement.hasTagName(HTMLNames::abbrTag)
            || (htmlElement.hasTagName(HTMLNames::areaTag) && ancestorsOfType<HTMLMapElement>(htmlElement).first())
            || htmlElement.hasTagName(HTMLNames::audioTag)
            || htmlElement.hasTagName(HTMLNames::bTag)
            || htmlElement.hasTagName(HTMLNames::bdiTag)
            || htmlElement.hasTagName(HTMLNames::bdoTag)
            || htmlElement.hasTagName(HTMLNames::brTag)
            || htmlElement.hasTagName(HTMLNames::buttonTag)
            || htmlElement.hasTagName(HTMLNames::canvasTag)
            || htmlElement.hasTagName(HTMLNames::citeTag)
            || htmlElement.hasTagName(HTMLNames::codeTag)
            || htmlElement.hasTagName(HTMLNames::datalistTag)
            || htmlElement.hasTagName(HTMLNames::dataTag)
            || htmlElement.hasTagName(HTMLNames::delTag)
            || htmlElement.hasTagName(HTMLNames::dfnTag)
            || htmlElement.hasTagName(HTMLNames::emTag)
            || htmlElement.hasTagName(HTMLNames::embedTag)
            || htmlElement.hasTagName(HTMLNames::iTag)
            || htmlElement.hasTagName(HTMLNames::iframeTag)
            || htmlElement.hasTagName(HTMLNames::imgTag)
            || htmlElement.hasTagName(HTMLNames::inputTag)
            || htmlElement.hasTagName(HTMLNames::insTag)
            || htmlElement.hasTagName(HTMLNames::kbdTag)
            || htmlElement.hasTagName(HTMLNames::keygenTag)
            || htmlElement.hasTagName(HTMLNames::labelTag)
            || htmlElement.hasTagName(HTMLNames::mapTag)
            || htmlElement.hasTagName(HTMLNames::markTag)
            || htmlElement.hasTagName(HTMLNames::meterTag)
            || htmlElement.hasTagName(HTMLNames::noscriptTag)
            || htmlElement.hasTagName(HTMLNames::objectTag)
            || htmlElement.hasTagName(HTMLNames::outputTag)
            || htmlElement.hasTagName(HTMLNames::progressTag)
            || htmlElement.hasTagName(HTMLNames::qTag)
            || htmlElement.hasTagName(HTMLNames::rubyTag)
            || htmlElement.hasTagName(HTMLNames::sTag)
            || htmlElement.hasTagName(HTMLNames::sampTag)
            || htmlElement.hasTagName(HTMLNames::scriptTag)
            || htmlElement.hasTagName(HTMLNames::selectTag)
            || htmlElement.hasTagName(HTMLNames::smallTag)
            || htmlElement.hasTagName(HTMLNames::spanTag)
            || htmlElement.hasTagName(HTMLNames::strongTag)
            || htmlElement.hasTagName(HTMLNames::subTag)
            || htmlElement.hasTagName(HTMLNames::supTag)
            || htmlElement.hasTagName(HTMLNames::templateTag)
            || htmlElement.hasTagName(HTMLNames::textareaTag)
            || htmlElement.hasTagName(HTMLNames::timeTag)
            || htmlElement.hasTagName(HTMLNames::uTag)
            || htmlElement.hasTagName(HTMLNames::varTag)
            || htmlElement.hasTagName(HTMLNames::videoTag)
            || htmlElement.hasTagName(HTMLNames::wbrTag);
    }

    return false;
}

bool MathMLPresentationElement::isFlowContent(const Node& node)
{
    // Flow content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#flow-content

    if (isPhrasingContent(node))
        return true;

    if (!is<HTMLElement>(node))
        return false;

    auto& htmlElement = downcast<HTMLElement>(node);
    return htmlElement.hasTagName(HTMLNames::addressTag)
        || htmlElement.hasTagName(HTMLNames::articleTag)
        || htmlElement.hasTagName(HTMLNames::asideTag)
        || htmlElement.hasTagName(HTMLNames::blockquoteTag)
        || htmlElement.hasTagName(HTMLNames::detailsTag)
        || htmlElement.hasTagName(HTMLNames::dialogTag)
        || htmlElement.hasTagName(HTMLNames::divTag)
        || htmlElement.hasTagName(HTMLNames::dlTag)
        || htmlElement.hasTagName(HTMLNames::fieldsetTag)
        || htmlElement.hasTagName(HTMLNames::figureTag)
        || htmlElement.hasTagName(HTMLNames::footerTag)
        || htmlElement.hasTagName(HTMLNames::formTag)
        || htmlElement.hasTagName(HTMLNames::h1Tag)
        || htmlElement.hasTagName(HTMLNames::h2Tag)
        || htmlElement.hasTagName(HTMLNames::h3Tag)
        || htmlElement.hasTagName(HTMLNames::h4Tag)
        || htmlElement.hasTagName(HTMLNames::h5Tag)
        || htmlElement.hasTagName(HTMLNames::h6Tag)
        || htmlElement.hasTagName(HTMLNames::headerTag)
        || htmlElement.hasTagName(HTMLNames::hrTag)
        || htmlElement.hasTagName(HTMLNames::mainTag)
        || htmlElement.hasTagName(HTMLNames::navTag)
        || htmlElement.hasTagName(HTMLNames::olTag)
        || htmlElement.hasTagName(HTMLNames::pTag)
        || htmlElement.hasTagName(HTMLNames::preTag)
        || htmlElement.hasTagName(HTMLNames::sectionTag)
        || (htmlElement.hasTagName(HTMLNames::styleTag) && htmlElement.hasAttribute("scoped"))
        || htmlElement.hasTagName(HTMLNames::tableTag)
        || htmlElement.hasTagName(HTMLNames::ulTag);
}

const MathMLElement::BooleanValue& MathMLPresentationElement::cachedBooleanAttribute(const QualifiedName& name, Optional<BooleanValue>& attribute)
{
    if (attribute)
        return attribute.value();

    // In MathML, attribute values are case-sensitive.
    const AtomString& value = attributeWithoutSynchronization(name);
    if (value == "true")
        attribute = BooleanValue::True;
    else if (value == "false")
        attribute = BooleanValue::False;
    else
        attribute = BooleanValue::Default;

    return attribute.value();
}

MathMLElement::Length MathMLPresentationElement::parseNumberAndUnit(const StringView& string)
{
    LengthType lengthType = LengthType::UnitLess;
    unsigned stringLength = string.length();
    UChar lastChar = string[stringLength - 1];
    if (lastChar == '%') {
        lengthType = LengthType::Percentage;
        stringLength--;
    } else if (stringLength >= 2) {
        UChar penultimateChar = string[stringLength - 2];
        if (penultimateChar == 'c' && lastChar == 'm')
            lengthType = LengthType::Cm;
        if (penultimateChar == 'e' && lastChar == 'm')
            lengthType = LengthType::Em;
        else if (penultimateChar == 'e' && lastChar == 'x')
            lengthType = LengthType::Ex;
        else if (penultimateChar == 'i' && lastChar == 'n')
            lengthType = LengthType::In;
        else if (penultimateChar == 'm' && lastChar == 'm')
            lengthType = LengthType::Mm;
        else if (penultimateChar == 'p' && lastChar == 'c')
            lengthType = LengthType::Pc;
        else if (penultimateChar == 'p' && lastChar == 't')
            lengthType = LengthType::Pt;
        else if (penultimateChar == 'p' && lastChar == 'x')
            lengthType = LengthType::Px;

        if (lengthType != LengthType::UnitLess)
            stringLength -= 2;
    }

    bool ok;
    float lengthValue = string.substring(0, stringLength).toFloat(ok);
    if (!ok)
        return Length();

    Length length;
    length.type = lengthType;
    length.value = lengthValue;
    return length;
}

MathMLElement::Length MathMLPresentationElement::parseNamedSpace(const StringView& string)
{
    // Named space values are case-sensitive.
    int namedSpaceValue;
    if (string == "veryverythinmathspace")
        namedSpaceValue = 1;
    else if (string == "verythinmathspace")
        namedSpaceValue = 2;
    else if (string == "thinmathspace")
        namedSpaceValue = 3;
    else if (string == "mediummathspace")
        namedSpaceValue = 4;
    else if (string == "thickmathspace")
        namedSpaceValue = 5;
    else if (string == "verythickmathspace")
        namedSpaceValue = 6;
    else if (string == "veryverythickmathspace")
        namedSpaceValue = 7;
    else if (string == "negativeveryverythinmathspace")
        namedSpaceValue = -1;
    else if (string == "negativeverythinmathspace")
        namedSpaceValue = -2;
    else if (string == "negativethinmathspace")
        namedSpaceValue = -3;
    else if (string == "negativemediummathspace")
        namedSpaceValue = -4;
    else if (string == "negativethickmathspace")
        namedSpaceValue = -5;
    else if (string == "negativeverythickmathspace")
        namedSpaceValue = -6;
    else if (string == "negativeveryverythickmathspace")
        namedSpaceValue = -7;
    else
        return Length();

    Length length;
    length.type = LengthType::MathUnit;
    length.value = namedSpaceValue;
    return length;
}

MathMLElement::Length MathMLPresentationElement::parseMathMLLength(const String& string)
{
    // The regular expression from the MathML Relax NG schema is as follows:
    //
    //   pattern = '\s*((-?[0-9]*([0-9]\.?|\.[0-9])[0-9]*(e[mx]|in|cm|mm|p[xtc]|%)?)|(negative)?((very){0,2}thi(n|ck)|medium)mathspace)\s*'
    //
    // We do not perform a strict verification of the syntax of whitespaces and number.
    // Instead, we just use isHTMLSpace and toFloat to parse these parts.

    // We first skip whitespace from both ends of the string.
    StringView stringView = string;
    StringView strippedLength = stripLeadingAndTrailingHTTPSpaces(stringView);

    if (strippedLength.isEmpty())
        return Length();

    // We consider the most typical case: a number followed by an optional unit.
    UChar firstChar = strippedLength[0];
    if (isASCIIDigit(firstChar) || firstChar == '-' || firstChar == '.')
        return parseNumberAndUnit(strippedLength);

    // Otherwise, we try and parse a named space.
    return parseNamedSpace(strippedLength);
}

const MathMLElement::Length& MathMLPresentationElement::cachedMathMLLength(const QualifiedName& name, Optional<Length>& length)
{
    if (length)
        return length.value();
    length = parseMathMLLength(attributeWithoutSynchronization(name));
    return length.value();
}

MathMLElement::MathVariant MathMLPresentationElement::parseMathVariantAttribute(const AtomString& attributeValue)
{
    // The mathvariant attribute values is case-sensitive.
    if (attributeValue == "normal")
        return MathVariant::Normal;
    if (attributeValue == "bold")
        return MathVariant::Bold;
    if (attributeValue == "italic")
        return MathVariant::Italic;
    if (attributeValue == "bold-italic")
        return MathVariant::BoldItalic;
    if (attributeValue == "double-struck")
        return MathVariant::DoubleStruck;
    if (attributeValue == "bold-fraktur")
        return MathVariant::BoldFraktur;
    if (attributeValue == "script")
        return MathVariant::Script;
    if (attributeValue == "bold-script")
        return MathVariant::BoldScript;
    if (attributeValue == "fraktur")
        return MathVariant::Fraktur;
    if (attributeValue == "sans-serif")
        return MathVariant::SansSerif;
    if (attributeValue == "bold-sans-serif")
        return MathVariant::BoldSansSerif;
    if (attributeValue == "sans-serif-italic")
        return MathVariant::SansSerifItalic;
    if (attributeValue == "sans-serif-bold-italic")
        return MathVariant::SansSerifBoldItalic;
    if (attributeValue == "monospace")
        return MathVariant::Monospace;
    if (attributeValue == "initial")
        return MathVariant::Initial;
    if (attributeValue == "tailed")
        return MathVariant::Tailed;
    if (attributeValue == "looped")
        return MathVariant::Looped;
    if (attributeValue == "stretched")
        return MathVariant::Stretched;
    return MathVariant::None;
}

Optional<MathMLElement::MathVariant> MathMLPresentationElement::specifiedMathVariant()
{
    if (!acceptsMathVariantAttribute())
        return WTF::nullopt;
    if (!m_mathVariant)
        m_mathVariant = parseMathVariantAttribute(attributeWithoutSynchronization(mathvariantAttr));
    return m_mathVariant.value() == MathVariant::None ? WTF::nullopt : m_mathVariant;
}

void MathMLPresentationElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    bool mathVariantAttribute = name == mathvariantAttr && acceptsMathVariantAttribute();
    if (mathVariantAttribute)
        m_mathVariant = WTF::nullopt;
    if ((mathVariantAttribute) && renderer())
        MathMLStyle::resolveMathMLStyleTree(renderer());

    MathMLElement::parseAttribute(name, value);
}

}

#endif // ENABLE(MATHML)
