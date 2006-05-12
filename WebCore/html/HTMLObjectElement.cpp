/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"
#include "HTMLObjectElement.h"

#include "EventNames.h"
#include "Frame.h"
#include "HTMLFormElement.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "RenderImage.h"
#include "Text.h"
#include "csshelper.h"
#include "html_imageimpl.h"
#include "render_frames.h"
#include "render_replaced.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

HTMLObjectElement::HTMLObjectElement(Document *doc) 
: HTMLPlugInElement(objectTag, doc)
, m_imageLoader(0)
{
    needWidgetUpdate = false;
    m_useFallbackContent = false;
    m_complete = false;
    m_docNamedItem = true;
}

HTMLObjectElement::~HTMLObjectElement()
{
#if __APPLE__
    // m_instance should have been cleaned up in detach().
    assert(!m_instance);
#endif
    
    delete m_imageLoader;
}

#if __APPLE__
KJS::Bindings::Instance *HTMLObjectElement::getInstance() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    if (m_instance)
        return m_instance.get();

    if (RenderObject *r = renderer()) {
        if (r->isWidget()) {
            if (Widget* widget = static_cast<RenderWidget*>(r)->widget()) {
                // Call into the frame (and over the bridge) to pull the Bindings::Instance
                // from the guts of the plugin.
                m_instance = frame->getObjectInstanceForWidget(widget);
                // Applet may specified with <object> tag.
                if (!m_instance)
                    m_instance = frame->getAppletInstanceForWidget(widget);
            }
        }
    }

    return m_instance.get();
}
#endif

HTMLFormElement* HTMLObjectElement::form() const
{
    for (Node* p = parentNode(); p != 0; p = p->parentNode()) {
        if (p->hasTagName(formTag))
            return static_cast<HTMLFormElement*>(p);
    }
    
    return 0;
}

