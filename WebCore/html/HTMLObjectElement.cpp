/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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

#include "CSSHelper.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameView.h"
#include "HTMLDocument.h"
#include "HTMLFormElement.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "Image.h"
#include "MIMETypeRegistry.h"
#include "RenderImage.h"
#include "RenderPartObject.h"
#include "RenderWidget.h"
#include "Text.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLObjectElement::HTMLObjectElement(Document* doc, bool createdByParser) 
    : HTMLPlugInElement(objectTag, doc)
    , m_needWidgetUpdate(!createdByParser)
    , m_useFallbackContent(false)
    , m_docNamedItem(true)
{
}

HTMLObjectElement::~HTMLObjectElement()
{
#if USE(JAVASCRIPTCORE_BINDINGS)
    // m_instance should have been cleaned up in detach().
    ASSERT(!m_instance);
#endif
}

#if USE(JAVASCRIPTCORE_BINDINGS)
KJS::Bindings::Instance *HTMLObjectElement::getInstance() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    if (m_instance)
        return m_instance.get();

    RenderWidget* renderWidget = (renderer() && renderer()->isWidget()) ? static_cast<RenderWidget*>(renderer()) : 0;
    if (renderWidget && !renderWidget->widget()) {
        document()->updateLayoutIgnorePendingStylesheets();
        renderWidget = (renderer() && renderer()->isWidget()) ? static_cast<RenderWidget*>(renderer()) : 0;
    }          
    if (renderWidget && renderWidget->widget()) 
        m_instance = frame->createScriptInstanceForWidget(renderWidget->widget());

    return m_instance.get();
}
#endif

