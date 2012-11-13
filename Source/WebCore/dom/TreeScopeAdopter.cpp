/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
 */
#include "config.h"
#include "TreeScopeAdopter.h"

#include "Document.h"
#include "NodeRareData.h"
#include "ShadowRoot.h"
#include "ShadowTree.h"

namespace WebCore {

static inline ShadowTree* shadowTreeFor(Node* node)
{
    return node->isElementNode() ? toElement(node)->shadowTree() : 0;
}

void TreeScopeAdopter::moveTreeToNewScope(Node* root) const
{
    ASSERT(needsScopeChange());

    // If an element is moved from a document and then eventually back again the collection cache for
    // that element may contain stale data as changes made to it will have updated the DOMTreeVersion
    // of the document it was moved to. By increasing the DOMTreeVersion of the donating document here
    // we ensure that the collection cache will be invalidated as needed when the element is moved back.
    Document* oldDocument = m_oldScope ? m_oldScope->rootNode()->document() : 0;
    Document* newDocument = m_newScope->rootNode()->document();
    bool willMoveToNewDocument = oldDocument != newDocument;
    if (oldDocument && willMoveToNewDocument)
        oldDocument->incDOMTreeVersion();

    for (Node* node = root; node; node = node->traverseNextNode(root)) {
        NodeRareData* rareData = node->setTreeScope(newDocument == m_newScope ? 0 : m_newScope);
        if (rareData && rareData->nodeLists()) {
            rareData->nodeLists()->invalidateCaches();
            if (m_oldScope)
                m_oldScope->removeNodeListCache();
            m_newScope->addNodeListCache();
        }

        if (willMoveToNewDocument)
            moveNodeToNewDocument(node, oldDocument, newDocument);

        if (ShadowTree* tree = shadowTreeFor(node)) {
            tree->setParentTreeScope(m_newScope);
            if (willMoveToNewDocument)
                moveShadowTreeToNewDocument(tree, oldDocument, newDocument);
        }
    }
}

void TreeScopeAdopter::moveTreeToNewDocument(Node* root, Document* oldDocument, Document* newDocument) const
{
    for (Node* node = root; node; node = node->traverseNextNode(root)) {
        moveNodeToNewDocument(node, oldDocument, newDocument);
        if (ShadowTree* tree = shadowTreeFor(node))
            moveShadowTreeToNewDocument(tree, oldDocument, newDocument);
    }
}

inline void TreeScopeAdopter::moveShadowTreeToNewDocument(ShadowTree* tree, Document* oldDocument, Document* newDocument) const
{
    for (ShadowRoot* root = tree->youngestShadowRoot(); root; root = root->olderShadowRoot())
        moveTreeToNewDocument(root, oldDocument, newDocument);
}

#ifndef NDEBUG
static bool didMoveToNewDocumentWasCalled = false;
static Document* oldDocumentDidMoveToNewDocumentWasCalledWith = 0;

void TreeScopeAdopter::ensureDidMoveToNewDocumentWasCalled(Document* oldDocument)
{
    ASSERT(!didMoveToNewDocumentWasCalled);
    ASSERT_UNUSED(oldDocument, oldDocument == oldDocumentDidMoveToNewDocumentWasCalledWith);
    didMoveToNewDocumentWasCalled = true;
}
#endif

inline void TreeScopeAdopter::moveNodeToNewDocument(Node* node, Document* oldDocument, Document* newDocument) const
{
    ASSERT(!node->inDocument() || oldDocument != newDocument);

    newDocument->guardRef();
    if (oldDocument)
        oldDocument->moveNodeIteratorsToNewDocument(node, newDocument);

    node->setDocument(newDocument);

#ifndef NDEBUG
    didMoveToNewDocumentWasCalled = false;
    oldDocumentDidMoveToNewDocumentWasCalledWith = oldDocument;
#endif

    node->didMoveToNewDocument(oldDocument);
    ASSERT(didMoveToNewDocumentWasCalled);
    
    if (oldDocument)
        oldDocument->guardDeref();
}

}
