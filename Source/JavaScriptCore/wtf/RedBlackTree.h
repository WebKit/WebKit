/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RedBlackTree_h
#define RedBlackTree_h

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>

namespace WTF {

// This implements a red-black tree with the following properties:
// - The allocation of nodes in the tree is entirely up to the user.
// - If you are in possession of a pointer to a node, you can delete
//   it from the tree. The tree will subsequently no longer have a
//   reference to this node.
// - The key type must implement operator< and ==.

template<typename KeyType, typename ValueType>
class RedBlackTree {
    WTF_MAKE_NONCOPYABLE(RedBlackTree);
private:
    enum Color {
        Red = 1,
        Black
    };
    
public:
    class Node {
        friend class RedBlackTree;
        
    public:
        Node(KeyType key, ValueType value)
            : m_key(key)
            , m_value(value)
        {
        }
        
        const Node* successor() const
        {
            const Node* x = this;
            if (x->right())
                return treeMinimum(x->right());
            const Node* y = x->parent();
            while (y && x == y->right()) {
                x = y;
                y = y->parent();
            }
            return y;
        }
        
        const Node* predecessor() const
        {
            const Node* x = this;
            if (x->left())
                return treeMaximum(x->left());
            const Node* y = x->parent();
            while (y && x == y->left()) {
                x = y;
                y = y->parent();
            }
            return y;
        }
        
        Node* successor()
        {
            return const_cast<Node*>(const_cast<const Node*>(this)->successor());
        }
    
        Node* predecessor()
        {
            return const_cast<Node*>(const_cast<const Node*>(this)->predecessor());
        }
    
        KeyType m_key;
        ValueType m_value;

    private:
        void reset()
        {
            m_left = 0;
            m_right = 0;
            m_parentAndRed = 1; // initialize to red
        }
        
        // NOTE: these methods should pack the parent and red into a single
        // word. But doing so appears to reveal a bug in the compiler.
        Node* parent() const
        {
            return reinterpret_cast<Node*>(m_parentAndRed & ~static_cast<uintptr_t>(1));
        }
        
        void setParent(Node* newParent)
        {
            m_parentAndRed = reinterpret_cast<uintptr_t>(newParent) | (m_parentAndRed & 1);
        }
        
        Node* left() const
        {
            return m_left;
        }
        
        void setLeft(Node* node)
        {
            m_left = node;
        }
        
        Node* right() const
        {
            return m_right;
        }
        
        void setRight(Node* node)
        {
            m_right = node;
        }
        
        Color color() const
        {
            if (m_parentAndRed & 1)
                return Red;
            return Black;
        }
        
        void setColor(Color value)
        {
            if (value == Red)
                m_parentAndRed |= 1;
            else
                m_parentAndRed &= ~static_cast<uintptr_t>(1);
        }
        
        Node* m_left;
        Node* m_right;
        uintptr_t m_parentAndRed;
    };
    
    RedBlackTree()
        : m_root(0)
    {
    }
    
    void insert(Node* x)
    {
        x->reset();
        treeInsert(x);
        x->setColor(Red);

        while (x != m_root && x->parent()->color() == Red) {
            if (x->parent() == x->parent()->parent()->left()) {
                Node* y = x->parent()->parent()->right();
                if (y && y->color() == Red) {
                    // Case 1
                    x->parent()->setColor(Black);
                    y->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    x = x->parent()->parent();
                } else {
                    if (x == x->parent()->right()) {
                        // Case 2
                        x = x->parent();
                        leftRotate(x);
                    }
                    // Case 3
                    x->parent()->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    rightRotate(x->parent()->parent());
                }
            } else {
                // Same as "then" clause with "right" and "left" exchanged.
                Node* y = x->parent()->parent()->left();
                if (y && y->color() == Red) {
                    // Case 1
                    x->parent()->setColor(Black);
                    y->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    x = x->parent()->parent();
                } else {
                    if (x == x->parent()->left()) {
                        // Case 2
                        x = x->parent();
                        rightRotate(x);
                    }
                    // Case 3
                    x->parent()->setColor(Black);
                    x->parent()->parent()->setColor(Red);
                    leftRotate(x->parent()->parent());
                }
            }
        }

        m_root->setColor(Black);
    }

