/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "TreeWalker.h"

#include "ExceptionCode.h"
#include "Node.h"
#include "NodeFilter.h"
#include <wtf/PassRefPtr.h>

namespace WebCore {

TreeWalker::TreeWalker(Node* rootNode, unsigned whatToShow, PassRefPtr<NodeFilter> filter, bool expandEntityReferences)
    : Traversal(rootNode, whatToShow, filter, expandEntityReferences)
    , m_current(rootNode)
{
}

void TreeWalker::setCurrentNode(Node* node, ExceptionCode& ec)
{
    if (!node) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    m_current = node;
}

void TreeWalker::setCurrentNode(Node* node)
{
    ASSERT(node);
    int dummy;
    setCurrentNode(node, dummy);
}

Node* TreeWalker::parentNode()
{
    Node* result = 0;
    for (Node* node = currentNode()->parentNode(); node && node != root(); node = node->parentNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::firstChild()
{
    Node* result = 0;
    for (Node* node = currentNode()->firstChild(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::lastChild()
{
    Node* result = 0;
    for (Node* node = currentNode()->lastChild(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::previousSibling()
{
    Node* result = 0;
    for (Node* node = currentNode()->previousSibling(); node; node = node->previousSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::nextSibling()
{
    Node* result = 0;
    for (Node* node = currentNode()->nextSibling(); node; node = node->nextSibling()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::previousNode()
{
    Node* result = 0;
    for (Node* node = currentNode()->traversePreviousNode(); node; node = node->traversePreviousNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

Node* TreeWalker::nextNode()
{
    Node* result = 0;
    for (Node* node = currentNode()->traverseNextNode(); node; node = node->traverseNextNode()) {
        if (acceptNode(node) == NodeFilter::FILTER_ACCEPT && !ancestorRejected(node)) {
            setCurrentNode(node);
            result = node;
            break;
        }
    }
    return result;
}

bool TreeWalker::ancestorRejected(const Node* node) const
{
    for (Node* a = node->parentNode(); a && a != root(); a = a->parentNode()) {
        if (acceptNode(a) == NodeFilter::FILTER_REJECT)
            return true;
    }
    return false;
}

} // namespace WebCore
