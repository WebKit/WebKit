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
#include <WebCore/MainFrame.h>
#include <WebCore/RenderText.h>
#include <WebCore/RenderView.h>
#include <WebCore/RenderWidget.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebRenderObject> WebRenderObject::create(WebPage* page)
{
    Frame* mainFrame = page->mainFrame();
    if (!mainFrame)
        return 0;

    if (!mainFrame->loader().client().hasHTMLView())
        return 0;

    RenderView* contentRenderer = mainFrame->contentRenderer();
    if (!contentRenderer)
        return 0;

    return adoptRef(new WebRenderObject(contentRenderer, true));
}

PassRefPtr<WebRenderObject> WebRenderObject::create(const String& name, const String& elementTagName, const String& elementID, PassRefPtr<API::Array> elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, PassRefPtr<API::Array> children)
{
    return adoptRef(new WebRenderObject(name, elementTagName, elementID, elementClassNames, absolutePosition, frameRect, children));
}

WebRenderObject::WebRenderObject(RenderObject* renderer, bool shouldIncludeDescendants)
{
    m_name = renderer->renderName();

    if (Node* node = renderer->node()) {
        if (node->isElementNode()) {
            Element* element = toElement(node);
            m_elementTagName = element->tagName();
            m_elementID = element->getIdAttribute();
            
            if (element->isStyledElement() && element->hasClass()) {
                Vector<RefPtr<API::Object>> classNames;
                classNames.reserveInitialCapacity(element->classNames().size());

                for (size_t i = 0, size = element->classNames().size(); i < size; ++i)
                    classNames.append(API::String::create(element->classNames()[i]));

                m_elementClassNames = API::Array::create(WTF::move(classNames));
            }
        }
    }

    // FIXME: broken with transforms
    m_absolutePosition = flooredIntPoint(renderer->localToAbsolute());

    if (renderer->isBox())
        m_frameRect = toRenderBox(renderer)->pixelSnappedFrameRect();
    else if (renderer->isText()) {
        m_frameRect = toRenderText(renderer)->linesBoundingBox();
        m_frameRect.setLocation(toRenderText(renderer)->firstRunLocation());
    } else if (renderer->isRenderInline())
        m_frameRect = toRenderBoxModelObject(renderer)->borderBoundingBox();

    if (!shouldIncludeDescendants)
        return;

    Vector<RefPtr<API::Object>> children;

    for (RenderObject* coreChild = renderer->firstChildSlow(); coreChild; coreChild = coreChild->nextSibling()) {
        RefPtr<WebRenderObject> child = adoptRef(new WebRenderObject(coreChild, shouldIncludeDescendants));
        children.append(WTF::move(child));
    }

    if (renderer->isWidget()) {
        if (Widget* widget = toRenderWidget(renderer)->widget()) {
            if (widget->isFrameView()) {
                FrameView* frameView = toFrameView(widget);
                if (RenderView* coreContentRenderer = frameView->frame().contentRenderer()) {
                    RefPtr<WebRenderObject> contentRenderer = adoptRef(new WebRenderObject(coreContentRenderer, shouldIncludeDescendants));

                    children.append(WTF::move(contentRenderer));
                }
            }
        }
    }

    m_children = API::Array::create(WTF::move(children));
}

WebRenderObject::WebRenderObject(const String& name, const String& elementTagName, const String& elementID, PassRefPtr<API::Array> elementClassNames, WebCore::IntPoint absolutePosition, WebCore::IntRect frameRect, PassRefPtr<API::Array> children)
    : m_children(children)
    , m_name(name)
    , m_elementTagName(elementTagName)
    , m_elementID(elementID)
    , m_elementClassNames(elementClassNames)
    , m_absolutePosition(absolutePosition)
    , m_frameRect(frameRect)
{
}

WebRenderObject::~WebRenderObject()
{
}

} // namespace WebKit