    Node* remove(Node* z)
    {
        ASSERT(z);
        ASSERT(z->parent() || z == m_root);
        
        // Y is the node to be unlinked from the tree.
        Node* y;
        if (!z->left() || !z->right())
            y = z;
        else
            y = z->successor();

        // Y is guaranteed to be non-null at this point.
        Node* x;
        if (y->left())
            x = y->left();
        else
            x = y->right();

        // X is the child of y which might potentially replace y in
        // the tree. X might be null at this point.
        Node* xParent;
        if (x) {
            x->setParent(y->parent());
            xParent = x->parent();
        } else
            xParent = y->parent();
        if (!y->parent())
            m_root = x;
        else {
            if (y == y->parent()->left())
                y->parent()->setLeft(x);
            else
                y->parent()->setRight(x);
        }
            
        if (y != z) {
            if (y->color() == Black)
                removeFixup(x, xParent);
            
            y->setParent(z->parent());
            y->setColor(z->color());
            y->setLeft(z->left());
            y->setRight(z->right());
            
            if (z->left())
                z->left()->setParent(y);
            if (z->right())
                z->right()->setParent(y);
            if (z->parent()) {
                if (z->parent()->left() == z)
                    z->parent()->setLeft(y);
                else
                    z->parent()->setRight(y);
            } else {
                ASSERT(m_root == z);
                m_root = y;
            }
        } else if (y->color() == Black)
            removeFixup(x, xParent);

        return z;
    }
    
    Node* remove(const KeyType& key)
    {
        Node* result = findExact(key);
        if (!result)
            return 0;
        return remove(result);
    }
    
    Node* findExact(const KeyType& key) const
    {
        for (Node* current = m_root; current;) {
            if (current->m_key == key)
                return current;
            if (key < current->m_key)
                current = current->left();
            else
                current = current->right();
        }
        return 0;
    }
    
    Node* findLeastGreaterThanOrEqual(const KeyType& key) const
    {
        Node* best = 0;
        for (Node* current = m_root; current;) {
            if (current->m_key == key)
                return current;
            if (current->m_key < key)
                current = current->right();
            else {
                best = current;
                current = current->left();
            }
        }
        return best;
    }
    
    Node* findGreatestLessThanOrEqual(const KeyType& key) const
    {
        Node* best = 0;
        for (Node* current = m_root; current;) {
            if (current->m_key == key)
                return current;
            if (current->m_key > key)
                current = current->left();
            else {
                best = current;
                current = current->right();
            }
        }
        return best;
    }
    
    Node* first() const
    {
        if (!m_root)
            return 0;
        return treeMinimum(m_root);
    }
    
    Node* last() const
    {
        if (!m_root)
            return 0;
        return treeMaximum(m_root);
    }
    
    // This is an O(n) operation.
    size_t size()
    {
        size_t result = 0;
        for (Node* current = first(); current; current = current->successor())
            result++;
        return result;
    }
    
    // This is an O(1) operation.
    bool isEmpty()
    {
        return !m_root;
    }
    
private:
    // Finds the minimum element in the sub-tree rooted at the given
    // node.
    static Node* treeMinimum(Node* x)
    {
        while (x->left())
            x = x->left();
        return x;
    }
    
    static Node* treeMaximum(Node* x)
    {
        while (x->right())
            x = x->right();
        return x;
    }

    static const Node* treeMinimum(const Node* x)
    {
        while (x->left())
            x = x->left();
        return x;
    }
    
    static const Node* treeMaximum(const Node* x)
    {
        while (x->right())
            x = x->right();
        return x;
    }

    void treeInsert(Node* z)
    {
        ASSERT(!z->left());
        ASSERT(!z->right());
        ASSERT(!z->parent());
        ASSERT(z->color() == Red);
        
        Node* y = 0;
        Node* x = m_root;
        while (x) {
            y = x;
            if (z->m_key < x->m_key)
                x = x->left();
            else
                x = x->right();
        }
        z->setParent(y);
        if (!y)
            m_root = z;
        else {
            if (z->m_key < y->m_key)
                y->setLeft(z);
            else
                y->setRight(z);
        }
    }

