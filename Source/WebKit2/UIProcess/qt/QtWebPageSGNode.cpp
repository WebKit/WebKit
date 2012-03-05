/*
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "QtWebPageSGNode.h"

namespace WebKit {

QtWebPageSGNode::QtWebPageSGNode(PassRefPtr<LayerTreeHostProxy> layerTreeHost, Client* client)
    : m_layerTreeHost(layerTreeHost)
    , m_client(client)
    , m_contentsScale(1)
{
}

QSGRenderNode::StateFlags QtWebPageSGNode::changedStates()
{
    return StateFlags(StencilState) | ColorState | BlendState;
}

void QtWebPageSGNode::render(const RenderState& state)
{
    QTransform transform = matrix() ? matrix()->toTransform() : QTransform();
    float scale = contentsScale();
    m_layerTreeHost->paintToCurrentGLContext(QTransform(transform).scale(scale, scale), inheritedOpacity(), clipRect());
}

void QtWebPageSGNode::setContentsScale(float scale)
{
    MutexLocker lock(m_mutex);
    m_contentsScale = scale;
}

float QtWebPageSGNode::contentsScale()
{
    MutexLocker lock(m_mutex);
    return m_contentsScale;
}

void QtWebPageSGNode::setClipRect(const QRectF& rect)
{
    MutexLocker lock(m_mutex);
    m_clipRect = rect;
}

QRectF QtWebPageSGNode::clipRect()
{
    MutexLocker lock(m_mutex);
    return m_clipRect;
}

QtWebPageSGNode::~QtWebPageSGNode()
{
    if (m_layerTreeHost)
        m_layerTreeHost->purgeGLResources();

    if (m_client)
        m_client->willDeleteScenegraphNode();
}

}
