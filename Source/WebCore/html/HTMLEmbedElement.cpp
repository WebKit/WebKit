/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2011 Apple Inc. All rights reserved.
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
#include "HTMLEmbedElement.h"

#include "Attribute.h"
#include "CSSPropertyNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "PluginDocument.h"
#include "RenderEmbeddedObject.h"
#include "RenderWidget.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include <wtf/Ref.h>

namespace WebCore {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLPlugInImageElement(tagName, document, createdByParser, ShouldPreferPlugInsForImages)
{
    ASSERT(hasTagName(embedTag));
}

PassRefPtr<HTMLEmbedElement> HTMLEmbedElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(new HTMLEmbedElement(tagName, document, createdByParser));
}

static inline RenderWidget* findWidgetRenderer(const Node* n) 
{
    if (!n->renderer())
        do
            n = n->parentNode();
        while (n && !n->hasTagName(objectTag));

    if (n && n->renderer() && n->renderer()->isWidget())
        return toRenderWidget(n->renderer());

    return 0;
}

RenderWidget* HTMLEmbedElement::renderWidgetForJSBindings() const
{
    FrameView* view = document().view();
    if (!view || (!view->isInLayout() && !view->isPainting()))
        document().updateLayoutIgnorePendingStylesheets();
    return findWidgetRenderer(this);
}

bool HTMLEmbedElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == hiddenAttr)
        return true;
    return HTMLPlugInImageElement::isPresentationAttribute(name);
}

void HTMLEmbedElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStyleProperties& style)
{
    if (name == hiddenAttr) {
        if (equalIgnoringCase(value, "yes") || equalIgnoringCase(value, "true")) {
            addPropertyToPresentationAttributeStyle(style, CSSPropertyWidth, 0, CSSPrimitiveValue::CSS_PX);
            addPropertyToPresentationAttributeStyle(style, CSSPropertyHeight, 0, CSSPrimitiveValue::CSS_PX);
        }
    } else
        HTMLPlugInImageElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLEmbedElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == typeAttr) {
        m_serviceType = value.string().left(value.find(";")).lower();
        // FIXME: The only difference between this and HTMLObjectElement's corresponding
        // code is that HTMLObjectElement does setNeedsWidgetUpdate(true). Consider moving
        // this up to the HTMLPlugInImageElement to be shared.
    } else if (name == codeAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        // FIXME: Why no call to updateImageLoaderWithNewURLSoon?
        // FIXME: If both code and src attributes are specified, last one parsed/changed wins. That can't be right!
    } else if (name == srcAttr) {
        m_url = stripLeadingAndTrailingHTMLSpaces(value);
        updateImageLoaderWithNewURLSoon();
        // FIXME: If both code and src attributes are specified, last one parsed/changed wins. That can't be right!
    } else
        HTMLPlugInImageElement::parseAttribute(name, value);
}

void HTMLEmbedElement::parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues)
{
    if (!hasAttributes())
        return;

    for (const Attribute& attribute : attributesIterator()) {
        paramNames.append(attribute.localName().string());
        paramValues.append(attribute.value().string());
    }
}

// FIXME: This should be unified with HTMLObjectElement::updateWidget and
// moved down into HTMLPluginImageElement.cpp
void HTMLEmbedElement::updateWidget(PluginCreationOption pluginCreationOption)
{
    ASSERT(!renderEmbeddedObject()->isPluginUnavailable());
    ASSERT(needsWidgetUpdate());
    setNeedsWidgetUpdate(false);

    if (m_url.isEmpty() && m_serviceType.isEmpty())
        return;

    // Note these pass m_url and m_serviceType to allow better code sharing with
    // <object> which modifies url and serviceType before calling these.
    if (!allowedToLoadFrameURL(m_url))
        return;

    // FIXME: It's sadness that we have this special case here.
    //        See http://trac.webkit.org/changeset/25128 and
    //        plugins/netscape-plugin-setwindow-size.html
    if (pluginCreationOption == CreateOnlyNonNetscapePlugins && wouldLoadAsNetscapePlugin(m_url, m_serviceType)) {
        // Ensure updateWidget() is called again during layout to create the Netscape plug-in.
        setNeedsWidgetUpdate(true);
        return;
    }

    // FIXME: These should be joined into a PluginParameters class.
    Vector<String> paramNames;
    Vector<String> paramValues;
    parametersForPlugin(paramNames, paramValues);

    Ref<HTMLEmbedElement> protect(*this); // Loading the plugin might remove us from the document.
    bool beforeLoadAllowedLoad = guardedDispatchBeforeLoadEvent(m_url);
    if (!beforeLoadAllowedLoad) {
        if (document().isPluginDocument()) {
            // Plugins inside plugin documents load differently than other plugins. By the time
            // we are here in a plugin document, the load of the plugin (which is the plugin document's
            // main resource) has already started. We need to explicitly cancel the main resource load here.
            toPluginDocument(&document())->cancelManualPluginLoad();
        }
        return;
    }
    if (!renderer()) // Do not load the plugin if beforeload removed this element or its renderer.
        return;

    // FIXME: beforeLoad could have detached the renderer!  Just like in the <object> case above.
    requestObject(m_url, m_serviceType, paramNames, paramValues);
}

bool HTMLEmbedElement::rendererIsNeeded(const RenderStyle& style)
{
    if (isImageType())
        return HTMLPlugInImageElement::rendererIsNeeded(style);

    // If my parent is an <object> and is not set to use fallback content, I
    // should be ignored and not get a renderer.
    ContainerNode* p = parentNode();
    if (p && p->hasTagName(objectTag)) {
        if (!p->renderer())
            return false;
        if (!toHTMLObjectElement(p)->useFallbackContent()) {
            ASSERT(!p->renderer()->isEmbeddedObject());
            return false;
        }
    }

#if ENABLE(DASHBOARD_SUPPORT)
    // Workaround for <rdar://problem/6642221>.
    if (document().frame()->settings().usesDashboardBackwardCompatibilityMode())
        return true;
#endif

    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

bool HTMLEmbedElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLPlugInImageElement::isURLAttribute(attribute);
}

const AtomicString& HTMLEmbedElement::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

void HTMLEmbedElement::addSubresourceAttributeURLs(ListHashSet<URL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document().completeURL(getAttribute(srcAttr)));
}

}
