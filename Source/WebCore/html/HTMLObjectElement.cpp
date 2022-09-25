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
#include "DOMFormData.h"
#include "ElementIterator.h"
#include "ElementName.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLMetaElement.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "HTMLParserIdioms.h"
#include "MIMETypeRegistry.h"
#include "NodeList.h"
#include "Page.h"
#include "RenderEmbeddedObject.h"
#include "RenderImage.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "Text.h"
#include "Widget.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>
#include <wtf/RobinHoodHashSet.h>

#if PLATFORM(IOS_FAMILY)
#include "RuntimeApplicationChecks.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLObjectElement);

using namespace HTMLNames;

inline HTMLObjectElement::HTMLObjectElement(const QualifiedName& tagName, Document& document, HTMLFormElement* form)
    : HTMLPlugInImageElement(tagName, document)
    , FormAssociatedElement(form)
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

void HTMLObjectElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    bool invalidateRenderer = false;
    bool needsWidgetUpdate = false;

    if (name == formAttr)
        formAttributeChanged();
    else if (name == typeAttr) {
        m_serviceType = value.string().left(value.find(';')).convertToASCIILowercase();
        invalidateRenderer = !hasAttributeWithoutSynchronization(classidAttr);
        needsWidgetUpdate = true;
    } else if (name == dataAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        invalidateRenderer = !hasAttributeWithoutSynchronization(classidAttr);
        needsWidgetUpdate = true;
        updateImageLoaderWithNewURLSoon();
    } else if (name == classidAttr) {
        invalidateRenderer = true;
        needsWidgetUpdate = true;
    } else
        HTMLPlugInImageElement::parseAttribute(name, value);

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

// FIXME: This function should not deal with url or serviceType!
void HTMLObjectElement::parametersForPlugin(Vector<AtomString>& paramNames, Vector<AtomString>& paramValues, String& url, String& serviceType)
{
    HashSet<StringImpl*, ASCIICaseInsensitiveHash> uniqueParamNames;
    String urlParameter;
    
    // Scan the PARAM children and store their name/value pairs.
    // Get the URL and type from the params if we don't already have them.
    for (auto& param : childrenOfType<HTMLParamElement>(*this)) {
        String name = param.name();
        if (name.isEmpty())
            continue;

        uniqueParamNames.add(name.impl());
        paramNames.append(param.name());
        paramValues.append(param.value());

        // FIXME: url adjustment does not belong in this function.
        if (url.isEmpty() && urlParameter.isEmpty() && (equalLettersIgnoringASCIICase(name, "src"_s) || equalLettersIgnoringASCIICase(name, "movie"_s) || equalLettersIgnoringASCIICase(name, "code"_s) || equalLettersIgnoringASCIICase(name, "url"_s)))
            urlParameter = stripLeadingAndTrailingHTMLSpaces(param.value());
        // FIXME: serviceType calculation does not belong in this function.
        if (serviceType.isEmpty() && equalLettersIgnoringASCIICase(name, "type"_s)) {
            serviceType = param.value();
            size_t pos = serviceType.find(';');
            if (pos != notFound)
                serviceType = serviceType.left(pos);
        }
    }
    
    // When OBJECT is used for an applet via Sun's Java plugin, the CODEBASE attribute in the tag
    // points to the Java plugin itself (an ActiveX component) while the actual applet CODEBASE is
    // in a PARAM tag. See <http://java.sun.com/products/plugin/1.2/docs/tags.html>. This means
    // we have to explicitly suppress the tag's CODEBASE attribute if there is none in a PARAM,
    // else our Java plugin will misinterpret it. [4004531]
    String codebase;
    if (MIMETypeRegistry::isJavaAppletMIMEType(serviceType)) {
        codebase = "codebase"_s;
        uniqueParamNames.add(codebase.impl()); // pretend we found it in a PARAM already
    }
    
    // Turn the attributes of the <object> element into arrays, but don't override <param> values.
    if (hasAttributes()) {
        for (const Attribute& attribute : attributesIterator()) {
            const AtomString& name = attribute.name().localName();
            if (!uniqueParamNames.contains(name.impl())) {
                paramNames.append(name);
                paramValues.append(attribute.value());
            }
        }
    }
    
    mapDataParamToSrc(paramNames, paramValues);
    
    // HTML5 says that an object resource's URL is specified by the object's data
    // attribute, not by a param element. However, for compatibility, allow the
    // resource's URL to be given by a param named "src", "movie", "code" or "url"
    // if we know that resource points to a plug-in.

    if (url.isEmpty() && !urlParameter.isEmpty()) {
        auto& loader = document().frame()->loader().subframeLoader();
        if (loader.resourceWillUsePlugin(urlParameter, serviceType))
            url = urlParameter;
    }
}