void HTMLObjectElement::parseMappedAttribute(MappedAttribute *attr)
{
    String val = attr->value();
    int pos;
    if (attr->name() == typeAttr) {
        serviceType = val.deprecatedString().lower();
        pos = serviceType.find( ";" );
        if ( pos!=-1 )
          serviceType = serviceType.left( pos );
        if (renderer())
          needWidgetUpdate = true;
        if (!isImageType() && m_imageLoader) {
          delete m_imageLoader;
          m_imageLoader = 0;
        }
    } else if (attr->name() == dataAttr) {
        url = WebCore::parseURL(val).deprecatedString();
        if (renderer())
          needWidgetUpdate = true;
        if (renderer() && isImageType()) {
          if (!m_imageLoader)
              m_imageLoader = new HTMLImageLoader(this);
          m_imageLoader->updateFromElement();
        }
    } else if (attr->name() == classidAttr) {
        classId = val;
        if (renderer())
          needWidgetUpdate = true;
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

Document* HTMLObjectElement::contentDocument() const
{
    // ###
    return 0;
}

bool HTMLObjectElement::rendererIsNeeded(RenderStyle *style)
{
    if (m_useFallbackContent || isImageType())
        return HTMLPlugInElement::rendererIsNeeded(style);

    Frame *frame = document()->frame();
    if (!frame || !frame->pluginsEnabled())
        return false;
    
    return true;
}

RenderObject *HTMLObjectElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    if (m_useFallbackContent)
        return RenderObject::createObject(this, style);
    if (isImageType())
        return new (arena) RenderImage(this);
    return new (arena) RenderPartObject(this);
}

void HTMLObjectElement::attach()
{
    HTMLPlugInElement::attach();

    if (renderer() && !m_useFallbackContent) {
        if (isImageType()) {
            if (!m_imageLoader)
                m_imageLoader = new HTMLImageLoader(this);
            m_imageLoader->updateFromElement();
            if (renderer()) {
                RenderImage* imageObj = static_cast<RenderImage*>(renderer());
                imageObj->setCachedImage(m_imageLoader->image());
            }
        } else {
            if (needWidgetUpdate) {
                // Set needWidgetUpdate to false before calling updateWidget because updateWidget may cause
                // this method or recalcStyle (which also calls updateWidget) to be called.
                needWidgetUpdate = false;
                static_cast<RenderPartObject*>(renderer())->updateWidget();
            } else {
                needWidgetUpdate = true;
                setChanged();
            }
        }
    }
}

void HTMLObjectElement::closeRenderer()
{
    // The parser just reached </object>.
    setComplete(true);
    
    HTMLPlugInElement::closeRenderer();
}

void HTMLObjectElement::setComplete(bool complete)
{
    if (complete != m_complete) {
        m_complete = complete;
        if (complete && !m_useFallbackContent) {
            needWidgetUpdate = true;
            if (inDocument())
                setChanged();
        }
    }
}

void HTMLObjectElement::detach()
{
    if (attached() && renderer() && !m_useFallbackContent) {
        // Update the widget the next time we attach (detaching destroys the plugin).
        needWidgetUpdate = true;
    }

#if __APPLE__
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
    if (!m_useFallbackContent && needWidgetUpdate && renderer() && !isImageType()) {
        detach();
        attach();
    }
    HTMLPlugInElement::recalcStyle(ch);
}

void HTMLObjectElement::childrenChanged()
{
    updateDocNamedItem();
    if (inDocument() && !m_useFallbackContent) {
        needWidgetUpdate = true;
        setChanged();
    }
}

bool HTMLObjectElement::isURLAttribute(Attribute *attr) const
{
    return (attr->name() == dataAttr || (attr->name() == usemapAttr && attr->value().domString()[0] != '#'));
}

bool HTMLObjectElement::isImageType()
{
    if (serviceType.isEmpty() && url.startsWith("data:")) {
        // Extract the MIME type from the data URL.
        int index = url.find(';');
        if (index == -1)
            index = url.find(',');
        if (index != -1) {
            int len = index - 5;
            if (len > 0)
                serviceType = url.mid(5, len);
            else
                serviceType = "text/plain"; // Data URLs with no MIME type are considered text/plain.
        }
    }
    
    return Image::supportsType(serviceType);
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
    // <param> elements and whitespace can be found by name in a
    // document, and other <object> elements cannot."
    bool wasNamedItem = m_docNamedItem;
    bool isNamedItem = true;
    Node* child = firstChild();
    while (child && isNamedItem) {
        if (child->isElementNode()) {
            if (!static_cast<Element*>(child)->hasTagName(paramTag))
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

void HTMLObjectElement::setCode(const String &value)
{
    setAttribute(codeAttr, value);
}

String HTMLObjectElement::archive() const
{
    return getAttribute(archiveAttr);
}

void HTMLObjectElement::setArchive(const String &value)
{
    setAttribute(archiveAttr, value);
}

String HTMLObjectElement::border() const
{
    return getAttribute(borderAttr);
}

void HTMLObjectElement::setBorder(const String &value)
{
    setAttribute(borderAttr, value);
}

String HTMLObjectElement::codeBase() const
{
    return getAttribute(codebaseAttr);
}

void HTMLObjectElement::setCodeBase(const String &value)
{
    setAttribute(codebaseAttr, value);
}

String HTMLObjectElement::codeType() const
{
    return getAttribute(codetypeAttr);
}

void HTMLObjectElement::setCodeType(const String &value)
{
    setAttribute(codetypeAttr, value);
}

String HTMLObjectElement::data() const
{
    return getAttribute(dataAttr);
}

void HTMLObjectElement::setData(const String &value)
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

String HTMLObjectElement::hspace() const
{
    return getAttribute(hspaceAttr);
}

void HTMLObjectElement::setHspace(const String &value)
{
    setAttribute(hspaceAttr, value);
}

String HTMLObjectElement::standby() const
{
    return getAttribute(standbyAttr);
}

void HTMLObjectElement::setStandby(const String &value)
{
    setAttribute(standbyAttr, value);
}

int HTMLObjectElement::tabIndex() const
{
    return getAttribute(tabindexAttr).toInt();
}

void HTMLObjectElement::setTabIndex(int tabIndex)
{
    setAttribute(tabindexAttr, String::number(tabIndex));
}

String HTMLObjectElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLObjectElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

String HTMLObjectElement::useMap() const
{
    return getAttribute(usemapAttr);
}

void HTMLObjectElement::setUseMap(const String &value)
{
    setAttribute(usemapAttr, value);
}

String HTMLObjectElement::vspace() const
{
    return getAttribute(vspaceAttr);
}

void HTMLObjectElement::setVspace(const String &value)
{
    setAttribute(vspaceAttr, value);
}

}
