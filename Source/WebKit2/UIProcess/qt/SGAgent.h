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

#ifndef SGAgent_h
#define SGAgent_h

#include "Deque.h"
#include "OwnPtr.h"
#include "HashMap.h"

class QImage;
class QRect;
class QSGItem;
class QSGNode;
class QSize;

namespace WebKit {

class NodeUpdate;
class PageNode;
class SGTileNode;

// Takes care of taking update requests then fulfilling them asynchronously on the scene graph thread.
class SGAgent {
public:
    SGAgent(QSGItem*);

    int createTileNode(float scale);
    void removeTileNode(int nodeID);
    void setNodeBackBuffer(int nodeID, const QImage& texture, const QRect& sourceRect, const QRect& targetRect);
    void swapTileBuffers();

    // Called by the QSGItem.
    void updatePaintNode(QSGNode* itemNode);
    bool isSwapPending() const { return m_isSwapPending; }

private:
    QSGNode* getScaleNode(float scale, QSGNode* itemNode);

    QSGItem* item;
    Deque<OwnPtr<NodeUpdate> > nodeUpdatesQueue;
    HashMap<int, SGTileNode*> nodes;
    float lastScale;
    QSGNode* lastScaleNode;
    int nextNodeID;
    bool m_isSwapPending;
};

struct NodeUpdate {
    enum Type {
        CreateTile,
        RemoveTile,
        SetBackBuffer,
        SwapTileBuffers
    };
    NodeUpdate(Type type)
        : type(type)
    { }
    virtual ~NodeUpdate() { }
    Type type;
};

}

#endif /* SGAgent_h */
