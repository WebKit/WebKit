/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "CSSHelper.h"
#include "CSSPropertyNames.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "MappedAttribute.h"
#include "RenderImage.h"
#include "RenderPartObject.h"
#include "RenderWidget.h"
#include "ScriptController.h"
#include "Settings.h"

namespace WebCore {

using namespace HTMLNames;

HTMLEmbedElement::HTMLEmbedElement(const QualifiedName& tagName, Document* doc)
    : HTMLPlugInImageElement(tagName, doc)
    , m_needWidgetUpdate(false)
{
    ASSERT(hasTagName(embedTag));
}

HTMLEmbedElement::~HTMLEmbedElement()
{
}

static inline RenderWidget* findWidgetRenderer(const Node* n) 
{
    if (!n->renderer())
        do
            n = n->parentNode();
        while (n && !n->hasTagName(objectTag));

    if (n && n->renderer() && n->renderer()->isWidget())
        return static_cast<RenderWidget*>(n->renderer());

    return 0;
}

RenderWidget* HTMLEmbedElement::renderWidgetForJSBindings() const
{
    RenderWidget* renderWidget = findWidgetRenderer(this);
    if (renderWidget && !renderWidget->widget()) {
        document()->updateLayoutIgnorePendingStylesheets();
        renderWidget = findWidgetRenderer(this);
    }
    return renderWidget;
}

bool HTMLEmbedElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == hiddenAttr) {
        result = eUniversal;
        return false;
    }
        
    return HTMLPlugInElement::mapToEntry(attrName, result);
}

void HTMLEmbedElement::parseMappedAttribute(MappedAttribute* attr)
{
    const AtomicString& value = attr->value();
  
    if (attr->name() == typeAttr) {
        m_serviceType = value.string().lower();
        int pos = m_serviceType.find(";");
        if (pos != -1)
            m_serviceType = m_serviceType.left(pos);
        if (!isImageType() && m_imageLoader)
            m_imageLoader.clear();
    } else if (attr->name() == codeAttr)
        m_url = parseURL(value.string());
    else if (attr->name() == srcAttr) {
        m_url = parseURL(value.string());
        if (renderer() && isImageType()) {
            if (!m_imageLoader)
                m_imageLoader.set(new HTMLImageLoader(this));
            m_imageLoader->updateFromElementIgnoringPreviousError();
        }
    } else if (attr->name() == hiddenAttr) {
        if (equalIgnoringCase(value.string(), "yes") || equalIgnoringCase(value.string(), "true")) {
            // FIXME: Not dynamic, since we add this but don't remove it, but it may be OK for now
            // that this rarely-used attribute won't work properly if you remove it.
            addCSSLength(attr, CSSPropertyWidth, "0");
            addCSSLength(attr, CSSPropertyHeight, "0");
        }
    } else if (attr->name() == nameAttr) {
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeNamedItem(m_name);
            document->addNamedItem(value);
        }
        m_name = value;
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

bool HTMLEmbedElement::rendererIsNeeded(RenderStyle* style)
{
    if (isImageType())
        return HTMLPlugInElement::rendererIsNeeded(style);

    Frame* frame = document()->frame();
    if (!frame)
        return false;

    Node* p = parentNode();
    if (p && p->hasTagName(objectTag)) {
        ASSERT(p->renderer());
        return false;
    }

#if ENABLE(DASHBOARD_SUPPORT)
    // Workaround for <rdar://problem/6642221>. 
    if (Settings* settings = frame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode())
            return true;
    }
#endif

    return HTMLPlugInElement::rendererIsNeeded(style);
}

RenderObject* HTMLEmbedElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderPartObject(this);
}

void HTMLEmbedElement::attach()
{
    m_needWidgetUpdate = true;

    bool isImage = isImageType();

    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer()) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();

        if (renderer())
            toRenderImage(renderer())->setCachedImage(m_imageLoader->image());
    }
}

void HTMLEmbedElement::updateWidget()
{
    document()->updateStyleIfNeeded();
    if (m_needWidgetUpdate && renderer() && !isImageType())
        static_cast<RenderPartObject*>(renderer())->updateWidget(true);
}

void HTMLEmbedElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->addNamedItem(m_name);

    String width = getAttribute(widthAttr);
    String height = getAttribute(heightAttr);
    if (!width.isEmpty() || !height.isEmpty()) {
        Node* n = parent();
        while (n && !n->hasTagName(objectTag))
            n = n->parent();
        if (n) {
            if (!width.isEmpty())
                static_cast<HTMLObjectElement*>(n)->setAttribute(widthAttr, width);
            if (!height.isEmpty())
                static_cast<HTMLObjectElement*>(n)->setAttribute(heightAttr, height);
        }
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLEmbedElement::removedFromDocument()
{
    if (document()->isHTMLDocument())
        static_cast<HTMLDocument*>(document())->removeNamedItem(m_name);

    HTMLPlugInElement::removedFromDocument();
}

void HTMLEmbedElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    HTMLPlugInElement::attributeChanged(attr, preserveDecls);

    if ((attr->name() == widthAttr || attr->name() == heightAttr) && !attr->isEmpty()) {
        Node* n = parent();
        while (n && !n->hasTagName(objectTag))
            n = n->parent();
        if (n)
            static_cast<HTMLObjectElement*>(n)->setAttribute(attr->name(), attr->value());
    }
}

bool HTMLEmbedElement::isURLAttribute(Attribute* attr) const
{
    return attr->name() == srcAttr;
}

const QualifiedName& HTMLEmbedElement::imageSourceAttributeName() const
{
    return srcAttr;
}

String HTMLEmbedElement::src() const
{
    return getAttribute(srcAttr);
}

void HTMLEmbedElement::setSrc(const String& value)
{
    setAttribute(srcAttr, value);
}

String HTMLEmbedElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLEmbedElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

void HTMLEmbedElement::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    HTMLPlugInImageElement::addSubresourceAttributeURLs(urls);

    addSubresourceURL(urls, document()->completeURL(src()));
}

}
