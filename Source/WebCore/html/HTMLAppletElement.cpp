/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2012 Apple Inc. All rights reserved.
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

#include "Attribute.h"
#include "Frame.h"
#include "HTMLDocument.h"
#include "HTMLImageLoader.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "RenderApplet.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document* document, bool createdByParser)
    : HTMLPlugInImageElement(tagName, document, createdByParser, ShouldNotPreferPlugInsForImages)
{
    ASSERT(hasTagName(appletTag));
}

PassRefPtr<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLAppletElement(tagName, document, createdByParser));
}

void HTMLAppletElement::parseMappedAttribute(Attribute* attr)
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
    } else if (isIdAttributeName(attr->name())) {
        const AtomicString& newId = attr->value();
        if (inDocument() && document()->isHTMLDocument()) {
            HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
            document->removeExtraNamedItem(m_id);
            document->addExtraNamedItem(newId);
        }
        m_id = newId;
        // also call superclass
        HTMLPlugInImageElement::parseMappedAttribute(attr);
    } else
        HTMLPlugInImageElement::parseMappedAttribute(attr);
}

void HTMLAppletElement::insertedIntoDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->addNamedItem(m_name);
        document->addExtraNamedItem(m_id);
    }

    HTMLPlugInImageElement::insertedIntoDocument();
}

void HTMLAppletElement::removedFromDocument()
{
    if (document()->isHTMLDocument()) {
        HTMLDocument* document = static_cast<HTMLDocument*>(this->document());
        document->removeNamedItem(m_name);
        document->removeExtraNamedItem(m_id);
    }

    HTMLPlugInImageElement::removedFromDocument();
}

bool HTMLAppletElement::rendererIsNeeded(RenderStyle* style)
{
    if (!fastHasAttribute(codeAttr))
        return false;

    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

RenderObject* HTMLAppletElement::createRenderer(RenderArena*, RenderStyle* style)
{
    if (!canEmbedJava())
        return RenderObject::createObject(this, style);

    return new (document()->renderArena()) RenderApplet(this);
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings() const
{
    if (!canEmbedJava())
        return 0;

    document()->updateLayoutIgnorePendingStylesheets();
    return renderPart();
}

void HTMLAppletElement::updateWidget(PluginCreationOption)
{
    setNeedsWidgetUpdate(false);
    // FIXME: This should ASSERT isFinishedParsingChildren() instead.
    if (!isFinishedParsingChildren())
        return;

    RenderEmbeddedObject* renderer = renderEmbeddedObject();

    int contentWidth = renderer->style()->width().isFixed() ? renderer->style()->width().value() :
        renderer->width() - renderer->borderAndPaddingWidth();
    int contentHeight = renderer->style()->height().isFixed() ? renderer->style()->height().value() :
        renderer->height() - renderer->borderAndPaddingHeight();

    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("code");
    paramValues.append(getAttribute(codeAttr).string());

    const AtomicString& codeBase = getAttribute(codebaseAttr);
    if (!codeBase.isNull()) {
        paramNames.append("codeBase");
        paramValues.append(codeBase);
    }

    const AtomicString& name = document()->isHTMLDocument() ? getAttribute(nameAttr) : getIdAttribute();
    if (!name.isNull()) {
        paramNames.append("name");
        paramValues.append(name);
    }

    const AtomicString& archive = getAttribute(archiveAttr);
    if (!archive.isNull()) {
        paramNames.append("archive");
        paramValues.append(archive);
    }

    paramNames.append("baseURL");
    paramValues.append(document()->baseURL().string());

    const AtomicString& mayScript = getAttribute(mayscriptAttr);
    if (!mayScript.isNull()) {
        paramNames.append("mayScript");
        paramValues.append(mayScript);
    }

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->hasTagName(paramTag))
            continue;

        HTMLParamElement* param = static_cast<HTMLParamElement*>(child);
        if (param->name().isEmpty())
            continue;

        paramNames.append(param->name());
        paramValues.append(param->value());
    }

    Frame* frame = document()->frame();
    ASSERT(frame);

    renderer->setWidget(frame->loader()->subframeLoader()->createJavaAppletWidget(IntSize(contentWidth, contentHeight), this, paramNames, paramValues));
}

bool HTMLAppletElement::canEmbedJava() const
{
    if (document()->securityOrigin()->isSandboxed(SandboxPlugins))
        return false;

    Settings* settings = document()->settings();
    return settings && settings->isJavaEnabled();
}

}
