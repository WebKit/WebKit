/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2026 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "HTMLBodyElement.h"

#include "CSSImageValue.h"
#include "CSSValueKeywords.h"
#include "DOMWrapperWorld.h"
#include "ElementInlines.h"
#include "EventNames.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "JSHTMLBodyElement.h"
#include "LocalDOMWindow.h"
#include "MutableStyleProperties.h"
#include "NodeName.h"
#include "ResourceLoaderOptions.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLBodyElement);

using namespace HTMLNames;

HTMLBodyElement::HTMLBodyElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(bodyTag));
}

Ref<HTMLBodyElement> HTMLBodyElement::create(Document& document)
{
    return adoptRef(*new HTMLBodyElement(bodyTag, document));
}

Ref<HTMLBodyElement> HTMLBodyElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLBodyElement(tagName, document));
}

HTMLBodyElement::~HTMLBodyElement() = default;

bool HTMLBodyElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    switch (name.nodeName()) {
    case AttributeNames::backgroundAttr:
    case AttributeNames::marginwidthAttr:
    case AttributeNames::leftmarginAttr:
    case AttributeNames::marginheightAttr:
    case AttributeNames::topmarginAttr:
    case AttributeNames::bgcolorAttr:
    case AttributeNames::textAttr:
        return true;
    default:
        break;
    }
    return HTMLElement::hasPresentationalHintsForAttribute(name);
}

void HTMLBodyElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    switch (name.nodeName()) {
    case AttributeNames::backgroundAttr: {
        auto url = value.string().trim(isASCIIWhitespace);
        if (!url.isEmpty())
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, CSSImageValue::create(protect(document())->encodingParseURL(url), localName())));
        break;
    }
    case AttributeNames::marginwidthAttr:
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginRight, value);
        break;
    case AttributeNames::leftmarginAttr:
        // https://html.spec.whatwg.org/multipage/rendering.html#the-page
        // marginwidth takes precedence over leftmargin.
        if (hasAttributeWithoutSynchronization(marginwidthAttr))
            break;
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginRight, value);
        break;
    case AttributeNames::marginheightAttr:
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginBottom, value);
        break;
    case AttributeNames::topmarginAttr:
        // https://html.spec.whatwg.org/multipage/rendering.html#the-page
        // marginheight takes precedence over topmargin.
        if (hasAttributeWithoutSynchronization(marginheightAttr))
            break;
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLPixelLengthToStyle(style, CSSPropertyMarginBottom, value);
        break;
    case AttributeNames::bgcolorAttr:
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
        break;
    case AttributeNames::textAttr:
        addHTMLColorToStyle(style, CSSPropertyColor, value);
        break;
    default:
        HTMLElement::collectPresentationalHintsForAttribute(name, value, style);
        break;
    }
}

const AtomString& HTMLBodyElement::eventNameForWindowEventHandlerAttribute(const QualifiedName& attributeName)
{
    static NeverDestroyed map = [] {
        EventHandlerNameMap map;
        JSHTMLBodyElement::forEachWindowEventHandlerContentAttribute([&] (const AtomString& attributeName, const AtomString& eventName) {
            map.add(attributeName.impl(), eventName);
        });
        return map;
    }();
    return eventNameForEventHandlerAttribute(attributeName, map);
}

void HTMLBodyElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    Ref document = this->document();
    switch (name.nodeName()) {
    case AttributeNames::vlinkAttr:
        if (auto parsedColor = parseLegacyColorValue(newValue))
            document->setVisitedLinkColor(*parsedColor);
        else
            document->resetVisitedLinkColor();
        invalidateStyleForSubtree();
        return;
    case AttributeNames::alinkAttr:
        if (auto parsedColor = parseLegacyColorValue(newValue))
            document->setActiveLinkColor(*parsedColor);
        else
            document->resetActiveLinkColor();
        invalidateStyleForSubtree();
        return;
    case AttributeNames::linkAttr:
        if (auto parsedColor = parseLegacyColorValue(newValue))
            document->setLinkColor(*parsedColor);
        else
            document->resetLinkColor();
        invalidateStyleForSubtree();
        return;
    case AttributeNames::onselectionchangeAttr:
        // FIXME: Emit "selectionchange" event at <input> / <textarea> elements and remove this special-case.
        // https://bugs.webkit.org/show_bug.cgi?id=234348
        document->setAttributeEventListener(eventNames().selectionchangeEvent, name, newValue, mainThreadNormalWorldSingleton());
        return;
    default:
        break;
    }

    if (auto& eventName = eventNameForWindowEventHandlerAttribute(name); !eventName.isNull()) {
        document->setWindowAttributeEventListener(eventName, name, newValue, mainThreadNormalWorldSingleton());
        return;
    }

    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

Node::NeedsPostConnectionSteps HTMLBodyElement::insertionSteps(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertionSteps(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return NeedsPostConnectionSteps::No;
    if (!is<HTMLFrameElementBase>(document().ownerElement()))
        return NeedsPostConnectionSteps::No;
    return NeedsPostConnectionSteps::Yes;
}

void HTMLBodyElement::postConnectionSteps()
{
    // A DOM mutation could have happened in between the call to insertionSteps() and the
    // call to postConnectionSteps().
    Ref document = this->document();
    if (!is<HTMLFrameElementBase>(document->ownerElement()))
        return;

    Ref ownerElement = *document->ownerElement();

    // https://html.spec.whatwg.org/multipage/rendering.html#the-page
    // The container frame element's marginwidth is only used when the body has
    // no marginwidth and no leftmargin. Same for marginheight vs topmargin.
    auto marginWidth = ownerElement->attributeWithoutSynchronization(marginwidthAttr);
    if (!marginWidth.isNull()
        && !hasAttributeWithoutSynchronization(marginwidthAttr)
        && !hasAttributeWithoutSynchronization(leftmarginAttr))
        setAttributeWithoutSynchronization(marginwidthAttr, marginWidth);
    auto marginHeight = ownerElement->attributeWithoutSynchronization(marginheightAttr);
    if (!marginHeight.isNull()
        && !hasAttributeWithoutSynchronization(marginheightAttr)
        && !hasAttributeWithoutSynchronization(topmarginAttr))
        setAttributeWithoutSynchronization(marginheightAttr, marginHeight);
}

bool HTMLBodyElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == backgroundAttr || HTMLElement::isURLAttribute(attribute);
}

bool HTMLBodyElement::supportsFocus() const
{
    return hasEditableStyle() || HTMLElement::supportsFocus();
}

void HTMLBodyElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, protect(document())->encodingParseURL(attributeWithoutSynchronization(backgroundAttr)));
}

} // namespace WebCore
