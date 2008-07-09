/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HeavyProfile.h"

#include "TreeProfile.h"

namespace KJS {

HeavyProfile::HeavyProfile(TreeProfile* treeProfile)
    : Profile(treeProfile->title())
{
    // FIXME: Find a way to calculate time for a HeavyProfile.
    // We will need to copy the time information from the m_treeProfile's head to our m_head.
    m_treeProfile = treeProfile;
    generateHeavyStructure();
}

void HeavyProfile::generateHeavyStructure()
{
    ProfileNode* treeHead = m_treeProfile->callTree();
    ProfileNode* currentNode = treeHead->firstChild();
    for (ProfileNode* nextNode = currentNode; nextNode; nextNode = nextNode->firstChild())
        currentNode = nextNode;

    // For each node
    HashMap<CallIdentifier, ProfileNode*> foundChildren;
    while (currentNode && currentNode != treeHead) {
        ProfileNode* child = foundChildren.get(currentNode->callIdentifier());
        if (child) // currentNode is in the set already
            mergeProfiles(child, currentNode);
        else { // currentNode is not in the set
            child = addNode(currentNode);
            foundChildren.set(currentNode->callIdentifier(), child);
        }

        currentNode = currentNode->traverseNextNodePostOrder();
    }
}

ProfileNode* HeavyProfile::addNode(ProfileNode* currentNode)
{
    RefPtr<ProfileNode> node = ProfileNode::create(m_head.get(), currentNode);
    m_head->addChild(node);

    addAncestorsAsChildren(currentNode->parent(), node.get());
    return node.get();
}

void HeavyProfile::mergeProfiles(ProfileNode* heavyProfileHead, ProfileNode* treeProfileHead)
{
    ASSERT_ARG(heavyProfileHead, heavyProfileHead);
    ASSERT_ARG(treeProfileHead, treeProfileHead);

    ProfileNode* currentHeavyNode = heavyProfileHead;
    ProfileNode* currentTreeNode = treeProfileHead;
    ProfileNode* previousHeavyNode;
    
    while (currentHeavyNode) {
        previousHeavyNode = currentHeavyNode;

        currentTreeNode = currentTreeNode->parent();
        currentHeavyNode = currentHeavyNode->findChild(currentTreeNode);
        // FIXME: Find a way to calculate time for a HeavyProfile.
        // We must sum self & total time, but maybe only for the leaf nodes.
    }

    // If currentTreeNode is null then we already have the whole tree we wanted to copy.
    // If not we need to copy the subset of the tree that remains different between the two.
    if (currentTreeNode)
        addAncestorsAsChildren(currentTreeNode, previousHeavyNode);
}

void HeavyProfile::addAncestorsAsChildren(ProfileNode* getFrom, ProfileNode* addTo)
{
    ASSERT_ARG(getFrom, getFrom);
    ASSERT_ARG(addTo, addTo);

    if (!getFrom->head())
        return;

    RefPtr<ProfileNode> currentNode = addTo;
    for (ProfileNode* treeAncestor = getFrom; treeAncestor && treeAncestor != getFrom->head(); treeAncestor = treeAncestor->parent()) {
        RefPtr<ProfileNode> newChild = ProfileNode::create(currentNode->head(), treeAncestor);
        currentNode->addChild(newChild);
        currentNode = newChild.release();
    }
}

}   // namespace KJS
