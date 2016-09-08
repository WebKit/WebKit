/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
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
#include "MathMLElement.h"

#if ENABLE(MATHML)

#include "ElementIterator.h"
#include "Event.h"
#include "EventHandler.h"
#include "HTMLAnchorElement.h"
#include "HTMLElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MathMLMathElement.h"
#include "MathMLNames.h"
#include "MathMLSelectElement.h"
#include "MouseEvent.h"
#include "RenderTableCell.h"
#include "SVGElement.h"
#include "SVGNames.h"
#include "SVGSVGElement.h"
#include "XLinkNames.h"

namespace WebCore {

using namespace MathMLNames;

MathMLElement::MathMLElement(const QualifiedName& tagName, Document& document)
    : StyledElement(tagName, document, CreateMathMLElement)
{
}

Ref<MathMLElement> MathMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLElement(tagName, document));
}

bool MathMLElement::isPhrasingContent(const Node& node) const
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
        // FIXME: add the <data> and <time> tags when they are implemented.
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
            || htmlElement.hasTagName(HTMLNames::uTag)
            || htmlElement.hasTagName(HTMLNames::varTag)
            || htmlElement.hasTagName(HTMLNames::videoTag)
            || htmlElement.hasTagName(HTMLNames::wbrTag);
    }

    return false;
}

bool MathMLElement::isFlowContent(const Node& node) const
{
    // Flow content is described in the HTML 5 specification:
    // http://www.w3.org/TR/html5/dom.html#flow-content

    if (isPhrasingContent(node))
        return true;

    if (!is<HTMLElement>(node))
        return false;

    auto& htmlElement = downcast<HTMLElement>(node);
    // FIXME add the <dialog> tag when it is implemented.
    return htmlElement.hasTagName(HTMLNames::addressTag)
        || htmlElement.hasTagName(HTMLNames::articleTag)
        || htmlElement.hasTagName(HTMLNames::asideTag)
        || htmlElement.hasTagName(HTMLNames::blockquoteTag)
        || htmlElement.hasTagName(HTMLNames::detailsTag)
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

unsigned MathMLElement::colSpan() const
{
    if (!hasTagName(mtdTag))
        return 1u;
    auto& colSpanValue = attributeWithoutSynchronization(columnspanAttr);
    return std::max(1u, limitToOnlyHTMLNonNegative(colSpanValue, 1u));
}

unsigned MathMLElement::rowSpan() const
{
    if (!hasTagName(mtdTag))
        return 1u;
    auto& rowSpanValue = attributeWithoutSynchronization(rowspanAttr);
    static const unsigned maxRowspan = 8190; // This constant comes from HTMLTableCellElement.
    return std::max(1u, std::min(limitToOnlyHTMLNonNegative(rowSpanValue, 1u), maxRowspan));
}

void MathMLElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            setNeedsStyleRecalc();
    } else if (name == rowspanAttr) {
        if (is<RenderTableCell>(renderer()) && hasTagName(mtdTag))
            downcast<RenderTableCell>(*renderer()).colSpanOrRowSpanChanged();
    } else if (name == columnspanAttr) {
        if (is<RenderTableCell>(renderer()) && hasTagName(mtdTag))
            downcast<RenderTableCell>(renderer())->colSpanOrRowSpanChanged();
    } else
        StyledElement::parseAttribute(name, value);
}

bool MathMLElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == colorAttr || name == dirAttr || name == fontfamilyAttr || name == fontsizeAttr || name == fontstyleAttr || name == fontweightAttr || name == mathbackgroundAttr || name == mathcolorAttr || name == mathsizeAttr)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

void MathMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == mathbackgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == mathsizeAttr) {
        // The following three values of mathsize are handled in WebCore/css/mathml.css
        if (value != "normal" && value != "small" && value != "big")
            addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, value);
    } else if (name == mathcolorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    // FIXME: deprecated attributes that should loose in a conflict with a non deprecated attribute
    else if (name == fontsizeAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, value);
    else if (name == backgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == colorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    else if (name == fontstyleAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontStyle, value);
    else if (name == fontweightAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontWeight, value);
    else if (name == fontfamilyAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontFamily, value);
    else if (name == dirAttr) {
        if (hasTagName(mathTag) || hasTagName(mrowTag) || hasTagName(mstyleTag) || isMathMLToken())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, value);
    }  else {
        ASSERT(!isPresentationAttribute(name));
        StyledElement::collectStyleForPresentationAttribute(name, value
        , style);
    }
}

bool MathMLElement::childShouldCreateRenderer(const Node& child) const
{
    // In general, only MathML children are allowed. Text nodes are only visible in token MathML elements.
    return is<MathMLElement>(child);
}

bool MathMLElement::willRespondToMouseClickEvents()
{
    return isLink() || StyledElement::willRespondToMouseClickEvents();
}

void MathMLElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }
        if (MouseEvent::canTriggerActivationBehavior(event)) {
            auto& href = attributeWithoutSynchronization(hrefAttr);
            const auto& url = stripLeadingAndTrailingHTMLSpaces(href);
            event.setDefaultHandled();
            if (auto* frame = document().frame())
                frame->loader().urlSelected(document().completeURL(url), "_self", &event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate());
            return;
        }
    }

    StyledElement::defaultEventHandler(event);
}

bool MathMLElement::canStartSelection() const
{
    if (!isLink())
        return StyledElement::canStartSelection();

    return hasEditableStyle();
}

bool MathMLElement::isFocusable() const
{
    if (renderer() && renderer()->absoluteClippedOverflowRect().isEmpty())
        return false;

    return StyledElement::isFocusable();
}

bool MathMLElement::isKeyboardFocusable(KeyboardEvent& event) const
{
    if (isFocusable() && StyledElement::supportsFocus())
        return StyledElement::isKeyboardFocusable(event);

    if (isLink())
        return document().frame()->eventHandler().tabsToLinks(&event);

    return StyledElement::isKeyboardFocusable(event);
}

bool MathMLElement::isMouseFocusable() const
{
    // Links are focusable by default, but only allow links with tabindex or contenteditable to be mouse focusable.
    // https://bugs.webkit.org/show_bug.cgi?id=26856
    if (isLink())
        return StyledElement::supportsFocus();

    return StyledElement::isMouseFocusable();
}

bool MathMLElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || StyledElement::isURLAttribute(attribute);
}

bool MathMLElement::supportsFocus() const
{
    if (hasEditableStyle())
        return StyledElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || StyledElement::supportsFocus();
}

int MathMLElement::tabIndex() const
{
    // Skip the supportsFocus check in StyledElement.
    return Element::tabIndex();
}

StringView MathMLElement::stripLeadingAndTrailingWhitespace(const StringView& stringView)
{
    unsigned start = 0, stringLength = stringView.length();
    while (stringLength > 0 && isHTMLSpace(stringView[start])) {
        start++;
        stringLength--;
    }
    while (stringLength > 0 && isHTMLSpace(stringView[start + stringLength - 1]))
        stringLength--;
    return stringView.substring(start, stringLength);
}

MathMLElement::Length MathMLElement::parseNumberAndUnit(const StringView& string)
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

MathMLElement::Length MathMLElement::parseNamedSpace(const StringView& string)
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

MathMLElement::Length MathMLElement::parseMathMLLength(const String& string)
{
    // The regular expression from the MathML Relax NG schema is as follows:
    //
    //   pattern = '\s*((-?[0-9]*([0-9]\.?|\.[0-9])[0-9]*(e[mx]|in|cm|mm|p[xtc]|%)?)|(negative)?((very){0,2}thi(n|ck)|medium)mathspace)\s*'
    //
    // We do not perform a strict verification of the syntax of whitespaces and number.
    // Instead, we just use isHTMLSpace and toFloat to parse these parts.

    // We first skip whitespace from both ends of the string.
    StringView stringView = stripLeadingAndTrailingWhitespace(string);

    if (stringView.isEmpty())
        return Length();

    // We consider the most typical case: a number followed by an optional unit.
    UChar firstChar = stringView[0];
    if (isASCIIDigit(firstChar) || firstChar == '-' || firstChar == '.')
        return parseNumberAndUnit(stringView);

    // Otherwise, we try and parse a named space.
    return parseNamedSpace(stringView);
}

const MathMLElement::Length& MathMLElement::cachedMathMLLength(const QualifiedName& name, Optional<Length>& length)
{
    if (length)
        return length.value();
    length = parseMathMLLength(attributeWithoutSynchronization(name));
    return length.value();
}

const MathMLElement::BooleanValue& MathMLElement::cachedBooleanAttribute(const QualifiedName& name, Optional<BooleanValue>& attribute)
{
    if (attribute)
        return attribute.value();

    // In MathML, attribute values are case-sensitive.
    const AtomicString& value = attributeWithoutSynchronization(name);
    if (value == "true")
        attribute = BooleanValue::True;
    else if (value == "false")
        attribute = BooleanValue::False;
    else
        attribute = BooleanValue::Default;

    return attribute.value();
}

MathMLElement::MathVariant MathMLElement::parseMathVariantAttribute(const AtomicString& attributeValue)
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

Optional<bool> MathMLElement::specifiedDisplayStyle()
{
    if (!acceptsDisplayStyleAttribute())
        return Nullopt;
    const MathMLElement::BooleanValue& specifiedDisplayStyle = cachedBooleanAttribute(displaystyleAttr, m_displayStyle);
    return toOptionalBool(specifiedDisplayStyle);
}

Optional<MathMLElement::MathVariant> MathMLElement::specifiedMathVariant()
{
    if (!acceptsMathVariantAttribute())
        return Nullopt;
    if (!m_mathVariant)
        m_mathVariant = parseMathVariantAttribute(attributeWithoutSynchronization(mathvariantAttr));
    return m_mathVariant.value() == MathVariant::None ? Nullopt : m_mathVariant;
}

}

#endif // ENABLE(MATHML)
