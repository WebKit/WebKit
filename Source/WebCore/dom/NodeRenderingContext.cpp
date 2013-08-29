/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "NodeRenderingContext.h"

#include "ContainerNode.h"
#include "ContentDistributor.h"
#include "FlowThreadController.h"
#include "HTMLContentElement.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Node.h"
#include "PseudoElement.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderObject.h"
#include "RenderText.h"
#include "RenderView.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "Text.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

NodeRenderingContext::NodeRenderingContext(Node* node)
    : m_node(node)
{
    m_renderingParent = NodeRenderingTraversal::parent(node);
}

NodeRenderingContext::~NodeRenderingContext()
{
}

static bool isRendererReparented(const RenderObject* renderer)
{
    if (!renderer->node()->isElementNode())
        return false;
    if (renderer->style() && !renderer->style()->flowThread().isEmpty())
        return true;
#if ENABLE(DIALOG_ELEMENT)
    if (toElement(renderer->node())->isInTopLayer())
        return true;
#endif
    return false;
}

RenderObject* NodeRenderingContext::nextRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->nextSibling();

#if ENABLE(DIALOG_ELEMENT)
    Element* element = m_node->isElementNode() ? toElement(m_node) : 0;
    if (element && element->isInTopLayer()) {
        const Vector<RefPtr<Element> >& topLayerElements = element->document()->topLayerElements();
        size_t position = topLayerElements.find(element);
        ASSERT(position != notFound);
        for (size_t i = position + 1; i < topLayerElements.size(); ++i) {
            if (RenderObject* renderer = topLayerElements[i]->renderer())
                return renderer;
        }
        return 0;
    }
#endif

    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    if (m_renderingParent && !m_renderingParent->attached())
        return 0;

    for (Node* sibling = NodeRenderingTraversal::nextSibling(m_node); sibling; sibling = NodeRenderingTraversal::nextSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }

    return 0;
}

RenderObject* NodeRenderingContext::previousRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->previousSibling();

#if ENABLE(DIALOG_ELEMENT)
    // FIXME: This doesn't work correctly for things in the top layer that are
    // display: none. We'd need to duplicate the logic in nextRenderer, but since
    // nothing needs that yet just assert.
    ASSERT(!m_node->isElementNode() || !toElement(m_node)->isInTopLayer());
#endif

    // FIXME: We should have the same O(N^2) avoidance as nextRenderer does
    // however, when I tried adding it, several tests failed.
    for (Node* sibling = NodeRenderingTraversal::previousSibling(m_node); sibling; sibling = NodeRenderingTraversal::previousSibling(sibling)) {
        RenderObject* renderer = sibling->renderer();
        if (renderer && !isRendererReparented(renderer))
            return renderer;
    }

    return 0;
}

RenderObject* NodeRenderingContext::parentRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->parent();

#if ENABLE(DIALOG_ELEMENT)
    if (m_node->isElementNode() && toElement(m_node)->isInTopLayer()) {
        // The parent renderer of top layer elements is the RenderView, but only
        // if the normal parent would have had a renderer.
        // FIXME: This behavior isn't quite right as the spec for top layer
        // only talks about display: none ancestors so putting a <dialog> inside
        // an <optgroup> seems like it should still work even though this check
        // will prevent it.
        if (!m_renderingParent || !m_renderingParent->renderer())
            return 0;
        return m_node->document()->renderView();
    }
#endif

    return m_renderingParent ? m_renderingParent->renderer() : 0;
}

bool NodeRenderingContext::resetStyleInheritance() const
{
    ContainerNode* parent = m_node->parentNode();
    return parent && parent->isShadowRoot() && toShadowRoot(parent)->resetStyleInheritance();
}

}
