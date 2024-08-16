/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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
#include "HTMLObjectElement.h"

#include "Attribute.h"
#include "CSSValueKeywords.h"
#include "CachedImage.h"
#include "ElementChildIteratorInlines.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "LocalFrame.h"
#include "MIMETypeRegistry.h"
#include "NodeList.h"
#include "NodeName.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "Text.h"
#include "Widget.h"
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLObjectElement);

using namespace HTMLNames;

inline HTMLObjectElement::HTMLObjectElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLPlugInImageElement(tagName, document)
    , FormListedElement(form)
{
    ASSERT(hasTagName(objectTag));
}

Ref<HTMLObjectElement> HTMLObjectElement::create(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
{
    return adoptRef(*new HTMLObjectElement(tagName, document, form));
}

HTMLObjectElement::~HTMLObjectElement()
{
    clearForm();
}

int HTMLObjectElement::defaultTabIndex() const
{
    return 0;
}

bool HTMLObjectElement::hasPresentationalHintsForAttribute(const QualifiedName& name) const
{
    if (name == borderAttr)
        return true;
    return HTMLPlugInImageElement::hasPresentationalHintsForAttribute(name);
}

void HTMLObjectElement::collectPresentationalHintsForAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == borderAttr)
        applyBorderAttributeToStyle(value, style);
    else
        HTMLPlugInImageElement::collectPresentationalHintsForAttribute(name, value, style);
}

void HTMLObjectElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    HTMLPlugInImageElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    bool invalidateRenderer = false;
    bool needsWidgetUpdate = false;

    switch (name.nodeName()) {
    case AttributeNames::typeAttr:
        m_serviceType = newValue.string().left(newValue.find(';')).convertToASCIILowercase();
        invalidateRenderer = !hasAttributeWithoutSynchronization(classidAttr);
        needsWidgetUpdate = true;
        break;
    case AttributeNames::dataAttr:
        // FIXME: trimming whitespace is probably redundant with the URL parser
        m_url = newValue.string().trim(isASCIIWhitespace);
        invalidateRenderer = !hasAttributeWithoutSynchronization(classidAttr);
        needsWidgetUpdate = true;
        updateImageLoaderWithNewURLSoon();
        break;
    case AttributeNames::classidAttr:
        invalidateRenderer = true;
        needsWidgetUpdate = true;
        break;
    default:
        FormListedElement::parseAttribute(name, newValue);
        break;
    }

    if (needsWidgetUpdate) {
        setNeedsWidgetUpdate(true);
        m_useFallbackContent = false;
    }

    if (!invalidateRenderer || !isConnected() || !renderer())
        return;

    scheduleUpdateForAfterStyleResolution();
    invalidateStyleAndRenderersForSubtree();
}

static void mapDataParamToSrc(Vector<AtomString>& paramNames, Vector<AtomString>& paramValues)
{
    // Some plugins don't understand the "data" attribute of the OBJECT tag (i.e. Real and WMP require "src" attribute).
    bool foundSrcParam = false;
    AtomString dataParamValue;
    for (unsigned i = 0; i < paramNames.size(); ++i) {
        if (equalLettersIgnoringASCIICase(paramNames[i], "src"_s))
            foundSrcParam = true;
        else if (equalLettersIgnoringASCIICase(paramNames[i], "data"_s))
            dataParamValue = paramValues[i];
    }
    if (!foundSrcParam && !dataParamValue.isNull()) {
        paramNames.append(AtomString { "src"_s });
        paramValues.append(WTFMove(dataParamValue));
    }
}

void HTMLObjectElement::parametersForPlugin(Vector<AtomString>& paramNames, Vector<AtomString>& paramValues)
{
    if (hasAttributes()) {
        for (const Attribute& attribute : attributesIterator()) {
            paramNames.append(attribute.name().localName());
            paramValues.append(attribute.value());
        }
    }

    mapDataParamToSrc(paramNames, paramValues);
}

bool HTMLObjectElement::hasFallbackContent() const
{
    for (RefPtr<Node> child = firstChild(); child; child = child->nextSibling()) {
        // Ignore whitespace-only text, and <param> tags, any other content is fallback content.
        if (auto* textChild = dynamicDowncast<Text>(*child)) {
            if (!textChild->containsOnlyASCIIWhitespace())
                return true;
        } else if (!is<HTMLParamElement>(*child))
            return true;
    }
    return false;
}

// FIXME: This should be unified with HTMLEmbedElement::updateWidget and
// moved down into HTMLPluginImageElement.cpp
void HTMLObjectElement::updateWidget(CreatePlugins createPlugins)
{
    ASSERT(!renderEmbeddedObject()->isPluginUnavailable());
    ASSERT(needsWidgetUpdate());

    // FIXME: This should ASSERT isFinishedParsingChildren() instead.
    if (!isFinishedParsingChildren()) {
        setNeedsWidgetUpdate(false);
        return;
    }

    // FIXME: I'm not sure it's ever possible to get into updateWidget during a
    // removal, but just in case we should avoid loading the frame to prevent
    // security bugs.
    if (!SubframeLoadingDisabler::canLoadFrame(*this)) {
        setNeedsWidgetUpdate(false);
        return;
    }

    // FIXME: These should be joined into a PluginParameters class.
    Vector<AtomString> paramNames;
    Vector<AtomString> paramValues;
    parametersForPlugin(paramNames, paramValues);

    auto url = this->url();
    if (!canLoadURL(url)) {
        setNeedsWidgetUpdate(false);
        return;
    }

    // FIXME: It's unfortunate that we have this special case here.
    // See http://trac.webkit.org/changeset/25128 and the plugins/netscape-plugin-setwindow-size.html test.
    auto serviceType = this->serviceType();
    if (createPlugins == CreatePlugins::No && wouldLoadAsPlugIn(url, serviceType))
        return;

    setNeedsWidgetUpdate(false);

    Ref protectedThis = *this; // Plugin loading can make arbitrary DOM mutations.

    // HTML5 says that fallback content should be rendered if a non-empty
    // classid is specified for which the UA can't find a suitable plug-in.
    //
    // Dispatching a beforeLoad event could have executed code that changed the document.
    // Make sure the URL is still safe to load.
    bool success = attributeWithoutSynchronization(classidAttr).isEmpty() && canLoadURL(url);
    if (success)
        success = requestObject(url, serviceType, paramNames, paramValues);
    if (!success && hasFallbackContent())
        renderFallbackContent();
}

