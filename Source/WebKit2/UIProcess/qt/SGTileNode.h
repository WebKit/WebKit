/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef SGTileNode_h
#define SGTileNode_h

#include <QSGGeometryNode>
#include <QSGOpaqueTextureMaterial>
#include <QSGTextureMaterial>

QT_BEGIN_NAMESPACE
class QSGTexture;
QT_END_NAMESPACE

namespace WebKit {

class SGTileNode : public QSGGeometryNode {
public:
    SGTileNode();

    void setTargetRect(const QRectF&);
    void setSourceRect(const QRectF&);
    // This takes ownership of the texture.
    void setTexture(QSGTexture*);

private:
    QSGGeometry m_geometry;
    QSGOpaqueTextureMaterial m_opaqueMaterial;
    QSGTextureMaterial m_material;
    QScopedPointer<QSGTexture> m_texture;

    QRectF m_targetRect;
    QRectF m_sourceRect;
};

}

#endif /* SGTileNode_h */
