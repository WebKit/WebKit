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

    m_serviceType = "application/x-java-applet";
}

PassRefPtr<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document* document, bool createdByParser)
{
    return adoptRef(new HTMLAppletElement(tagName, document, createdByParser));
}

void HTMLAppletElement::parseAttribute(Attribute* attr)
{
    if (attr->name() == altAttr ||
        attr->name() == archiveAttr ||
        attr->name() == codeAttr ||
        attr->name() == codebaseAttr ||
        attr->name() == mayscriptAttr ||
        attr->name() == objectAttr) {
        // Do nothing.
        return;
    }

    HTMLPlugInImageElement::parseAttribute(attr);
}

bool HTMLAppletElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    if (!fastHasAttribute(codeAttr))
        return false;
    return HTMLPlugInImageElement::rendererIsNeeded(context);
}

RenderObject* HTMLAppletElement::createRenderer(RenderArena*, RenderStyle* style)
{
    if (!canEmbedJava())
        return RenderObject::createObject(this, style);

    return new (document()->renderArena()) RenderApplet(this);
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings()
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

    LayoutUnit contentWidth = renderer->style()->width().isFixed() ? LayoutUnit(renderer->style()->width().value()) :
        renderer->width() - renderer->borderAndPaddingWidth();
    LayoutUnit contentHeight = renderer->style()->height().isFixed() ? LayoutUnit(renderer->style()->height().value()) :
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

    const AtomicString& name = document()->isHTMLDocument() ? getNameAttribute() : getIdAttribute();
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

    renderer->setWidget(frame->loader()->subframeLoader()->createJavaAppletWidget(roundedIntSize(LayoutSize(contentWidth, contentHeight)), this, paramNames, paramValues));
}

bool HTMLAppletElement::canEmbedJava() const
{
    if (document()->isSandboxed(SandboxPlugins))
        return false;

    Settings* settings = document()->settings();
    if (!settings)
        return false;

    if (!settings->isJavaEnabled())
        return false;

    if (document()->securityOrigin()->isLocal() && !settings->isJavaEnabledForLocalFiles())
        return false;

    return true;
}

}
