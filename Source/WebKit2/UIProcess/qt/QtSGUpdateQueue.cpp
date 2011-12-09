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
#include "QtSGUpdateQueue.h"

#include "PassOwnPtr.h"
#include "QtSGTileNode.h"
#include <QtQuick/QQuickItem>

namespace WebKit {

struct NodeUpdateCreateTile : public NodeUpdate {
    NodeUpdateCreateTile(int nodeID, float scale)
        : NodeUpdate(CreateTile)
        , nodeID(nodeID)
        , scale(scale)
    { }
    int nodeID;
    float scale;
};

struct NodeUpdateRemoveTile : public NodeUpdate {
    NodeUpdateRemoveTile(int nodeID)
        : NodeUpdate(RemoveTile)
        , nodeID(nodeID)
    { }
    int nodeID;
};

struct NodeUpdateSetBackBuffer : public NodeUpdate {
    NodeUpdateSetBackBuffer(int nodeID, const QImage& backBuffer, const QRect& sourceRect, const QRect& targetRect)
        : NodeUpdate(SetBackBuffer)
        , nodeID(nodeID)
        , backBuffer(backBuffer)
        , sourceRect(sourceRect)
        , targetRect(targetRect)
    { }
    int nodeID;
    QImage backBuffer;
    QRect sourceRect;
    QRect targetRect;
};

struct NodeUpdateSwapTileBuffers : public NodeUpdate {
    NodeUpdateSwapTileBuffers()
        : NodeUpdate(SwapTileBuffers)
    { }
};

QtSGUpdateQueue::QtSGUpdateQueue(QQuickItem *item)
    : item(item)
    , lastScale(0)
    , lastScaleNode(0)
    , nextNodeID(1)
    , m_isSwapPending(false)
{
}

int QtSGUpdateQueue::createTileNode(float scale)
{
    int nodeID = nextNodeID++;
    nodeUpdateQueue.append(adoptPtr(new NodeUpdateCreateTile(nodeID, scale)));
    item->update();
    return nodeID;
}

void QtSGUpdateQueue::removeTileNode(int nodeID)
{
    nodeUpdateQueue.append(adoptPtr(new NodeUpdateRemoveTile(nodeID)));
    item->update();
}

void QtSGUpdateQueue::setNodeBackBuffer(int nodeID, const QImage& backBuffer, const QRect& sourceRect, const QRect& targetRect)
{
    nodeUpdateQueue.append(adoptPtr(new NodeUpdateSetBackBuffer(nodeID, backBuffer, sourceRect, targetRect)));
    item->update();
}

void QtSGUpdateQueue::swapTileBuffers()
{
    nodeUpdateQueue.append(adoptPtr(new NodeUpdateSwapTileBuffers()));
    m_isSwapPending = true;
    item->update();
}

void QtSGUpdateQueue::applyUpdates(QSGNode* itemNode)
{
    while (!nodeUpdateQueue.isEmpty()) {
        OwnPtr<NodeUpdate> nodeUpdate(nodeUpdateQueue.takeFirst());
        switch (nodeUpdate->type) {
        case NodeUpdate::CreateTile: {
            NodeUpdateCreateTile* createTileUpdate = static_cast<NodeUpdateCreateTile*>(nodeUpdate.get());
            QtSGTileNode* tileNode = new QtSGTileNode(item->sceneGraphEngine());
            getScaleNode(createTileUpdate->scale, itemNode)->prependChildNode(tileNode);
            nodes.set(createTileUpdate->nodeID, tileNode);
            break;
        }
        case NodeUpdate::RemoveTile: {
            NodeUpdateRemoveTile* removeUpdate = static_cast<NodeUpdateRemoveTile*>(nodeUpdate.get());
            QSGNode* node = nodes.take(removeUpdate->nodeID);
            QSGNode* scaleNode = node->parent();

            scaleNode->removeChildNode(node);
            if (!scaleNode->childCount()) {
                if (scaleNode == lastScaleNode) {
                    lastScale = 0;
                    lastScaleNode = 0;
                }
                delete scaleNode;
            }
            delete node;
            break;
        }
        case NodeUpdate::SetBackBuffer: {
            NodeUpdateSetBackBuffer* setBackBufferUpdate = static_cast<NodeUpdateSetBackBuffer*>(nodeUpdate.get());
            QtSGTileNode* tileNode = nodes.get(setBackBufferUpdate->nodeID);
            tileNode->setBackBuffer(setBackBufferUpdate->backBuffer, setBackBufferUpdate->sourceRect, setBackBufferUpdate->targetRect);
            break;
        }
        case NodeUpdate::SwapTileBuffers: {
            HashMap<int, QtSGTileNode*>::iterator end = nodes.end();
            for (HashMap<int, QtSGTileNode*>::iterator it = nodes.begin(); it != end; ++it)
                it->second->swapBuffersIfNeeded();
            m_isSwapPending = false;
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

QSGNode* QtSGUpdateQueue::getScaleNode(float scale, QSGNode* itemNode)
{
    if (scale == lastScale)
        return lastScaleNode;

    QSGTransformNode* scaleNode = new QSGTransformNode;
    QMatrix4x4 scaleMatrix;
    // Use scale(float,float) to prevent scaling the Z component.
    // Reverse the item's transform scale since our tiles were generated for this specific scale.
    scaleMatrix.scale(1 / scale, 1 / scale);
    scaleNode->setMatrix(scaleMatrix);
    // Prepend instead of append to paint the new, incomplete, scale before/behind the previous one.
    itemNode->prependChildNode(scaleNode);

    lastScale = scale;
    lastScaleNode = scaleNode;
    return lastScaleNode;
}

}
