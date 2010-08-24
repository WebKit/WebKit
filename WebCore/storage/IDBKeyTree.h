/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef IDBKeyTree_h
#define IDBKeyTree_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKey.h"
#include <wtf/AVLTree.h>
#include <wtf/Vector.h>

namespace WebCore {

template <typename ValueType>
class IDBKeyTree : public RefCounted<IDBKeyTree<ValueType> > {
public:
    static PassRefPtr<IDBKeyTree> create()
    {
        return adoptRef(new IDBKeyTree());
    }
    ~IDBKeyTree();

    ValueType* get(IDBKey* key);
    void put(IDBKey* key, ValueType* value);
    void remove(IDBKey* key);

private:
    struct TreeNode {
        RefPtr<ValueType> value;
        RefPtr<IDBKey> key;

        TreeNode* less;
        TreeNode* greater;
        int balanceFactor;
    };

    struct AVLTreeAbstractor {
        typedef TreeNode* handle;
        typedef size_t size;
        typedef IDBKey* key;

        handle get_less(handle h) { return h->less; }
        void set_less(handle h, handle lh) { h->less = lh; }
        handle get_greater(handle h) { return h->greater; }
        void set_greater(handle h, handle gh) { h->greater = gh; }
        int get_balance_factor(handle h) { return h->balanceFactor; }
        void set_balance_factor(handle h, int bf) { h->balanceFactor = bf; }

        static handle null() { return 0; }

        int compare_key_key(key va, key vb);
        int compare_key_node(key k, handle h) { return compare_key_key(k, h->key.get()); }
        int compare_node_node(handle h1, handle h2) { return compare_key_key(h1->key.get(), h2->key.get()); }
    };

    IDBKeyTree();

    typedef WTF::AVLTree<AVLTreeAbstractor> TreeType;
    TreeType m_tree;
};

template <typename ValueType>
IDBKeyTree<ValueType>::IDBKeyTree()
{
}

template <typename ValueType>
IDBKeyTree<ValueType>::~IDBKeyTree()
{
    typename TreeType::Iterator iter;
    iter.start_iter_least(m_tree);
    for (; *iter; ++iter)
        delete *iter;
    m_tree.purge();
}

template <typename ValueType>
int IDBKeyTree<ValueType>::AVLTreeAbstractor::compare_key_key(key va, key vb)
{
    if (va->type() != vb->type())
        return vb->type() - va->type();

    switch (va->type()) {
    case IDBKey::NullType:
        return 0;
    case IDBKey::NumberType:
        return vb->number() - va->number();
    case IDBKey::StringType:
        return codePointCompare(va->string(), vb->string());
    // FIXME: Handle dates.  Oh, and test this thoroughly.
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

template <typename ValueType>
ValueType* IDBKeyTree<ValueType>::get(IDBKey* key)
{
    TreeNode* node = m_tree.search(key);
    if (!node)
        return 0;
    return node->value.get();
}

template <typename ValueType>
void IDBKeyTree<ValueType>::put(IDBKey* key, ValueType* value)
{
    TreeNode* node = m_tree.search(key);
    if (!node) {
        node = leakPtr(new TreeNode());
        node->key = key;
        m_tree.insert(node);
    }
    node->value = value;
}

template <typename ValueType>
void IDBKeyTree<ValueType>::remove(IDBKey* key)
{
    TreeNode* node = m_tree.remove(key);
    if (node)
        delete node;
}

}

#endif // ENABLE(INDEXED_DATABASE)

#endif // IDBKeyTree_h
