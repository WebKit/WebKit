/*
 * Copyright (C) 2007, 2015 Apple Inc. All rights reserved.
 *           (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#pragma once

#include "Document.h"
#include "ElementIterator.h"
#include "Frame.h"
#include "NodeTraversal.h"
#include <wtf/Assertions.h>
#include <wtf/Ref.h>

namespace WebCore {

void notifyChildNodeInserted(ContainerNode& insertionPoint, Node&, NodeVector& postInsertionNotificationTargets);
void notifyChildNodeRemoved(ContainerNode& insertionPoint, Node&);
void removeDetachedChildrenInContainer(ContainerNode&);

enum SubframeDisconnectPolicy {
    RootAndDescendants,
    DescendantsOnly
};
void disconnectSubframes(ContainerNode& root, SubframeDisconnectPolicy);

inline void disconnectSubframesIfNeeded(ContainerNode& root, SubframeDisconnectPolicy policy)
{
    if (!root.connectedSubframeCount())
        return;
    disconnectSubframes(root, policy);
}

} // namespace WebCore