Node::InsertedIntoAncestorResult HTMLObjectElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLPlugInImageElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    FormListedElement::elementInsertedIntoAncestor(*this, insertionType);
    if (!insertionType.connectedToDocument)
        return InsertedIntoAncestorResult::Done;
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLObjectElement::didFinishInsertingNode()
{
    resetFormOwner();
}

void HTMLObjectElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLPlugInImageElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    FormListedElement::elementRemovedFromAncestor(*this, removalType);
}

void HTMLObjectElement::childrenChanged(const ChildChange& change)
{
    updateExposedState();
    if (isConnected() && !m_useFallbackContent) {
        setNeedsWidgetUpdate(true);
        scheduleUpdateForAfterStyleResolution();
        invalidateStyleForSubtree();
    }
    HTMLPlugInImageElement::childrenChanged(change);
}

bool HTMLObjectElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == dataAttr || attribute.name() == codebaseAttr || HTMLPlugInImageElement::isURLAttribute(attribute);
}

const AtomString& HTMLObjectElement::imageSourceURL() const
{
    return attributeWithoutSynchronization(dataAttr);
}

void HTMLObjectElement::renderFallbackContent()
{
    if (m_useFallbackContent)
        return;

    if (!isConnected())
        return;

    scheduleUpdateForAfterStyleResolution();
    invalidateStyleAndRenderersForSubtree();

    // Before we give up and use fallback content, check to see if this is a MIME type issue.
    auto* loader = imageLoader();
    if (loader && loader->image() && loader->image()->status() != CachedResource::LoadError) {
        m_serviceType = loader->image()->response().mimeType();
        if (!isImageType()) {
            // If we don't think we have an image type anymore, then clear the image from the loader.
            loader->clearImage();
            return;
        }
    }

    m_useFallbackContent = true;
}

static inline bool preventsParentObjectFromExposure(const Element& child)
{
    static NeverDestroyed mostKnownTags = [] {
        MemoryCompactLookupOnlyRobinHoodHashSet<QualifiedName> set;
        auto* tags = HTMLNames::getHTMLTags();
        set.reserveInitialCapacity(HTMLNames::HTMLTagsCount);
        for (size_t i = 0; i < HTMLNames::HTMLTagsCount; i++) {
            auto& tag = *tags[i];
            // Only the param element was explicitly mentioned in the HTML specification rule
            // we were trying to implement, but these are other known HTML elements that we
            // have decided, over the years, to treat as children that do not prevent object
            // names from being exposed.
            if (tag == bgsoundTag
                || tag == detailsTag
                || tag == figcaptionTag
                || tag == figureTag
                || tag == paramTag
                || tag == summaryTag
                || tag == trackTag)
                continue;
            set.add(tag);
        }
        return set;
    }();
    return mostKnownTags.get().contains(child.tagQName());
}

static inline bool preventsParentObjectFromExposure(const Node& child)
{
    if (auto* childElement = dynamicDowncast<Element>(child))
        return preventsParentObjectFromExposure(*childElement);
    if (auto* childText = dynamicDowncast<Text>(child))
        return !childText->containsOnlyASCIIWhitespace();
    return true;
}

static inline bool shouldBeExposed(const HTMLObjectElement& element)
{
    // FIXME: This should be redone to use the concept of an exposed object element,
    // as documented in the HTML specification section describing DOM tree accessors.

    // The rule we try to implement here, from older HTML specifications, is "object elements
    // with no children other than param elements, unknown elements and whitespace can be found
    // by name in a document, and other object elements cannot".

    for (RefPtr child = element.firstChild(); child; child = child->nextSibling()) {
        if (preventsParentObjectFromExposure(*child))
            return false;
    }
    return true;
}

void HTMLObjectElement::updateExposedState()
{
    bool wasExposed = std::exchange(m_isExposed, shouldBeExposed(*this));

    if (m_isExposed != wasExposed && isConnected() && !isInShadowTree()) {
        if (RefPtr document = dynamicDowncast<HTMLDocument>(this->document())) {
            auto& id = getIdAttribute();
            if (!id.isEmpty()) {
                if (m_isExposed)
                    document->addDocumentNamedItem(id, *this);
                else
                    document->removeDocumentNamedItem(id, *this);
            }

            auto& name = getNameAttribute();
            if (!name.isEmpty() && id != name) {
                if (m_isExposed)
                    document->addDocumentNamedItem(name, *this);
                else
                    document->removeDocumentNamedItem(name, *this);
            }
        }
    }
}

void HTMLObjectElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(dataAttr)));
}

void HTMLObjectElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    FormListedElement::didMoveToNewDocument();
    HTMLPlugInImageElement::didMoveToNewDocument(oldDocument, newDocument);
}

bool HTMLObjectElement::canContainRangeEndPoint() const
{
    // Call through to HTMLElement because HTMLPlugInElement::canContainRangeEndPoint
    // returns false unconditionally. An object element using fallback content is
    // treated like a generic HTML element.
    return m_useFallbackContent && HTMLElement::canContainRangeEndPoint();
}

}
