/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "HTMLFrameOwnerElement.h"

#include "FrameLoader.h"
#include "LocalDOMWindow.h"
#include "LocalFrame.h"
#include "RemoteFrame.h"
#include "RemoteFrameClient.h"
#include "RenderWidget.h"
#include "SVGDocument.h"
#include "SVGElementTypeHelpers.h"
#include "ScriptController.h"
#include "ShadowRoot.h"
#include "StyleTreeResolver.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLFrameOwnerElement);

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName, Document& document, OptionSet<TypeFlag> constructionType)
    : HTMLElement(tagName, document, constructionType)
{
}

RenderWidget* HTMLFrameOwnerElement::renderWidget() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    return dynamicDowncast<RenderWidget>(renderer());
}

void HTMLFrameOwnerElement::setContentFrame(Frame& frame)
{
    // Make sure we will not end up with two frames referencing the same owner element.
    ASSERT(!m_contentFrame || m_contentFrame->ownerElement() != this);
    // Disconnected frames should not be allowed to load.
    ASSERT(isConnected());
    m_contentFrame = frame;

    for (RefPtr<ContainerNode> node = this; node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame()
{
    if (!m_contentFrame)
        return;

    m_contentFrame = nullptr;

    for (RefPtr<ContainerNode> node = this; node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::disconnectContentFrame()
{
    if (RefPtr frame = m_contentFrame.get()) {
        if (frame->settings().siteIsolationEnabled())
            frame->broadcastFrameRemovalToOtherProcesses();
        frame->frameDetached();
        frame->disconnectOwnerElement();
    }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement()
{
    if (m_contentFrame)
        m_contentFrame->disconnectOwnerElement();
}

Document* HTMLFrameOwnerElement::contentDocument() const
{
    if (auto* localFrame = dynamicDowncast<LocalFrame>(m_contentFrame.get()))
        return localFrame->document();
    return nullptr;
}

WindowProxy* HTMLFrameOwnerElement::contentWindow() const
{
    return m_contentFrame ? &m_contentFrame->windowProxy() : nullptr;
}

void HTMLFrameOwnerElement::setSandboxFlags(SandboxFlags flags)
{
    m_sandboxFlags = flags;
}

bool HTMLFrameOwnerElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    return m_contentFrame && HTMLElement::isKeyboardFocusable(event);
}

Document* HTMLFrameOwnerElement::getSVGDocument() const
{
    auto* document = contentDocument();
    if (is<SVGDocument>(document))
        return document;
    return nullptr;
}

void HTMLFrameOwnerElement::scheduleInvalidateStyleAndLayerComposition()
{
    if (Style::postResolutionCallbacksAreSuspended()) {
        RefPtr<HTMLFrameOwnerElement> element = this;
        Style::deprecatedQueuePostResolutionCallback([element] {
            element->invalidateStyleAndLayerComposition();
        });
    } else
        invalidateStyleAndLayerComposition();
}

bool HTMLFrameOwnerElement::isProhibitedSelfReference(const URL& completeURL) const
{
    // We allow one level of self-reference because some websites depend on that, but we don't allow more than one.
    bool foundOneSelfReference = false;
    for (Frame* frame = document().frame(); frame; frame = frame->tree().parent()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame);
        if (!localFrame)
            continue;
        // Use creationURL() because url() can be changed via History.replaceState() so it's not reliable.
        if (equalIgnoringFragmentIdentifier(localFrame->document()->creationURL(), completeURL)) {
            if (foundOneSelfReference)
                return true;
            foundOneSelfReference = true;
        }
    }
    return false;
}

bool SubframeLoadingDisabler::canLoadFrame(HTMLFrameOwnerElement& owner)
{
    for (RefPtr<ContainerNode> node = &owner; node; node = node->parentOrShadowHostNode()) {
        if (disabledSubtreeRoots().contains(node.get()))
            return false;
    }
    return true;
}

} // namespace WebCore
