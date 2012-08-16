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

#include "ComposedShadowTreeWalker.h"
#include "ContainerNode.h"
#include "ContentDistributor.h"
#include "ElementShadow.h"
#include "FlowThreadController.h"
#include "HTMLContentElement.h"
#include "HTMLNames.h"
#include "HTMLShadowElement.h"
#include "Node.h"
#include "RenderFullScreen.h"
#include "RenderNamedFlowThread.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "ShadowRoot.h"

#if ENABLE(SVG)
#include "SVGNames.h"
#endif

namespace WebCore {

using namespace HTMLNames;

NodeRenderingContext::NodeRenderingContext(Node* node)
    : m_node(node)
    , m_style(0)
    , m_parentFlowRenderer(0)
{
    ComposedShadowTreeWalker::findParent(m_node, &m_parentDetails);
}

NodeRenderingContext::NodeRenderingContext(Node* node, RenderStyle* style)
    : m_node(node)
    , m_style(style)
    , m_parentFlowRenderer(0)
{
}

NodeRenderingContext::~NodeRenderingContext()
{
}

void NodeRenderingContext::setStyle(PassRefPtr<RenderStyle> style)
{
    m_style = style;
    moveToFlowThreadIfNeeded();
}

PassRefPtr<RenderStyle> NodeRenderingContext::releaseStyle()
{
    return m_style.release();
}

RenderObject* NodeRenderingContext::nextRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->nextSibling();

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->nextRendererForNode(m_node);

    // Avoid an O(N^2) problem with this function by not checking for
    // nextRenderer() when the parent element hasn't attached yet.
    if (m_parentDetails.node() && !m_parentDetails.node()->attached())
        return 0;

    ComposedShadowTreeWalker walker(m_node);
    do {
        walker.nextSibling();
        if (!walker.get())
            return 0;
        if (RenderObject* renderer = walker.get()->renderer()) {
            // Do not return elements that are attached to a different flow-thread.
            if (renderer->style() && !renderer->style()->flowThread().isEmpty())
                continue;
            return renderer;
        }
    } while (true);

    ASSERT_NOT_REACHED();
    return 0;
}

RenderObject* NodeRenderingContext::previousRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->previousSibling();

    if (m_parentFlowRenderer)
        return m_parentFlowRenderer->previousRendererForNode(m_node);

    // FIXME: We should have the same O(N^2) avoidance as nextRenderer does
    // however, when I tried adding it, several tests failed.

    ComposedShadowTreeWalker walker(m_node);
    do {
        walker.previousSibling();
        if (!walker.get())
            return 0;
        if (RenderObject* renderer = walker.get()->renderer()) {
            // Do not return elements that are attached to a different flow-thread.
            if (renderer->style() && !renderer->style()->flowThread().isEmpty())
                continue;
            return renderer;
        }
    } while (true);

    ASSERT_NOT_REACHED();
    return 0;
}

RenderObject* NodeRenderingContext::parentRenderer() const
{
    if (RenderObject* renderer = m_node->renderer())
        return renderer->parent();
    if (m_parentFlowRenderer)
        return m_parentFlowRenderer;

    return m_parentDetails.node() ? m_parentDetails.node()->renderer() : 0;
}

bool NodeRenderingContext::shouldCreateRenderer() const
{
    if (!m_parentDetails.node())
        return false;
    RenderObject* parentRenderer = this->parentRenderer();
    if (!parentRenderer)
        return false;
    if (!parentRenderer->canHaveChildren())
        return false;
    if (!m_parentDetails.node()->childShouldCreateRenderer(*this))
        return false;
    return true;
}

void NodeRenderingContext::moveToFlowThreadIfNeeded()
{
    if (!m_node->document()->cssRegionsEnabled())
        return;

    if (!m_node->isElementNode() || !m_style || m_style->flowThread().isEmpty())
        return;

    // FIXME: Do not collect elements if they are in shadow tree.
    if (m_node->isInShadowTree())
        return;

#if ENABLE(SVG)
    // Allow only svg root elements to be directly collected by a render flow thread.
    if (m_node->isSVGElement()
        && (!(m_node->hasTagName(SVGNames::svgTag) && m_node->parentNode() && !m_node->parentNode()->isSVGElement())))
        return;
#endif

    m_flowThread = m_style->flowThread();
    ASSERT(m_node->document()->renderView());
    FlowThreadController* flowThreadController = m_node->document()->renderView()->flowThreadController();
    m_parentFlowRenderer = flowThreadController->ensureRenderFlowThreadWithName(m_flowThread);
    flowThreadController->registerNamedFlowContentNode(m_node, m_parentFlowRenderer);
}

bool NodeRenderingContext::isOnEncapsulationBoundary() const
{
    return isOnUpperEncapsulationBoundary() || isLowerEncapsulationBoundary(m_parentDetails.insertionPoint()) || isLowerEncapsulationBoundary(m_node->parentNode());
}

bool NodeRenderingContext::isOnUpperEncapsulationBoundary() const
{
    return m_node->parentNode() && m_node->parentNode()->isShadowRoot();
}

NodeRendererFactory::NodeRendererFactory(Node* node)
    : m_context(node)
{
}

RenderObject* NodeRendererFactory::createRenderer()
{
    Node* node = m_context.node();
    RenderObject* newRenderer = node->createRenderer(node->document()->renderArena(), m_context.style());
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

void NodeRendererFactory::createRendererIfNeeded()
{
    Node* node = m_context.node();
    Document* document = node->document();
    if (!document->shouldCreateRenderers())
        return;

    ASSERT(!node->renderer());
    ASSERT(document->shouldCreateRenderers());

    if (!m_context.shouldCreateRenderer())
        return;

    Element* element = node->isElementNode() ? toElement(node) : 0;
    if (element)
        m_context.setStyle(element->styleForRenderer());
    else if (RenderObject* parentRenderer = m_context.parentRenderer())
        m_context.setStyle(parentRenderer->style());

    if (!node->rendererIsNeeded(m_context)) {
        if (element && m_context.style()->affectedByEmpty())
            element->setStyleAffectedByEmpty();
        return;
    }

    RenderObject* parentRenderer = m_context.hasFlowThreadParent() ? m_context.parentFlowRenderer() : m_context.parentRenderer();
    // Do not call m_context.nextRenderer() here in the first clause, because it expects to have
    // the renderer added to its parent already.
    RenderObject* nextRenderer = m_context.hasFlowThreadParent() ? m_context.parentFlowRenderer()->nextRendererForNode(node) : m_context.nextRenderer();
    RenderObject* newRenderer = createRenderer();

#if ENABLE(FULLSCREEN_API)
    if (document->webkitIsFullScreen() && document->webkitCurrentFullScreenElement() == node)
        newRenderer = RenderFullScreen::wrapRenderer(newRenderer, parentRenderer, document);
#endif

    if (!newRenderer)
        return;

    // Note: Adding newRenderer instead of renderer(). renderer() may be a child of newRenderer.
    parentRenderer->addChild(newRenderer, nextRenderer);
}

}
