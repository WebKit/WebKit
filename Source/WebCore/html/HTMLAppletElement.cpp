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

#include "ElementIterator.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "RenderEmbeddedObject.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "Widget.h"

namespace WebCore {

using namespace HTMLNames;

HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document& document, bool createdByParser)
    : HTMLPlugInImageElement(tagName, document, createdByParser, ShouldNotPreferPlugInsForImages)
{
    ASSERT(hasTagName(appletTag));

    m_serviceType = "application/x-java-applet";
}

PassRefPtr<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document& document, bool createdByParser)
{
    return adoptRef(new HTMLAppletElement(tagName, document, createdByParser));
}

void HTMLAppletElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == altAttr
        || name == archiveAttr
        || name == codeAttr
        || name == codebaseAttr
        || name == mayscriptAttr
        || name == objectAttr) {
        // Do nothing.
        return;
    }

    HTMLPlugInImageElement::parseAttribute(name, value);
}

bool HTMLAppletElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!fastHasAttribute(codeAttr))
        return false;
    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLAppletElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (!canEmbedJava())
        return RenderElement::createFor(*this, std::move(style));

    return RenderEmbeddedObject::createForApplet(*this, std::move(style));
}

RenderWidget* HTMLAppletElement::renderWidgetForJSBindings() const
{
    if (!canEmbedJava())
        return 0;

    document().updateLayoutIgnorePendingStylesheets();
    return renderWidget();
}

void HTMLAppletElement::updateWidget(PluginCreationOption pluginCreationOption)
{
    setNeedsWidgetUpdate(false);
    // FIXME: This should ASSERT isFinishedParsingChildren() instead.
    if (!isFinishedParsingChildren())
        return;

#if PLATFORM(IOS)
    UNUSED_PARAM(pluginCreationOption);
#else
    // FIXME: It's sadness that we have this special case here.
    //        See http://trac.webkit.org/changeset/25128 and
    //        plugins/netscape-plugin-setwindow-size.html
    if (pluginCreationOption == CreateOnlyNonNetscapePlugins) {
        // Ensure updateWidget() is called again during layout to create the Netscape plug-in.
        setNeedsWidgetUpdate(true);
        return;
    }

    RenderEmbeddedObject* renderer = renderEmbeddedObject();

    LayoutUnit contentWidth = renderer->style().width().isFixed() ? LayoutUnit(renderer->style().width().value()) :
        renderer->width() - renderer->horizontalBorderAndPaddingExtent();
    LayoutUnit contentHeight = renderer->style().height().isFixed() ? LayoutUnit(renderer->style().height().value()) :
        renderer->height() - renderer->verticalBorderAndPaddingExtent();

    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("code");
    paramValues.append(getAttribute(codeAttr).string());

    const AtomicString& codeBase = getAttribute(codebaseAttr);
    if (!codeBase.isNull()) {
        paramNames.append("codeBase");
        paramValues.append(codeBase.string());
    }

    const AtomicString& name = document().isHTMLDocument() ? getNameAttribute() : getIdAttribute();
    if (!name.isNull()) {
        paramNames.append("name");
        paramValues.append(name.string());
    }

    const AtomicString& archive = getAttribute(archiveAttr);
    if (!archive.isNull()) {
        paramNames.append("archive");
        paramValues.append(archive.string());
    }

    paramNames.append("baseURL");
    paramValues.append(document().baseURL().string());

    const AtomicString& mayScript = getAttribute(mayscriptAttr);
    if (!mayScript.isNull()) {
        paramNames.append("mayScript");
        paramValues.append(mayScript.string());
    }

    for (auto& param : childrenOfType<HTMLParamElement>(*this)) {
        if (param.name().isEmpty())
            continue;

        paramNames.append(param.name());
        paramValues.append(param.value());
    }

    Frame* frame = document().frame();
    ASSERT(frame);

    renderer->setWidget(frame->loader().subframeLoader().createJavaAppletWidget(roundedIntSize(LayoutSize(contentWidth, contentHeight)), *this, paramNames, paramValues));
#endif // !PLATFORM(IOS)
}

bool HTMLAppletElement::canEmbedJava() const
{
    if (document().isSandboxed(SandboxPlugins))
        return false;

    Settings* settings = document().settings();
    if (!settings)
        return false;

    if (!settings->isJavaEnabled())
        return false;

    if (document().securityOrigin()->isLocal() && !settings->isJavaEnabledForLocalFiles())
        return false;

    return true;
}

}
