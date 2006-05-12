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
#include "HTMLEmbedElement.h"

#include "CSSPropertyNames.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "csshelper.h"
#include "RenderPartObject.h"

namespace WebCore {

using namespace HTMLNames;

HTMLEmbedElement::HTMLEmbedElement(Document* doc)
    : HTMLPlugInElement(embedTag, doc)
{
}

HTMLEmbedElement::~HTMLEmbedElement()
{
#if __APPLE__
    // m_instance should have been cleaned up in detach().
    assert(!m_instance);
#endif
}

#if __APPLE__
KJS::Bindings::Instance *HTMLEmbedElement::getInstance() const
{
    Frame* frame = document()->frame();
    if (!frame)
        return 0;

    if (m_instance)
        return m_instance.get();
    
    RenderObject *r = renderer();
    if (!r) {
        Node *p = parentNode();
        if (p && p->hasTagName(objectTag))
            r = p->renderer();
    }

    if (r && r->isWidget()){
        if (Widget* widget = static_cast<RenderWidget*>(r)->widget()) {
            // Call into the frame (and over the bridge) to pull the Bindings::Instance
            // from the guts of the Java VM.
            m_instance = frame->getEmbedInstanceForWidget(widget);
            // Applet may specified with <embed> tag.
            if (!m_instance)
                m_instance = frame->getAppletInstanceForWidget(widget);
        }
    }
    return m_instance.get();
}
#endif

bool HTMLEmbedElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == hiddenAttr) {
        result = eUniversal;
        return false;
    }
        
    return HTMLPlugInElement::mapToEntry(attrName, result);
}

void HTMLEmbedElement::parseMappedAttribute(MappedAttribute *attr)
{
    DeprecatedString val = attr->value().deprecatedString();
  
    int pos;
    if (attr->name() == typeAttr) {
        serviceType = val.lower();
        pos = serviceType.find( ";" );
        if ( pos!=-1 )
            serviceType = serviceType.left( pos );
    } else if (attr->name() == codeAttr ||
               attr->name() == srcAttr) {
         url = WebCore::parseURL(attr->value()).deprecatedString();
    } else if (attr->name() == pluginpageAttr ||
               attr->name() == pluginspageAttr) {
        pluginPage = val;
    } else if (attr->name() == hiddenAttr) {
        if (val.lower()=="yes" || val.lower()=="true") {
            // FIXME: Not dynamic, but it's not really important that such a rarely-used
            // feature work dynamically.
            addCSSLength( attr, CSS_PROP_WIDTH, "0" );
            addCSSLength( attr, CSS_PROP_HEIGHT, "0" );
        }
    } else if (attr->name() == nameAttr) {
        String newNameAttr = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* doc = static_cast<HTMLDocument*>(document());
            doc->removeNamedItem(oldNameAttr);
            doc->addNamedItem(newNameAttr);
        }
        oldNameAttr = newNameAttr;
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

bool HTMLEmbedElement::rendererIsNeeded(RenderStyle *style)
{
    Frame *frame = document()->frame();
    if (!frame || !frame->pluginsEnabled())
        return false;

    Node *p = parentNode();
    if (p && p->hasTagName(objectTag)) {
        assert(p->renderer());
        return false;
    }

    return true;
}

RenderObject *HTMLEmbedElement::createRenderer(RenderArena *arena, RenderStyle *style)
{
    return new (arena) RenderPartObject(this);
}

void HTMLEmbedElement::attach()
{
    HTMLPlugInElement::attach();

    if (renderer())
        static_cast<RenderPartObject*>(renderer())->updateWidget();
}

void HTMLEmbedElement::detach()
{
#if __APPLE__
    m_instance = 0;
#endif
    HTMLPlugInElement::detach();
}

void HTMLEmbedElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->addNamedItem(oldNameAttr);
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLEmbedElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument *doc = static_cast<HTMLDocument *>(document());
        doc->removeNamedItem(oldNameAttr);
    }

    HTMLPlugInElement::removedFromDocument();
}

bool HTMLEmbedElement::isURLAttribute(Attribute *attr) const
{
    return attr->name() == srcAttr;
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

}