bool HTMLObjectElement::hasFallbackContent() const
{
    for (RefPtr<Node> child = firstChild(); child; child = child->nextSibling()) {
        // Ignore whitespace-only text, and <param> tags, any other content is fallback content.
        if (is<Text>(*child)) {
            if (!downcast<Text>(*child).data().isAllSpecialCharacters<isHTMLSpace>())
                return true;
        } else if (!is<HTMLParamElement>(*child))
            return true;
    }
    return false;
}

bool HTMLObjectElement::hasValidClassId()
{
    if (MIMETypeRegistry::isJavaAppletMIMEType(serviceType()) && protocolIs(attributeWithoutSynchronization(classidAttr), "java"_s))
        return true;

    // HTML5 says that fallback content should be rendered if a non-empty
    // classid is specified for which the UA can't find a suitable plug-in.
    return attributeWithoutSynchronization(classidAttr).isEmpty();
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

    String url = this->url();
    String serviceType = this->serviceType();

    // FIXME: These should be joined into a PluginParameters class.
    Vector<AtomString> paramNames;
    Vector<AtomString> paramValues;
    parametersForPlugin(paramNames, paramValues, url, serviceType);

    // Note: url is modified above by parametersForPlugin.
    if (!canLoadURL(url)) {
        setNeedsWidgetUpdate(false);
        return;
    }

    // FIXME: It's unfortunate that we have this special case here.
    // See http://trac.webkit.org/changeset/25128 and the plugins/netscape-plugin-setwindow-size.html test.
    if (createPlugins == CreatePlugins::No && wouldLoadAsPlugIn(url, serviceType))
        return;

    setNeedsWidgetUpdate(false);

    Ref protectedThis = *this; // Plugin loading can make arbitrary DOM mutations.

    // Dispatching a beforeLoad event could have executed code that changed the document.
    // Make sure the URL is still safe to load.
    bool success = hasValidClassId() && canLoadURL(url);
    if (success)
        success = requestObject(url, serviceType, paramNames, paramValues);
    if (!success && hasFallbackContent())
        renderFallbackContent();
}

Node::InsertedIntoAncestorResult HTMLObjectElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLPlugInImageElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    FormAssociatedElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    return InsertedIntoAncestorResult::NeedsPostInsertionCallback;
}

void HTMLObjectElement::didFinishInsertingNode()
{
    resetFormOwner();
}

void HTMLObjectElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLPlugInImageElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    FormAssociatedElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
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
                || tag == commandTag
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
    if (is<Element>(child))
        return preventsParentObjectFromExposure(downcast<Element>(child));
    if (is<Text>(child))
        return !downcast<Text>(child).data().isAllSpecialCharacters<isHTMLSpace>();
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

    if (m_isExposed != wasExposed && isConnected() && !isInShadowTree() && is<HTMLDocument>(document())) {
        auto& document = downcast<HTMLDocument>(this->document());

        auto& id = getIdAttribute();
        if (!id.isEmpty()) {
            if (m_isExposed)
                document.addDocumentNamedItem(*id.impl(), *this);
            else
                document.removeDocumentNamedItem(*id.impl(), *this);
        }

        auto& name = getNameAttribute();
        if (!name.isEmpty() && id != name) {
            if (m_isExposed)
                document.addDocumentNamedItem(*name.impl(), *this);
            else
                document.removeDocumentNamedItem(*name.impl(), *this);
        }
    }
}

bool HTMLObjectElement::containsJavaApplet() const
{
    if (MIMETypeRegistry::isJavaAppletMIMEType(attributeWithoutSynchronization(typeAttr)))
        return true;

    for (auto& child : childrenOfType<Element>(*this)) {
        if (child.hasTagName(paramTag) && equalLettersIgnoringASCIICase(child.getNameAttribute(), "type"_s)
            && MIMETypeRegistry::isJavaAppletMIMEType(child.attributeWithoutSynchronization(valueAttr).string()))
            return true;
        if (child.hasTagName(objectTag) && downcast<HTMLObjectElement>(child).containsJavaApplet())
            return true;
        if (child.hasTagName(appletTag))
            return true;
    }
    
    return false;
}

void HTMLObjectElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(attributeWithoutSynchronization(dataAttr)));
}

void HTMLObjectElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    FormAssociatedElement::didMoveToNewDocument(oldDocument);
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