void HTMLObjectElement::parseMappedAttribute(MappedAttribute *attr)
{
    String val = attr->value();
    int pos;
    if (attr->name() == typeAttr) {
        m_serviceType = val.lower();
        pos = m_serviceType.find(";");
        if (pos != -1)
          m_serviceType = m_serviceType.left(pos);
        if (renderer())
          m_needWidgetUpdate = true;
        if (!isImageType() && m_imageLoader)
          m_imageLoader.clear();
    } else if (attr->name() == dataAttr) {
        m_url = parseURL(val);
        if (renderer())
          m_needWidgetUpdate = true;
        if (renderer() && isImageType()) {
          if (!m_imageLoader)
              m_imageLoader.set(new HTMLImageLoader(this));
          m_imageLoader->updateFromElement();
        }
    } else if (attr->name() == classidAttr) {
        m_classId = val;
        if (renderer())
          m_needWidgetUpdate = true;
    } else if (attr->name() == onloadAttr) {
        setHTMLEventListener(loadEvent, attr);
    } else if (attr->name() == onunloadAttr) {
        setHTMLEventListener(unloadEvent, attr);
    } else if (attr->name() == nameAttr) {
            String newNameAttr = attr->value();
            if (isDocNamedItem() && inDocument() && document()->isHTMLDocument()) {
                HTMLDocument *doc = static_cast<HTMLDocument *>(document());
                doc->removeNamedItem(oldNameAttr);
                doc->addNamedItem(newNameAttr);
            }
            oldNameAttr = newNameAttr;
    } else if (attr->name() == idAttr) {
        String newIdAttr = attr->value();
        if (isDocNamedItem() && inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* doc = static_cast<HTMLDocument*>(document());
            doc->removeDocExtraNamedItem(oldIdAttr);
            doc->addDocExtraNamedItem(newIdAttr);
        }
        oldIdAttr = newIdAttr;
        // also call superclass
        HTMLPlugInElement::parseMappedAttribute(attr);
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

bool HTMLObjectElement::rendererIsNeeded(RenderStyle* style)
{
    if (m_useFallbackContent || isImageType())
        return HTMLPlugInElement::rendererIsNeeded(style);

    Frame* frame = document()->frame();
    if (!frame)
        return false;
    
    return true;
}

RenderObject *HTMLObjectElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    if (m_useFallbackContent)
        return RenderObject::createObject(this, style);
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderPartObject(this);
}

void HTMLObjectElement::attach()
{
    bool isImage = isImageType();

    if (!isImage)
        queuePostAttachCallback(&HTMLPlugInElement::updateWidgetCallback, this);

    HTMLPlugInElement::attach();

    if (isImage && renderer() && !m_useFallbackContent) {
        if (!m_imageLoader)
            m_imageLoader.set(new HTMLImageLoader(this));
        m_imageLoader->updateFromElement();
        if (renderer()) {
            RenderImage* imageObj = static_cast<RenderImage*>(renderer());
            imageObj->setCachedImage(m_imageLoader->image());
        }
    }
}

void HTMLObjectElement::updateWidget()
{
    if (m_needWidgetUpdate && renderer() && !m_useFallbackContent && !isImageType())
        static_cast<RenderPartObject*>(renderer())->updateWidget(true);
}

void HTMLObjectElement::finishParsingChildren()
{
    HTMLPlugInElement::finishParsingChildren();
    if (!m_useFallbackContent) {
        m_needWidgetUpdate = true;
        if (inDocument())
            setChanged();
    }
}

void HTMLObjectElement::detach()
{
    if (attached() && renderer() && !m_useFallbackContent) {
        // Update the widget the next time we attach (detaching destroys the plugin).
        m_needWidgetUpdate = true;
    }

#if USE(JAVASCRIPTCORE_BINDINGS)
    m_instance = 0;
#endif
    HTMLPlugInElement::detach();
}

void HTMLObjectElement::insertedIntoDocument()
{
    if (isDocNamedItem() && document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->addNamedItem(oldNameAttr);
        doc->addDocExtraNamedItem(oldIdAttr);
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLObjectElement::removedFromDocument()
{
    if (isDocNamedItem() && document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->removeNamedItem(oldNameAttr);
        doc->removeDocExtraNamedItem(oldIdAttr);
    }

    HTMLPlugInElement::removedFromDocument();
}

void HTMLObjectElement::recalcStyle(StyleChange ch)
{
    if (!m_useFallbackContent && m_needWidgetUpdate && renderer() && !isImageType()) {
        detach();
        attach();
    }
    HTMLPlugInElement::recalcStyle(ch);
}

void HTMLObjectElement::childrenChanged(bool changedByParser)
{
    updateDocNamedItem();
    if (inDocument() && !m_useFallbackContent) {
        m_needWidgetUpdate = true;
        setChanged();
    }
    HTMLPlugInElement::childrenChanged(changedByParser);
}

bool HTMLObjectElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == dataAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#'));
}

const QualifiedName& HTMLObjectElement::imageSourceAttributeName() const
{
    return dataAttr;
}

bool HTMLObjectElement::isImageType()
{
    if (m_serviceType.isEmpty() && m_url.startsWith("data:")) {
        // Extract the MIME type from the data URL.
        int index = m_url.find(';');
        if (index == -1)
            index = m_url.find(',');
        if (index != -1) {
            int len = index - 5;
            if (len > 0)
                m_serviceType = m_url.substring(5, len);
            else
                m_serviceType = "text/plain"; // Data URLs with no MIME type are considered text/plain.
        }
    }
    if (Frame* frame = document()->frame()) {
        KURL completedURL(frame->loader()->completeURL(m_url));
        return frame->loader()->client()->objectContentType(completedURL, m_serviceType) == ObjectContentImage;
    }

    return Image::supportsType(m_serviceType);
}

void HTMLObjectElement::renderFallbackContent()
{
    if (m_useFallbackContent)
        return;

    // Mark ourselves as using the fallback content.
    m_useFallbackContent = true;

    // Now do a detach and reattach.    
    // FIXME: Style gets recalculated which is suboptimal.
    detach();
    attach();
}

void HTMLObjectElement::updateDocNamedItem()
{
    // The rule is "<object> elements with no children other than
    // <param> elements, unknown elements and whitespace can be
    // found by name in a document, and other <object> elements cannot."
    bool wasNamedItem = m_docNamedItem;
    bool isNamedItem = true;
    Node* child = firstChild();
    while (child && isNamedItem) {
        if (child->isElementNode()) {
            Element* element = static_cast<Element*>(child);
            if (HTMLElement::isRecognizedTagName(element->tagQName()) && !element->hasTagName(paramTag))
                isNamedItem = false;
        } else if (child->isTextNode()) {
            if (!static_cast<Text*>(child)->containsOnlyWhitespace())
                isNamedItem = false;
        } else
            isNamedItem = false;
        child = child->nextSibling();
    }
    if (isNamedItem != wasNamedItem && document()->isHTMLDocument()) {
        HTMLDocument* doc = static_cast<HTMLDocument*>(document());
        if (isNamedItem) {
            doc->addNamedItem(oldNameAttr);
            doc->addDocExtraNamedItem(oldIdAttr);
        } else {
            doc->removeNamedItem(oldNameAttr);
            doc->removeDocExtraNamedItem(oldIdAttr);
        }
    }
    m_docNamedItem = isNamedItem;
}

String HTMLObjectElement::code() const
{
    return getAttribute(codeAttr);
}

void HTMLObjectElement::setCode(const String& value)
{
    setAttribute(codeAttr, value);
}

String HTMLObjectElement::archive() const
{
    return getAttribute(archiveAttr);
}

void HTMLObjectElement::setArchive(const String& value)
{
    setAttribute(archiveAttr, value);
}

String HTMLObjectElement::border() const
{
    return getAttribute(borderAttr);
}

void HTMLObjectElement::setBorder(const String& value)
{
    setAttribute(borderAttr, value);
}

String HTMLObjectElement::codeBase() const
{
    return getAttribute(codebaseAttr);
}

void HTMLObjectElement::setCodeBase(const String& value)
{
    setAttribute(codebaseAttr, value);
}

String HTMLObjectElement::codeType() const
{
    return getAttribute(codetypeAttr);
}

void HTMLObjectElement::setCodeType(const String& value)
{
    setAttribute(codetypeAttr, value);
}

String HTMLObjectElement::data() const
{
    return document()->completeURL(getAttribute(dataAttr));
}

void HTMLObjectElement::setData(const String& value)
{
    setAttribute(dataAttr, value);
}

bool HTMLObjectElement::declare() const
{
    return !getAttribute(declareAttr).isNull();
}

void HTMLObjectElement::setDeclare(bool declare)
{
    setAttribute(declareAttr, declare ? "" : 0);
}

int HTMLObjectElement::hspace() const
{
    return getAttribute(hspaceAttr).toInt();
}

void HTMLObjectElement::setHspace(int value)
{
    setAttribute(hspaceAttr, String::number(value));
}

String HTMLObjectElement::standby() const
{
    return getAttribute(standbyAttr);
}

void HTMLObjectElement::setStandby(const String& value)
{
    setAttribute(standbyAttr, value);
}

void HTMLObjectElement::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, String::number(tabIndex));
}

String HTMLObjectElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLObjectElement::setType(const String& value)
{
    setAttribute(typeAttr, value);
}

String HTMLObjectElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLObjectElement::setUseMap(const String& value)
{
    setAttribute(usemapAttr, value);
}

int HTMLObjectElement::vspace() const
{
    return getAttribute(vspaceAttr).toInt();
}

void HTMLObjectElement::setVspace(int value)
{
    setAttribute(vspaceAttr, String::number(value));
}

bool HTMLObjectElement::containsJavaApplet() const
{
    if (MIMETypeRegistry::isJavaAppletMIMEType(type()))
        return true;
        
    Node* child = firstChild();
    while (child) {
        if (child->isElementNode()) {
            Element* e = static_cast<Element*>(child);
            if (e->hasTagName(paramTag) &&
                e->getAttribute(nameAttr).domString().lower() == "type" &&
                MIMETypeRegistry::isJavaAppletMIMEType(e->getAttribute(valueAttr).domString()))
                return true;
            else if (e->hasTagName(objectTag) && static_cast<HTMLObjectElement*>(e)->containsJavaApplet())
                return true;
            else if (e->hasTagName(appletTag))
                return true;
        }
        child = child->nextSibling();
    }
    
    return false;
}

}
