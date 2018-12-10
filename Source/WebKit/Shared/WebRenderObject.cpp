/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebRenderObject.h"

#include "APIArray.h"
#include "APIString.h"
#include "WebPage.h"
#include <WebCore/Frame.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameLoaderClient.h>
#include <WebCore/RenderInline.h>
#include <WebCore/RenderText.h>
#include <WebCore/RenderView.h>
#include <WebCore/RenderWidget.h>

using namespace WebCore;

namespace WebKit {

RefPtr<WebRenderObject> WebRenderObject::create(WebPage* page)
{
    Frame* mainFrame = page->mainFrame();
    if (!mainFrame)
        return nullptr;

    if (!mainFrame->loader().client().hasHTMLView())
        return nullptr;

    RenderView* contentRenderer = mainFrame->contentRenderer();
    if (!contentRenderer)
        return nullptr;

    return adoptRef(new WebRenderObject(contentRenderer, true));
}

Ref<WebRenderObject> WebRenderObject::create(const String& name, const String& elementTagName, const String& elementID, RefPtr<API::Array>&& elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, const String& textSnippet, unsigned textLength, RefPtr<API::Array>&& children)
{
    return adoptRef(*new WebRenderObject(name, elementTagName, elementID, WTFMove(elementClassNames), absolutePosition, frameRect, textSnippet, textLength, WTFMove(children)));
}

WebRenderObject::WebRenderObject(RenderObject* renderer, bool shouldIncludeDescendants)
{
    m_name = renderer->renderName();
    m_textLength = 0;

    if (Node* node = renderer->node()) {
        if (is<Element>(*node)) {
            Element& element = downcast<Element>(*node);
            m_elementTagName = element.tagName();
            m_elementID = element.getIdAttribute();
            
            if (element.isStyledElement() && element.hasClass()) {
                Vector<RefPtr<API::Object>> classNames;
                classNames.reserveInitialCapacity(element.classNames().size());

                for (size_t i = 0, size = element.classNames().size(); i < size; ++i)
                    classNames.append(API::String::create(element.classNames()[i]));

                m_elementClassNames = API::Array::create(WTFMove(classNames));
            }
        }

        if (node->isTextNode()) {
            String value = node->nodeValue();
            m_textLength = value.length();

            const int maxSnippetLength = 40;
            if (value.length() > maxSnippetLength)
                m_textSnippet = value.substring(0, maxSnippetLength);
            else
                m_textSnippet = value;
        }
    }

    // FIXME: broken with transforms
    m_absolutePosition = flooredIntPoint(renderer->localToAbsolute());

    if (is<RenderBox>(*renderer))
        m_frameRect = snappedIntRect(downcast<RenderBox>(*renderer).frameRect());
    else if (is<RenderText>(*renderer)) {
        m_frameRect = downcast<RenderText>(*renderer).linesBoundingBox();
        m_frameRect.setLocation(downcast<RenderText>(*renderer).firstRunLocation());
    } else if (is<RenderInline>(*renderer))
        m_frameRect = IntRect(downcast<RenderInline>(*renderer).borderBoundingBox());

    if (!shouldIncludeDescendants)
        return;

    Vector<RefPtr<API::Object>> children;

    for (auto* coreChild = renderer->firstChildSlow(); coreChild; coreChild = coreChild->nextSibling())
        children.append(adoptRef(*new WebRenderObject(coreChild, shouldIncludeDescendants)));

    if (is<RenderWidget>(*renderer)) {
        auto* widget = downcast<RenderWidget>(*renderer).widget();
        if (is<FrameView>(widget)) {
            if (auto* coreContentRenderer = downcast<FrameView>(*widget).frame().contentRenderer())
                children.append(adoptRef(*new WebRenderObject(coreContentRenderer, shouldIncludeDescendants)));
        }
    }

    m_children = API::Array::create(WTFMove(children));
}

WebRenderObject::WebRenderObject(const String& name, const String& elementTagName, const String& elementID, RefPtr<API::Array>&& elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, const String& textSnippet, unsigned textLength, RefPtr<API::Array>&& children)
    : m_children(WTFMove(children))
    , m_name(name)
    , m_elementTagName(elementTagName)
    , m_elementID(elementID)
    , m_textSnippet(textSnippet)
    , m_elementClassNames(WTFMove(elementClassNames))
    , m_absolutePosition(absolutePosition)
    , m_frameRect(frameRect)
    , m_textLength(textLength)
{
}

WebRenderObject::~WebRenderObject()
{
}

} // namespace WebKit
