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
#include "SGAgent.h"

#include "PassOwnPtr.h"
#include "SGTileNode.h"
#include <QSGEngine>
#include <QSGItem>
#include <QSGTexture>

namespace WebKit {

struct NodeUpdateCreateTile : public NodeUpdate {
    NodeUpdateCreateTile(int nodeID, int parentNodeID)
        : NodeUpdate(CreateTile)
        , nodeID(nodeID)
        , parentNodeID(parentNodeID)
    { }
    int nodeID;
    int parentNodeID;
};

struct NodeUpdateCreateScale : public NodeUpdate {
    NodeUpdateCreateScale(int nodeID, int parentNodeID, float scale)
        : NodeUpdate(CreateScale)
        , nodeID(nodeID)
        , parentNodeID(parentNodeID)
        , scale(scale)
    { }
    int nodeID;
    int parentNodeID;
    float scale;
};

struct NodeUpdateRemove : public NodeUpdate {
    NodeUpdateRemove(int nodeID)
        : NodeUpdate(Remove)
        , nodeID(nodeID)
    { }
    int nodeID;
};

struct NodeUpdateSetTexture : public NodeUpdate {
    NodeUpdateSetTexture(int nodeID, const QImage& texture, const QRect& sourceRect, const QRect& targetRect)
        : NodeUpdate(SetTexture)
        , nodeID(nodeID)
        , texture(texture)
        , sourceRect(sourceRect)
        , targetRect(targetRect)
    { }
    int nodeID;
    QImage texture;
    QRect sourceRect;
    QRect targetRect;
};


SGAgent::SGAgent(QSGItem* item)
    : item(item)
    , nextNodeID(1)
{
}

int SGAgent::createTileNode(int parentNodeID)
{
    int nodeID = nextNodeID++;
    nodeUpdatesQueue.append(adoptPtr(new NodeUpdateCreateTile(nodeID, parentNodeID)));
    item->update();
    return nodeID;
}

int SGAgent::createScaleNode(int parentNodeID, float scale)
{
    int nodeID = nextNodeID++;
    nodeUpdatesQueue.append(adoptPtr(new NodeUpdateCreateScale(nodeID, parentNodeID, scale)));
    item->update();
    return nodeID;
}

void SGAgent::removeNode(int nodeID)
{
    nodeUpdatesQueue.append(adoptPtr(new NodeUpdateRemove(nodeID)));
    item->update();
}

void SGAgent::setNodeTexture(int nodeID, const QImage& texture, const QRect& sourceRect, const QRect& targetRect)
{
    nodeUpdatesQueue.append(adoptPtr(new NodeUpdateSetTexture(nodeID, texture, sourceRect, targetRect)));
    item->update();
}

void SGAgent::updatePaintNode(QSGNode* itemNode)
{
    while (!nodeUpdatesQueue.isEmpty()) {
        OwnPtr<NodeUpdate> nodeUpdate(nodeUpdatesQueue.takeFirst());
        switch (nodeUpdate->type) {
        case NodeUpdate::CreateTile: {
            NodeUpdateCreateTile* createTileUpdate = static_cast<NodeUpdateCreateTile*>(nodeUpdate.get());
            SGTileNode* tileNode = new SGTileNode;
            QSGNode* parentNode = createTileUpdate->parentNodeID ? nodes.get(createTileUpdate->parentNodeID) : itemNode;
            parentNode->prependChildNode(tileNode);
            nodes.set(createTileUpdate->nodeID, tileNode);
            break;
        }
        case NodeUpdate::CreateScale: {
            NodeUpdateCreateScale* createScaleUpdate = static_cast<NodeUpdateCreateScale*>(nodeUpdate.get());
            QSGTransformNode* scaleNode = new QSGTransformNode;
            QMatrix4x4 scaleMatrix;
            // Use scale(float,float) to prevent scaling the Z component.
            scaleMatrix.scale(createScaleUpdate->scale, createScaleUpdate->scale);
            scaleNode->setMatrix(scaleMatrix);
            QSGNode* parentNode = createScaleUpdate->parentNodeID ? nodes.get(createScaleUpdate->parentNodeID) : itemNode;
            // Prepend instead of append to paint the new, incomplete, tileset before/behind the previous one.
            parentNode->prependChildNode(scaleNode);
            nodes.set(createScaleUpdate->nodeID, scaleNode);
            break;
        }
        case NodeUpdate::Remove: {
            NodeUpdateRemove* removeUpdate = static_cast<NodeUpdateRemove*>(nodeUpdate.get());
            delete nodes.take(removeUpdate->nodeID);
            break;
        }
        case NodeUpdate::SetTexture: {
            NodeUpdateSetTexture* setTextureUpdate = static_cast<NodeUpdateSetTexture*>(nodeUpdate.get());
            SGTileNode* tileNode = static_cast<SGTileNode*>(nodes.get(setTextureUpdate->nodeID));
            if (tileNode) {
                QSGTexture* texture = item->sceneGraphEngine()->createTextureFromImage(setTextureUpdate->texture);
                tileNode->setTexture(texture);
                tileNode->setTargetRect(setTextureUpdate->targetRect);
                tileNode->setSourceRect(texture->convertToNormalizedSourceRect(setTextureUpdate->sourceRect));
            }
            break;
        }
        default:
            ASSERT_NOT_REACHED();
        }
    }
}

}