    //----------------------------------------------------------------------
    // Red-Black tree operations
    //

    // Left-rotates the subtree rooted at x.
    // Returns the new root of the subtree (x's right child).
    Node* leftRotate(Node* x)
    {
        // Set y.
        Node* y = x->right();

        // Turn y's left subtree into x's right subtree.
        x->setRight(y->left());
        if (y->left())
            y->left()->setParent(x);

        // Link x's parent to y.
        y->setParent(x->parent());
        if (!x->parent())
            m_root = y;
        else {
            if (x == x->parent()->left())
                x->parent()->setLeft(y);
            else
                x->parent()->setRight(y);
        }

        // Put x on y's left.
        y->setLeft(x);
        x->setParent(y);

        return y;
    }

    // Right-rotates the subtree rooted at y.
    // Returns the new root of the subtree (y's left child).
    Node* rightRotate(Node* y)
    {
        // Set x.
        Node* x = y->left();

        // Turn x's right subtree into y's left subtree.
        y->setLeft(x->right());
        if (x->right())
            x->right()->setParent(y);

        // Link y's parent to x.
        x->setParent(y->parent());
        if (!y->parent())
            m_root = x;
        else {
            if (y == y->parent()->left())
                y->parent()->setLeft(x);
            else
                y->parent()->setRight(x);
        }

        // Put y on x's right.
        x->setRight(y);
        y->setParent(x);

        return x;
    }

    // Restores the red-black property to the tree after splicing out
    // a node. Note that x may be null, which is why xParent must be
    // supplied.
    void removeFixup(Node* x, Node* xParent)
    {
        while (x != m_root && (!x || x->color() == Black)) {
            if (x == xParent->left()) {
                // Note: the text points out that w can not be null.
                // The reason is not obvious from simply looking at
                // the code; it comes about from the properties of the
                // red-black tree.
                Node* w = xParent->right();
                ASSERT(w); // x's sibling should not be null.
                if (w->color() == Red) {
                    // Case 1
                    w->setColor(Black);
                    xParent->setColor(Red);
                    leftRotate(xParent);
                    w = xParent->right();
                }
                if ((!w->left() || w->left()->color() == Black)
                    && (!w->right() || w->right()->color() == Black)) {
                    // Case 2
                    w->setColor(Red);
                    x = xParent;
                    xParent = x->parent();
                } else {
                    if (!w->right() || w->right()->color() == Black) {
                        // Case 3
                        w->left()->setColor(Black);
                        w->setColor(Red);
                        rightRotate(w);
                        w = xParent->right();
                    }
                    // Case 4
                    w->setColor(xParent->color());
                    xParent->setColor(Black);
                    if (w->right())
                        w->right()->setColor(Black);
                    leftRotate(xParent);
                    x = m_root;
                    xParent = x->parent();
                }
            } else {
                // Same as "then" clause with "right" and "left"
                // exchanged.

                // Note: the text points out that w can not be null.
                // The reason is not obvious from simply looking at
                // the code; it comes about from the properties of the
                // red-black tree.
                Node* w = xParent->left();
                ASSERT(w); // x's sibling should not be null.
                if (w->color() == Red) {
                    // Case 1
                    w->setColor(Black);
                    xParent->setColor(Red);
                    rightRotate(xParent);
                    w = xParent->left();
                }
                if ((!w->right() || w->right()->color() == Black)
                    && (!w->left() || w->left()->color() == Black)) {
                    // Case 2
                    w->setColor(Red);
                    x = xParent;
                    xParent = x->parent();
                } else {
                    if (!w->left() || w->left()->color() == Black) {
                        // Case 3
                        w->right()->setColor(Black);
                        w->setColor(Red);
                        leftRotate(w);
                        w = xParent->left();
                    }
                    // Case 4
                    w->setColor(xParent->color());
                    xParent->setColor(Black);
                    if (w->left())
                        w->left()->setColor(Black);
                    rightRotate(xParent);
                    x = m_root;
                    xParent = x->parent();
                }
            }
        }
        if (x)
            x->setColor(Black);
    }

    Node* m_root;
};

}

#endif

