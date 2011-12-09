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
#include "QtSGTileNode.h"

#include <QtQuick/QSGEngine>
#include <QtQuick/QSGFlatColorMaterial>
#include <QtQuick/QSGTexture>

namespace WebKit {

QtSGTileNode::QtSGTileNode(QSGEngine* engine)
    : m_engine(engine)
    , m_geometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4)
    , m_textureMaterialsCreated(false)
{
    setFlags(OwnsMaterial | OwnsOpaqueMaterial);
    setGeometry(&m_geometry);
    setMaterial(new QSGFlatColorMaterial);
    setOpaqueMaterial(new QSGFlatColorMaterial);
}

void QtSGTileNode::setBackBuffer(const QImage& backBuffer, const QRectF& sourceRect, const QRectF& targetRect)
{
    m_backBufferTexture.reset(m_engine->createTextureFromImage(backBuffer));
    m_backBufferTargetRect = targetRect;
    m_backBufferSourceRect = m_backBufferTexture->convertToNormalizedSourceRect(sourceRect);

    // Force the texture upload.
    m_backBufferTexture->bind();
}

void QtSGTileNode::swapBuffersIfNeeded()
{
    if (!m_backBufferTexture)
        return;

    if (!m_textureMaterialsCreated) {
        setMaterial(new QSGTextureMaterial);
        setOpaqueMaterial(new QSGOpaqueTextureMaterial);
        m_textureMaterialsCreated = true;
    }

    static_cast<QSGTextureMaterial*>(material())->setTexture(m_backBufferTexture.data());
    static_cast<QSGOpaqueTextureMaterial*>(opaqueMaterial())->setTexture(m_backBufferTexture.data());
    markDirty(DirtyMaterial);

    QSGGeometry::updateTexturedRectGeometry(&m_geometry, m_backBufferTargetRect, m_backBufferSourceRect);
    markDirty(DirtyGeometry);

    m_frontBufferTexture.swap(m_backBufferTexture);
    m_backBufferTexture.reset();
}

}
