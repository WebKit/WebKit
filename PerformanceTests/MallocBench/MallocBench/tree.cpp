/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "tree.h"
#include <limits>
#include <stdlib.h>
#include <strings.h>

#include "mbmalloc.h"

namespace {

struct Node {
    void* operator new(size_t size)
    {
        return mbmalloc(size);
    }

    void operator delete(void* p, size_t size)
    {
        mbfree(p, size);
    }

    Node(Node* left, Node* right, size_t payloadSize, size_t id)
        : m_refCount(1)
        , m_left(left)
        , m_right(right)
        , m_payload(static_cast<char*>(mbmalloc(payloadSize)))
        , m_payloadSize(payloadSize)
        , m_id(id)
    {
        if (m_left)
            m_left->ref();
        if (m_right)
            m_right->ref();
        bzero(m_payload, payloadSize);
    }

    ~Node()
    {
        if (m_left)
            m_left->deref();
        if (m_right)
            m_right->deref();
        mbfree(m_payload, m_payloadSize);
    }

    void ref()
    {
        ++m_refCount;
    }

    void deref()
    {
        if (m_refCount == 1)
            delete this;
        else
            --m_refCount;
    }
    
    size_t id() { return m_id; }
    Node* left() { return m_left; }
    Node* right() { return m_right; }

    void setLeft(Node* left)
    {
        left->ref();
        if (m_left)
            m_left->deref();
        
        m_left = left;
    }

    void setRight(Node* right)
    {
        right->ref();
        if (m_right)
            m_right->deref();
        
        m_right = right;
    }

    unsigned m_refCount;
    Node* m_left;
    Node* m_right;
    char* m_payload;
    size_t m_payloadSize;
    size_t m_id;
};

void verify(Node* node, Node* left, Node* right)
{
    if (left && left->id() >= node->id())
        abort();

    if (right && right->id() <= node->id())
        abort();
}

Node* createTree(size_t depth, size_t& counter)
{
    if (!depth)
        return 0;

    Node* left = createTree(depth - 1, counter);
    size_t id = counter++;
    Node* right = createTree(depth - 1, counter);

    Node* result = new Node(left, right, ((depth * 8) & (64 - 1)) | 1, id);

    verify(result, left, right);

    if (left)
        left->deref();
    if (right)
        right->deref();
    return result;
}

Node* createTree(size_t depth)
{
    size_t counter = 0;
    return createTree(depth, counter);
}

void churnTree(Node* node, size_t stride, size_t& counter)
{
    if (!node)
        return;
    
    churnTree(node->left(), stride, counter);

    if (node->left() && !(counter % stride)) {
        Node* left = new Node(node->left()->left(), node->left()->right(), (counter & (64 - 1)) | 1, node->left()->id());
        Node* right = new Node(node->right()->left(), node->right()->right(), (counter & (64 - 1)) | 1, node->right()->id());
        node->setLeft(left);
        node->setRight(right);
        left->deref();
        right->deref();
    }
    ++counter;

    churnTree(node->right(), stride, counter);

    verify(node, node->left(), node->right());
}

void churnTree(Node* tree, size_t stride)
{
    size_t counter;
    churnTree(tree, stride, counter);
}

} // namespace

void benchmark_tree_allocate(bool isParallel)
{
    size_t times = 24;
    size_t depth = 16;
    if (isParallel) {
        times *= 4;
        depth = 13;
    }

    for (size_t time = 0; time < times; ++time) {
        Node* tree = createTree(depth);
        tree->deref();
    }
}

void benchmark_tree_traverse(bool isParallel)
{
    size_t times = 256;
    size_t depth = 15;
    if (isParallel) {
        times = 512;
        depth = 13;
    }

    Node* tree = createTree(depth);
    for (size_t time = 0; time < times; ++time)
        churnTree(tree, std::numeric_limits<size_t>::max()); // Reuse this to iterate and validate.
    tree->deref();
}

void benchmark_tree_churn(bool isParallel)
{
    size_t times = 160;
    size_t depth = 15;
    if (isParallel) {
        times *= 4;
        depth = 12;
    }

    Node* tree = createTree(depth);
    for (size_t time = 0; time < times; ++time)
        churnTree(tree, 8);
    tree->deref();
}
