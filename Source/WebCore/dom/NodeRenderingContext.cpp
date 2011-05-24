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
#include "Node.h"
#include "RenderFullScreen.h"
#include "RenderObject.h"
#include "ShadowRoot.h"

namespace WebCore {

NodeRenderingContext::NodeRenderingContext(Node* node)
    : m_location(LocationNotInTree)
    , m_phase(AttachStraight)
    , m_node(node)
    , m_parentNodeForRenderingAndStyle(0)
    , m_visualParentShadowRoot(0)
    , m_style(0)
{
    ContainerNode* parent = m_node->parentOrHostNode();
    if (!parent)
        return;

    if (parent->isShadowBoundary()) {
        m_location = LocationShadowChild;
        m_parentNodeForRenderingAndStyle = parent->shadowHost();
        return;
    }

    m_location = LocationLightChild;

    if (parent->isElementNode()) {
        m_visualParentShadowRoot = toElement(parent)->shadowRoot();

        if (m_visualParentShadowRoot) {
            if (ContainerNode* contentContainer = m_visualParentShadowRoot->activeContentContainer()) {
                m_phase = AttachContentForwarded;
                m_parentNodeForRenderingAndStyle = NodeRenderingContext(contentContainer).parentNodeForRenderingAndStyle();
                return;
            } 
                
            m_phase = AttachContentLight;
            m_parentNodeForRenderingAndStyle = parent;
            return;
        }
    }

    m_parentNodeForRenderingAndStyle = parent;
}

NodeRenderingContext::NodeRenderingContext(Node* node, RenderStyle* style)
    : m_location(LocationNotInTree)
    , m_phase(AttachStraight)
    , m_node(node)
    , m_parentNodeForRenderingAndStyle(0)
    , m_visualParentShadowRoot(0)
    , m_style(style)
{
}

NodeRenderingContext::~NodeRenderingContext()
{
}

void NodeRenderingContext::setStyle(PassRefPtr<RenderStyle> style)
{
    m_style = style;
}

PassRefPtr<RenderStyle> NodeRenderingContext::releaseStyle()
{
    return m_style.release();
}

RenderObject* NodeRenderingContext::nextRenderer() const
{
    if (m_phase != AttachContentForwarded)
        return m_node->nextRenderer();
    // Returns 0 here to insert renderer at the end of child list.
    // We assume content children are always attached in tree order and
    // there is no partial render tree creation.
    return 0;
}

RenderObject* NodeRenderingContext::parentRenderer() const
{
    return m_parentNodeForRenderingAndStyle->renderer();
}

void NodeRenderingContext::hostChildrenChanged()
{
    if (m_phase == AttachContentLight)
        m_visualParentShadowRoot->hostChildrenChanged();
}

bool NodeRenderingContext::shouldCreateRenderer() const
{
    ASSERT(parentNodeForRenderingAndStyle());

    if (m_location == LocationNotInTree || m_phase == AttachContentLight)
        return false;

    RenderObject* parentRenderer = this->parentRenderer();
    if (!parentRenderer)
        return false;

    if (m_location == LocationLightChild && m_phase == AttachStraight) {
        // FIXME: Ignoring canHaveChildren() in a case of shadow children might be wrong.
        // See https://bugs.webkit.org/show_bug.cgi?id=52423
        if (!parentRenderer->canHaveChildren())
            return false;
    
        if (m_visualParentShadowRoot && !m_parentNodeForRenderingAndStyle->canHaveLightChildRendererWithShadow())
            return false;
    }

    if (!m_parentNodeForRenderingAndStyle->childShouldCreateRenderer(m_node))
        return false;

    return true;
}


NodeRendererFactory::NodeRendererFactory(Node* node)
    : m_context(node)
{
}

RenderObject* NodeRendererFactory::createRendererAndStyle()
{
    Node* node = m_context.node();
    Document* document = node->document();
    ASSERT(!node->renderer());
    ASSERT(document->shouldCreateRenderers());

    if (!m_context.shouldCreateRenderer())
        return 0;

    m_context.setStyle(node->styleForRenderer());
    if (!node->rendererIsNeeded(m_context))
        return 0;

    RenderObject* newRenderer = node->createRenderer(document->renderArena(), m_context.style());
    if (!newRenderer)
        return 0;

    if (!m_context.parentRenderer()->isChildAllowed(newRenderer, m_context.style())) {
        newRenderer->destroy();
        return 0;
    }

    node->setRenderer(newRenderer);
    newRenderer->setAnimatableStyle(m_context.releaseStyle()); // setAnimatableStyle() can depend on renderer() already being set.
    return newRenderer;
}

#if ENABLE(FULLSCREEN_API)
static RenderFullScreen* wrapWithRenderFullScreen(RenderObject* object, Document* document)
{
    RenderFullScreen* fullscreenRenderer = new (document->renderArena()) RenderFullScreen(document);
    fullscreenRenderer->setStyle(RenderFullScreen::createFullScreenStyle());
    // It's possible that we failed to create the new render and end up wrapping nothing.
    // We'll end up displaying a black screen, but Jer says this is expected.
    if (object)
        fullscreenRenderer->addChild(object);
    document->setFullScreenRenderer(fullscreenRenderer);
    return fullscreenRenderer;
}
#endif

void NodeRendererFactory::createRendererIfNeeded()
{
    Node* node = m_context.node();
    Document* document = node->document();

    if (!document->shouldCreateRenderers())
        return;

    RenderObject* newRenderer = createRendererAndStyle();

#if ENABLE(FULLSCREEN_API)
    if (document->webkitIsFullScreen() && document->webkitCurrentFullScreenElement() == node)
        newRenderer = wrapWithRenderFullScreen(newRenderer, document);
#endif

    // FIXME: This side effect should be visible from attach() code.
    m_context.hostChildrenChanged();

    if (!newRenderer)
        return;

    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    m_context.parentRenderer()->addChild(newRenderer, m_context.nextRenderer());
}

}
