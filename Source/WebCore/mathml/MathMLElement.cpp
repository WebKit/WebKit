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

#include "ElementInlines.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "HTMLAnchorElement.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLTableCellElement.h"
#include "MathMLNames.h"
#include "MouseEvent.h"
#include "NodeName.h"
#include "RenderTableCell.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(MathMLElement);

using namespace MathMLNames;

MathMLElement::MathMLElement(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> typeFlags)
    : StyledElement(tagName, document, typeFlags | TypeFlag::IsMathMLElement)
{
}

Ref<MathMLElement> MathMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLElement(tagName, document));
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
    return std::max(1u, std::min(limitToOnlyHTMLNonNegative(rowSpanValue, 1u), HTMLTableCellElement::maxRowspan));
}

void MathMLElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::hrefAttr:
        setIsLink(!newValue.isNull() && !shouldProhibitLinks(this));
        break;
    case AttributeNames::columnspanAttr:
    case AttributeNames::rowspanAttr:
        if (is<RenderTableCell>(renderer()) && hasTagName(mtdTag))
            downcast<RenderTableCell>(*renderer()).colSpanOrRowSpanChanged();
        break;
    case AttributeNames::tabindexAttr:
        if (newValue.isEmpty())
            setTabIndexExplicitly(std::nullopt);
        else if (auto optionalTabIndex = parseHTMLInteger(newValue))
            setTabIndexExplicitly(optionalTabIndex.value());
        break;
    default:
        if (auto& eventName = HTMLElement::eventNameForEventHandlerAttribute(name); !eventName.isNull()) {
            setAttributeEventListener(eventName, name, newValue);
            return;
        }
        StyledElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

bool MathMLElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::backgroundAttr:
    case AttributeNames::colorAttr:
    case AttributeNames::dirAttr:
    case AttributeNames::fontfamilyAttr:
    case AttributeNames::fontsizeAttr:
    case AttributeNames::fontstyleAttr:
    case AttributeNames::fontweightAttr:
    case AttributeNames::mathbackgroundAttr:
    case AttributeNames::mathcolorAttr:
    case AttributeNames::mathsizeAttr:
    case AttributeNames::displaystyleAttr:
        return true;
    default:
        break;
    }
    return StyledElement::hasPresentationalHintsForAttribute(name);
}

static inline bool isDisallowedMathSizeAttribute(const AtomString& value)
{
    // FIXME(https://webkit.org/b/245927): The CSS parser sometimes accept non-zero <number> font-size values on MathML elements, so explicitly disallow them.
    bool ok;
    value.toDouble(&ok);
    if (ok && value != "0"_s)
        return true;

    // Keywords from CSS font-size are disallowed.
    return equalIgnoringASCIICase(value, "medium"_s)
        || value.endsWithIgnoringASCIICase("large"_s)
        || value.endsWithIgnoringASCIICase("small"_s)
        || equalIgnoringASCIICase(value, "smaller"_s)
        || equalIgnoringASCIICase(value, "larger"_s)
        || equalIgnoringASCIICase(value, "math"_s);
}

static String convertMathSizeIfNeeded(const AtomString& value)
{
    if (value == "small"_s)
        return "0.75em"_s;
    if (value == "normal"_s)
        return "1em"_s;
    if (value == "big"_s)
        return "1.5em"_s;

    // FIXME: mathsize accepts any MathML length, including named spaces (see parseMathMLLength).
    // FIXME: Might be better to use double than float.
    // FIXME: Might be better to use "shortest" numeric formatting instead of fixed width.
    bool ok = false;
    float unitlessValue = value.toFloat(&ok);
    if (!ok)
        return value;
    return makeString(FormattedNumber::fixedWidth(unitlessValue * 100, 3), '%');
}

void MathMLElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::mathbackgroundAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyBackgroundColor, value);
        return;
    case AttributeNames::mathsizeAttr:
        if (document().settings().coreMathMLEnabled()) {
            if (!isDisallowedMathSizeAttribute(value))
                addPropertyToPresentationalHintStyle(style, CSSPropertyFontSize, value);
        } else
            addPropertyToPresentationalHintStyle(style, CSSPropertyFontSize, convertMathSizeIfNeeded(value));
        return;
    case AttributeNames::mathcolorAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyColor, value);
        return;
    case AttributeNames::dirAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyDirection, value);
        return;
    case AttributeNames::displaystyleAttr:
        if (equalLettersIgnoringASCIICase(value, "false"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyMathStyle, CSSValueCompact);
        else if (equalLettersIgnoringASCIICase(value, "true"_s))
            addPropertyToPresentationalHintStyle(style, CSSPropertyMathStyle, CSSValueNormal);
        return;
    default:
        break;
    }

    if (document().settings().coreMathMLEnabled()) {
        StyledElement::collectPresentationalHintsForAttribute(name, value, style);
        return;
    }

    // FIXME: The following are deprecated attributes that should lose if there is a conflict with a non-deprecated attribute.
    switch (name.nodeName()) {
    case AttributeNames::fontsizeAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyFontSize, value);
        break;
    case AttributeNames::backgroundAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyBackgroundColor, value);
        break;
    case AttributeNames::colorAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyColor, value);
        break;
    case AttributeNames::fontstyleAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyFontStyle, value);
        break;
    case AttributeNames::fontweightAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyFontWeight, value);
        break;
    case AttributeNames::fontfamilyAttr:
        addPropertyToPresentationalHintStyle(style, CSSPropertyFontFamily, value);
        break;
    default:
        ASSERT(!hasPresentationalHintsForAttribute(name));
        StyledElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

bool MathMLElement::childShouldCreateRenderer(const Node& child) const
{
    // In general, only MathML children are allowed. Text nodes are only visible in token MathML elements.
    return is<MathMLElement>(child);
}

bool MathMLElement::willRespondToMouseClickEventsWithEditability(Editability editability) const
{
    return isLink() || StyledElement::willRespondToMouseClickEventsWithEditability(editability);
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
            const auto& href = attributeWithoutSynchronization(hrefAttr);
            event.setDefaultHandled();
            if (RefPtr frame = document().frame())
                frame->checkedLoader()->changeLocation(document().completeURL(href), selfTargetFrameName(), &event, ReferrerPolicy::EmptyString, document().shouldOpenExternalURLsPolicyToPropagate());
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

bool MathMLElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isFocusable() && StyledElement::supportsFocus())
        return StyledElement::isKeyboardFocusable(event);

    if (isLink())
        return document().frame()->eventHandler().tabsToLinks(event);

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

}

#endif // ENABLE(MATHML)
