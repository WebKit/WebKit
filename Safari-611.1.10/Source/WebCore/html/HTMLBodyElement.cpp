/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
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
#include "CSSParser.h"
#include "CSSValueKeywords.h"
#include "DOMWindow.h"
#include "DOMWrapperWorld.h"
#include "EventNames.h"
#include "HTMLFrameElement.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLBodyElement);

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

bool HTMLBodyElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == marginwidthAttr || name == leftmarginAttr || name == marginheightAttr || name == topmarginAttr || name == bgcolorAttr || name == textAttr || name == bgpropertiesAttr)
        return true;
    return HTMLElement::isPresentationAttribute(name);
}

void HTMLBodyElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == backgroundAttr) {
        String url = stripLeadingAndTrailingHTMLSpaces(value);
        if (!url.isEmpty()) {
            auto imageValue = CSSImageValue::create(document().completeURL(url), LoadedFromOpaqueSource::No);
            imageValue.get().setInitiator(localName());
            style.setProperty(CSSProperty(CSSPropertyBackgroundImage, WTFMove(imageValue)));
        }
    } else if (name == marginwidthAttr || name == leftmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    } else if (name == marginheightAttr || name == topmarginAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    } else if (name == bgcolorAttr) {
        addHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
    } else if (name == textAttr) {
        addHTMLColorToStyle(style, CSSPropertyColor, value);
    } else if (name == bgpropertiesAttr) {
        if (equalLettersIgnoringASCIICase(value, "fixed"))
           addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundAttachment, CSSValueFixed);
    } else
        HTMLElement::collectStyleForPresentationAttribute(name, value, style);
}

HTMLElement::EventHandlerNameMap HTMLBodyElement::createWindowEventHandlerNameMap()
{
    static const QualifiedName* const table[] = {
        &onafterprintAttr.get(),
        &onbeforeprintAttr.get(),
        &onbeforeunloadAttr.get(),
        &onblurAttr.get(),
        &onerrorAttr.get(),
        &onfocusAttr.get(),
        &onfocusinAttr.get(),
        &onfocusoutAttr.get(),
        &onhashchangeAttr.get(),
        &onlanguagechangeAttr.get(),
        &onloadAttr.get(),
        &onmessageAttr.get(),
        &onofflineAttr.get(),
        &ononlineAttr.get(),
        &onorientationchangeAttr.get(),
        &onpagehideAttr.get(),
        &onpageshowAttr.get(),
        &onpopstateAttr.get(),
        &onresizeAttr.get(),
        &onscrollAttr.get(),
        &onstorageAttr.get(),
        &onunloadAttr.get(),
        &onwebkitmouseforcechangedAttr.get(),
        &onwebkitmouseforcedownAttr.get(),
        &onwebkitmouseforceupAttr.get(),
        &onwebkitmouseforcewillbeginAttr.get(),
        &onwebkitwillrevealbottomAttr.get(),
        &onwebkitwillrevealleftAttr.get(),
        &onwebkitwillrevealrightAttr.get(),
        &onwebkitwillrevealtopAttr.get(),
    };

    EventHandlerNameMap map;
    populateEventHandlerNameMap(map, table);
    return map;
}

const AtomString& HTMLBodyElement::eventNameForWindowEventHandlerAttribute(const QualifiedName& attributeName)
{
    static NeverDestroyed<EventHandlerNameMap> map = createWindowEventHandlerNameMap();
    return eventNameForEventHandlerAttribute(attributeName, map.get());
}

void HTMLBodyElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == vlinkAttr || name == alinkAttr || name == linkAttr) {
        if (value.isNull()) {
            if (name == linkAttr)
                document().resetLinkColor();
            else if (name == vlinkAttr)
                document().resetVisitedLinkColor();
            else
                document().resetActiveLinkColor();
        } else {
            Color color = CSSParser::parseColor(value, !document().inQuirksMode());
            if (color.isValid()) {
                if (name == linkAttr)
                    document().setLinkColor(color);
                else if (name == vlinkAttr)
                    document().setVisitedLinkColor(color);
                else
                    document().setActiveLinkColor(color);
            }
        }

        invalidateStyleForSubtree();
        return;
    }

    if (name == onselectionchangeAttr) {
        document().setAttributeEventListener(eventNames().selectionchangeEvent, name, value, mainThreadNormalWorld());
        return;
    }

    auto& eventName = eventNameForWindowEventHandlerAttribute(name);
    if (!eventName.isNull()) {
        document().setWindowAttributeEventListener(eventName, name, value, mainThreadNormalWorld());
        return;
    }

    HTMLElement::parseAttribute(name, value);
}

Node::InsertedIntoAncestorResult HTMLBodyElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    if (!is<HTMLFrameElementBase>(document().ownerElement()))
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLBodyElement::didFinishInsertingNode()
{
    ASSERT(is<HTMLFrameElementBase>(document().ownerElement()));
    auto ownerElement = makeRef(*document().ownerElement());

    // FIXME: It's surprising this is web compatible since it means marginwidth and marginheight attributes
    // appear or get overwritten on body elements of a document embedded through <iframe> or <frame>.
    // Better to find a way to do addHTMLLengthToStyle based on the attributes from the frame element,
    // without modifying the body element's attributes. Could also add code so we can respond to updates
    // to the frame element attributes.
    auto marginWidth = ownerElement->attributeWithoutSynchronization(marginwidthAttr);
    if (!marginWidth.isNull())
        setAttributeWithoutSynchronization(marginwidthAttr, marginWidth);
    auto marginHeight = ownerElement->attributeWithoutSynchronization(marginheightAttr);
    if (!marginHeight.isNull())
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

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(backgroundAttr)));
}

} // namespace WebCore
