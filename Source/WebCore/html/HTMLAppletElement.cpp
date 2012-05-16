/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "RenderApplet.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

inline HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document* document)
    : HTMLPlugInElement(tagName, document)
{
    ASSERT(hasTagName(appletTag));
}

PassRefPtr<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLAppletElement(tagName, document));
}

void HTMLAppletElement::parseAttribute(const Attribute& attribute)
{
    if (attribute.name() == altAttr
        || attribute.name() == archiveAttr
        || attribute.name() == codeAttr
        || attribute.name() == codebaseAttr
        || attribute.name() == mayscriptAttr
        || attribute.name() == objectAttr) {
        // Do nothing.
    } else
        HTMLPlugInElement::parseAttribute(attribute);
}

bool HTMLAppletElement::rendererIsNeeded(const NodeRenderingContext& context)
{
    if (!fastHasAttribute(codeAttr))
        return false;
    return HTMLPlugInElement::rendererIsNeeded(context);
}

RenderObject* HTMLAppletElement::createRenderer(RenderArena*, RenderStyle* style)
{
    if (canEmbedJava()) {
        HashMap<String, String> args;

        args.set("code", getAttribute(codeAttr));

        const AtomicString& codeBase = getAttribute(codebaseAttr);
        if (!codeBase.isNull())
            args.set("codeBase", codeBase);

        const AtomicString& name = document()->isHTMLDocument() ? getNameAttribute() : getIdAttribute();
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

void HTMLAppletElement::defaultEventHandler(Event* event)
{
    RenderObject* r = renderer();
    if (!r || !r->isWidget())
        return;
    Widget* widget = toRenderWidget(r)->widget();
    if (!widget)
        return;
    widget->handleEvent(event);
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings()
{
    if (!canEmbedJava())
        return 0;

    if (!renderer() || !renderer()->isApplet())
        return 0;

    RenderApplet* applet = toRenderApplet(renderer());
    applet->createWidgetIfNecessary();
    return applet;
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

void HTMLAppletElement::finishParsingChildren()
{
    // The parser just reached </applet>, so all the params are available now.
    HTMLPlugInElement::finishParsingChildren();
    if (renderer())
        renderer()->setNeedsLayout(true); // This will cause it to create its widget & the Java applet
}

}
