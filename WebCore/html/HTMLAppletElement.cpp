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
#include "HTMLAppletElement.h"

#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "RenderApplet.h"
#include "RenderInline.h"
#include "Settings.h"
#include "ScriptController.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document* doc)
    : HTMLPlugInElement(tagName, doc)
{
    ASSERT(hasTagName(appletTag));
}

HTMLAppletElement::~HTMLAppletElement()
{
}

void HTMLAppletElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == altAttr ||
        attr->name() == archiveAttr ||
        attr->name() == codeAttr ||
        attr->name() == codebaseAttr ||
        attr->name() == mayscriptAttr ||
        attr->name() == objectAttr) {
        // Do nothing.
    } else if (attr->name() == nameAttr) {
        const AtomicString& newName = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeNamedItem(m_name);
            document->addNamedItem(newName);
        }
        m_name = newName;
    } else if (attr->name() == idAttr) {
        const AtomicString& newId = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeExtraNamedItem(m_id);
            document->addExtraNamedItem(newId);
        }
        m_id = newId;
        // also call superclass
        HTMLPlugInElement::parseMappedAttribute(attr);
    } else
        HTMLPlugInElement::parseMappedAttribute(attr);
}

void HTMLAppletElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->addNamedItem(m_name);
        document->addExtraNamedItem(m_id);
    }

    HTMLPlugInElement::insertedIntoDocument();
}

void HTMLAppletElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->removeNamedItem(m_name);
        document->removeExtraNamedItem(m_id);
    }

    HTMLPlugInElement::removedFromDocument();
}

bool HTMLAppletElement::rendererIsNeeded(RenderStyle* style)
{
    if (getAttribute(codeAttr).isNull())
        return false;

    return HTMLPlugInElement::rendererIsNeeded(style);
}

RenderObject* HTMLAppletElement::createRenderer(RenderArena*, RenderStyle* style)
{
    Settings* settings = document()->settings();

    if (settings && settings->isJavaEnabled()) {
        HashMap<String, String> args;

        args.set("code", getAttribute(codeAttr));

        const AtomicString& codeBase = getAttribute(codebaseAttr);
        if (!codeBase.isNull())
            args.set("codeBase", codeBase);

        const AtomicString& name = getAttribute(document()->isHTMLDocument() ? nameAttr : idAttr);
        if (!name.isNull())
            args.set("name", name);
        const AtomicString& archive = getAttribute(archiveAttr);
        if (!archive.isNull())
            args.set("archive", archive);

        args.set("baseURL", document()->baseURL().string());

        const AtomicString& mayScript = getAttribute(mayscriptAttr);
        if (!mayScript.isNull())
            args.set("mayScript", mayScript);

        // Other arguments (from <PARAM> tags) are added later.
        
        return new (document()->renderArena()) RenderApplet(this, args);
    }

    return RenderObject::createObject(this, style);
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings() const
{
    Settings* settings = document()->settings();
    if (!settings || !settings->isJavaEnabled())
        return 0;

    RenderApplet* applet = static_cast<RenderApplet*>(renderer());
    if (applet)
        applet->createWidgetIfNecessary();

    return applet;
}

void HTMLAppletElement::finishParsingChildren()
{
    // The parser just reached </applet>, so all the params are available now.
    HTMLPlugInElement::finishParsingChildren();
    if (renderer())
        renderer()->setNeedsLayout(true); // This will cause it to create its widget & the Java applet
}

String HTMLAppletElement::alt() const
{
    return getAttribute(altAttr);
}

void HTMLAppletElement::setAlt(const String &value)
{
    setAttribute(altAttr, value);
}

String HTMLAppletElement::archive() const
{
    return getAttribute(archiveAttr);
}

void HTMLAppletElement::setArchive(const String &value)
{
    setAttribute(archiveAttr, value);
}

String HTMLAppletElement::code() const
{
    return getAttribute(codeAttr);
}

void HTMLAppletElement::setCode(const String &value)
{
    setAttribute(codeAttr, value);
}

String HTMLAppletElement::codeBase() const
{
    return getAttribute(codebaseAttr);
}

void HTMLAppletElement::setCodeBase(const String &value)
{
    setAttribute(codebaseAttr, value);
}

String HTMLAppletElement::hspace() const
{
    return getAttribute(hspaceAttr);
}

void HTMLAppletElement::setHspace(const String &value)
{
    setAttribute(hspaceAttr, value);
}

String HTMLAppletElement::object() const
{
    return getAttribute(objectAttr);
}

void HTMLAppletElement::setObject(const String &value)
{
    setAttribute(objectAttr, value);
}

String HTMLAppletElement::vspace() const
{
    return getAttribute(vspaceAttr);
}

void HTMLAppletElement::setVspace(const String &value)
{
    setAttribute(vspaceAttr, value);
}

}
