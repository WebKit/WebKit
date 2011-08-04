/*
 * Copyright (C) 2010, 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "SGTileNode.h"

namespace WebKit {

SGTileNode::SGTileNode()
    : m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
{
    setGeometry(&m_geometry);
    setMaterial(&m_material);
    setOpaqueMaterial(&m_opaqueMaterial);
}

void SGTileNode::setTargetRect(const QRectF& r)
{
    if (m_targetRect == r)
        return;
    m_targetRect = r;
    QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_targetRect, m_sourceRect);
    markDirty(DirtyGeometry);
}

void SGTileNode::setSourceRect(const QRectF& r)
{
    if (m_sourceRect == r)
        return;
    m_sourceRect = r;
    QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_targetRect, m_sourceRect);
    markDirty(DirtyGeometry);
}

void SGTileNode::setTexture(QSGTexture* texture)
{
    if (m_material.texture() == texture)
        return;
    m_material.setTexture(texture);
    m_opaqueMaterial.setTexture(texture);
    m_texture.reset(texture);
    markDirty(DirtyMaterial);
}

}
