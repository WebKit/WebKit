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
#include "HTMLNames.h"
#include "HTMLParamElement.h"
#include "RenderEmbeddedObject.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include "SubframeLoader.h"
#include "Widget.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLAppletElement);

using namespace HTMLNames;

inline HTMLAppletElement::HTMLAppletElement(const QualifiedName& tagName, Document& document)
    : HTMLPlugInImageElement(tagName, document)
{
    ASSERT(hasTagName(appletTag));

    m_serviceType = "application/x-java-applet"_s;
}

Ref<HTMLAppletElement> HTMLAppletElement::create(const QualifiedName& tagName, Document& document)
{
    auto result = adoptRef(*new HTMLAppletElement(tagName, document));
    result->finishCreating();
    return result;
}

void HTMLAppletElement::parseAttribute(const QualifiedName& name, const AtomString& value)
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

bool HTMLAppletElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == codebaseAttr
        || attribute.name().localName() == objectAttr
        || HTMLPlugInImageElement::isURLAttribute(attribute);
}

bool HTMLAppletElement::rendererIsNeeded(const RenderStyle& style)
{
    if (!hasAttributeWithoutSynchronization(codeAttr))
        return false;
    return HTMLPlugInImageElement::rendererIsNeeded(style);
}

RenderPtr<RenderElement> HTMLAppletElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (!canEmbedJava())
        return RenderElement::createFor(*this, WTFMove(style));

    return RenderEmbeddedObject::createForApplet(*this, WTFMove(style));
}

RenderWidget* HTMLAppletElement::renderWidgetLoadingPlugin() const
{
    return canEmbedJava() ? HTMLPlugInImageElement::renderWidgetLoadingPlugin() : nullptr;
}

void HTMLAppletElement::updateWidget(CreatePlugins createPlugins)
{
    // FIXME: This should ASSERT isFinishedParsingChildren() instead.
    if (!isFinishedParsingChildren()) {
        setNeedsWidgetUpdate(false);
        return;
    }

#if PLATFORM(IOS_FAMILY)
    UNUSED_PARAM(createPlugins);
#else
    // FIXME: It's unfortunate that we have this special case here.
    // See http://trac.webkit.org/changeset/25128 and the plugins/netscape-plugin-setwindow-size.html test.
    if (createPlugins == CreatePlugins::No)
        return;

    setNeedsWidgetUpdate(false);

    Vector<String> paramNames;
    Vector<String> paramValues;

    paramNames.append("code");
    paramValues.append(attributeWithoutSynchronization(codeAttr).string());

    const AtomString& codeBase = attributeWithoutSynchronization(codebaseAttr);
    if (!codeBase.isNull()) {
        paramNames.append("codeBase"_s);
        paramValues.append(codeBase.string());
    }

    const AtomString& name = document().isHTMLDocument() ? getNameAttribute() : getIdAttribute();
    if (!name.isNull()) {
        paramNames.append("name");
        paramValues.append(name.string());
    }

    const AtomString& archive = attributeWithoutSynchronization(archiveAttr);
    if (!archive.isNull()) {
        paramNames.append("archive"_s);
        paramValues.append(archive.string());
    }

    paramNames.append("baseURL"_s);
    paramValues.append(document().baseURL().string());

    const AtomString& mayScript = attributeWithoutSynchronization(mayscriptAttr);
    if (!mayScript.isNull()) {
        paramNames.append("mayScript"_s);
        paramValues.append(mayScript.string());
    }

    for (auto& param : childrenOfType<HTMLParamElement>(*this)) {
        if (param.name().isEmpty())
            continue;

        paramNames.append(param.name());
        paramValues.append(param.value());
    }

    RefPtr<Frame> frame = document().frame();
    ASSERT(frame);

    auto contentSize = LayoutSize { };
    {
        auto* renderer = renderEmbeddedObject();
        auto& style = renderer->style();

        contentSize = LayoutSize { style.width().isFixed() ? LayoutUnit(style.width().value()) : renderer->width() - renderer->horizontalBorderAndPaddingExtent(),
            style.height().isFixed() ? LayoutUnit(style.height().value()) : renderer->height() - renderer->verticalBorderAndPaddingExtent() };
    }

    auto widget = frame->loader().subframeLoader().createJavaAppletWidget(roundedIntSize(contentSize), *this, paramNames, paramValues);
    // createJavaAppletWidget needs to check if the plugin(replacement) is obscured. Since the overlapping test requires up-to-date geometry, it initiates a top level style recalc/layout.
    // Let's see if this element still has a renderer after the style recalc.
    if (auto* renderer = renderEmbeddedObject())
        renderer->setWidget(WTFMove(widget));
#endif // !PLATFORM(IOS_FAMILY)
}

bool HTMLAppletElement::canEmbedJava() const
{
    if (document().isSandboxed(SandboxPlugins))
        return false;

    if (!document().settings().isJavaEnabled())
        return false;

    if (document().securityOrigin().isLocal() && !document().settings().isJavaEnabledForLocalFiles())
        return false;

    return true;
}

}
