/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TreeWalker.h"

#include "ExceptionCode.h"
#include "ContainerNode.h"
#include "NodeTraversal.h"

#include <runtime/JSCJSValueInlines.h>

namespace WebCore {

TreeWalker::TreeWalker(Node& rootNode, unsigned long whatToShow, RefPtr<NodeFilter>&& filter)
    : NodeIteratorBase(rootNode, whatToShow, WTFMove(filter))
    , m_current(root())
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

inline Node* TreeWalker::setCurrent(RefPtr<Node>&& node)
{
    m_current = node;
    return m_current.get();
}

Node* TreeWalker::parentNode()
{
    RefPtr<Node> node = m_current;
    while (node != root()) {
        node = node->parentNode();
        if (!node)
            return nullptr;
        short acceptNodeResult = acceptNode(node.get());
        if (acceptNodeResult == NodeFilter::FILTER_ACCEPT)
            return setCurrent(WTFMove(node));
    }
    return nullptr;
}

Node* TreeWalker::firstChild()
{
    for (RefPtr<Node> node = m_current->firstChild(); node; ) {
        short acceptNodeResult = acceptNode(node.get());
        switch (acceptNodeResult) {
            case NodeFilter::FILTER_ACCEPT:
                m_current = node.release();
                return m_current.get();
            case NodeFilter::FILTER_SKIP:
                if (node->firstChild()) {
                    node = node->firstChild();
                    continue;
                }
                break;
            case NodeFilter::FILTER_REJECT:
                break;
        }
        do {
            if (node->nextSibling()) {
                node = node->nextSibling();
                break;
            }
            ContainerNode* parent = node->parentNode();
            if (!parent || parent == root() || parent == m_current)
                return nullptr;
            node = parent;
        } while (node);
    }
    return nullptr;
}

Node* TreeWalker::lastChild()
{
    for (RefPtr<Node> node = m_current->lastChild(); node; ) {
        short acceptNodeResult = acceptNode(node.get());
        switch (acceptNodeResult) {
            case NodeFilter::FILTER_ACCEPT:
                m_current = node.release();
                return m_current.get();
            case NodeFilter::FILTER_SKIP:
                if (node->lastChild()) {
                    node = node->lastChild();
                    continue;
                }
                break;
            case NodeFilter::FILTER_REJECT:
                break;
        }
        do {
            if (node->previousSibling()) {
                node = node->previousSibling();
                break;
            }
            ContainerNode* parent = node->parentNode();
            if (!parent || parent == root() || parent == m_current)
                return nullptr;
            node = parent;
        } while (node);
    }
    return nullptr;
}

template<TreeWalker::SiblingTraversalType type> Node* TreeWalker::traverseSiblings()
{
    RefPtr<Node> node = m_current;
    if (node == root())
        return nullptr;

    auto isNext = type == SiblingTraversalType::Next;
    while (true) {
        for (RefPtr<Node> sibling = isNext ? node->nextSibling() : node->previousSibling(); sibling; ) {
            short acceptNodeResult = acceptNode(sibling.get());
            if (acceptNodeResult == NodeFilter::FILTER_ACCEPT) {
                m_current = WTFMove(sibling);
                return m_current.get();
            }
            node = sibling;
            sibling = isNext ? sibling->firstChild() : sibling->lastChild();
            if (acceptNodeResult == NodeFilter::FILTER_REJECT || !sibling)
                sibling = isNext ? node->nextSibling() : node->previousSibling();
        }
        node = node->parentNode();
        if (!node || node == root())
            return nullptr;
        short acceptNodeResult = acceptNode(node.get());
        if (acceptNodeResult == NodeFilter::FILTER_ACCEPT)
            return nullptr;
    }
}

Node* TreeWalker::previousSibling()
{
    return traverseSiblings<SiblingTraversalType::Previous>();
}

Node* TreeWalker::nextSibling()
{
    return traverseSiblings<SiblingTraversalType::Next>();
}

Node* TreeWalker::previousNode()
{
    RefPtr<Node> node = m_current;
    while (node != root()) {
        while (Node* previousSibling = node->previousSibling()) {
            node = previousSibling;
            short acceptNodeResult = acceptNode(node.get());
            if (acceptNodeResult == NodeFilter::FILTER_REJECT)
                continue;
            while (Node* lastChild = node->lastChild()) {
                node = lastChild;
                acceptNodeResult = acceptNode(node.get());
                if (acceptNodeResult == NodeFilter::FILTER_REJECT)
                    break;
            }
            if (acceptNodeResult == NodeFilter::FILTER_ACCEPT) {
                m_current = node.release();
                return m_current.get();
            }
        }
        if (node == root())
            return nullptr;
        ContainerNode* parent = node->parentNode();
        if (!parent)
            return nullptr;
        node = parent;
        short acceptNodeResult = acceptNode(node.get());
        if (acceptNodeResult == NodeFilter::FILTER_ACCEPT)
            return setCurrent(WTFMove(node));
    }
    return nullptr;
}

Node* TreeWalker::nextNode()
{
    RefPtr<Node> node = m_current;
Children:
    while (Node* firstChild = node->firstChild()) {
        node = firstChild;
        short acceptNodeResult = acceptNode(node.get());
        if (acceptNodeResult == NodeFilter::FILTER_ACCEPT)
            return setCurrent(WTFMove(node));
        if (acceptNodeResult == NodeFilter::FILTER_REJECT)
            break;
    }
    while (Node* nextSibling = NodeTraversal::nextSkippingChildren(*node, root())) {
        node = nextSibling;
        short acceptNodeResult = acceptNode(node.get());
        if (acceptNodeResult == NodeFilter::FILTER_ACCEPT)
            return setCurrent(WTFMove(node));
        if (acceptNodeResult == NodeFilter::FILTER_SKIP)
            goto Children;
    }
    return nullptr;
}

} // namespace WebCore
