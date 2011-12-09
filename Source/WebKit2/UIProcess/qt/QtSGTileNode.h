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

#ifndef QtSGTileNode_h
#define QtSGTileNode_h

#include <QtQuick/QSGGeometryNode>
#include <QtQuick/QSGOpaqueTextureMaterial>
#include <QtQuick/QSGTextureMaterial>

QT_BEGIN_NAMESPACE
class QSGEngine;
class QSGTexture;
QT_END_NAMESPACE

namespace WebKit {

class QtSGTileNode : public QSGGeometryNode {
public:
    QtSGTileNode(QSGEngine*);
    void setBackBuffer(const QImage&, const QRectF& sourceRect, const QRectF& targetRect);
    void swapBuffersIfNeeded();

private:
    QSGEngine* m_engine;
    QSGGeometry m_geometry;
    QScopedPointer<QSGTexture> m_frontBufferTexture;
    QScopedPointer<QSGTexture> m_backBufferTexture;
    bool m_textureMaterialsCreated;

    QRectF m_backBufferTargetRect;
    QRectF m_backBufferSourceRect;
};

}

#endif // QtSGTileNode_h
