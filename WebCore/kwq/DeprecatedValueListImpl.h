/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef KWQVALUELIST_H_
#define KWQVALUELIST_H_

#include "KWQDef.h"
#include <kxmlcore/RefPtr.h>

class DeprecatedValueListImplNode;

class DeprecatedValueListImplIterator
{
public: 
    DeprecatedValueListImplIterator();
    
    bool operator==(const DeprecatedValueListImplIterator &other);
    bool operator!=(const DeprecatedValueListImplIterator &other);

    DeprecatedValueListImplNode *node();
    const DeprecatedValueListImplNode *node() const;

    DeprecatedValueListImplIterator& operator++();
    DeprecatedValueListImplIterator operator++(int);
    DeprecatedValueListImplIterator& operator--();

private:
    DeprecatedValueListImplIterator(const DeprecatedValueListImplNode *n);

    DeprecatedValueListImplNode *nodeImpl;

    friend class DeprecatedValueListImpl;
};

class DeprecatedValueListImpl 
{
public:
    DeprecatedValueListImpl(void (*deleteFunc)(DeprecatedValueListImplNode *), DeprecatedValueListImplNode *(*copyNode)(DeprecatedValueListImplNode *));
    ~DeprecatedValueListImpl();
    
    DeprecatedValueListImpl(const DeprecatedValueListImpl&);
    DeprecatedValueListImpl& operator=(const DeprecatedValueListImpl&);
        
    void clear();
    unsigned count() const;
    bool isEmpty() const;

    DeprecatedValueListImplIterator appendNode(DeprecatedValueListImplNode *node);
    DeprecatedValueListImplIterator prependNode(DeprecatedValueListImplNode *node);
    void removeEqualNodes(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *));
    unsigned containsEqualNodes(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const;
    
    DeprecatedValueListImplIterator findEqualNode(DeprecatedValueListImplNode *node, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const;

    DeprecatedValueListImplIterator insert(const DeprecatedValueListImplIterator &iterator, DeprecatedValueListImplNode* node);
    DeprecatedValueListImplIterator removeIterator(DeprecatedValueListImplIterator &iterator);
    DeprecatedValueListImplIterator fromLast();

    DeprecatedValueListImplNode *firstNode();
    DeprecatedValueListImplNode *lastNode();

    DeprecatedValueListImplNode *firstNode() const;
    DeprecatedValueListImplNode *lastNode() const;

    DeprecatedValueListImplIterator begin();
    DeprecatedValueListImplIterator end();

    DeprecatedValueListImplIterator begin() const;
    DeprecatedValueListImplIterator end() const;
    DeprecatedValueListImplIterator fromLast() const;
    
    DeprecatedValueListImplNode *nodeAt(unsigned index);
    DeprecatedValueListImplNode *nodeAt(unsigned index) const;
    
    bool isEqual(const DeprecatedValueListImpl &other, bool (*equalFunc)(const DeprecatedValueListImplNode *, const DeprecatedValueListImplNode *)) const;
    
private:
    void copyOnWrite();

    class KWQValueListPrivate;

    RefPtr<KWQValueListPrivate> d;
    
    friend class DeprecatedValueListImplNode;
};

class DeprecatedValueListImplNode
{
protected:
    DeprecatedValueListImplNode();

private:
    DeprecatedValueListImplNode *prev;
    DeprecatedValueListImplNode *next;

    friend class DeprecatedValueListImpl;
    friend class DeprecatedValueListImplIterator;
    friend class DeprecatedValueListImpl::KWQValueListPrivate;
};

inline DeprecatedValueListImplIterator::DeprecatedValueListImplIterator() : 
    nodeImpl(NULL)
{
}

inline bool DeprecatedValueListImplIterator::operator==(const DeprecatedValueListImplIterator &other)
{
    return nodeImpl == other.nodeImpl;
}

inline bool DeprecatedValueListImplIterator::operator!=(const DeprecatedValueListImplIterator &other)
{
    return nodeImpl != other.nodeImpl;
}

inline DeprecatedValueListImplNode *DeprecatedValueListImplIterator::node()
{
    return nodeImpl;
}

inline const DeprecatedValueListImplNode *DeprecatedValueListImplIterator::node() const
{
    return nodeImpl;
}

inline DeprecatedValueListImplIterator& DeprecatedValueListImplIterator::operator++()
{
    if (nodeImpl != NULL) {
        nodeImpl = nodeImpl->next;
    }
    return *this;
}

inline DeprecatedValueListImplIterator DeprecatedValueListImplIterator::operator++(int)
{
    DeprecatedValueListImplIterator tmp(*this);

    if (nodeImpl != NULL) {
        nodeImpl = nodeImpl->next;
    }

    return tmp;
}

inline DeprecatedValueListImplIterator& DeprecatedValueListImplIterator::operator--()
{
    if (nodeImpl != NULL) {
        nodeImpl = nodeImpl->prev;
    }
    return *this;
}

inline DeprecatedValueListImplIterator::DeprecatedValueListImplIterator(const DeprecatedValueListImplNode *n) :
    nodeImpl((DeprecatedValueListImplNode *)n)
{
}

inline DeprecatedValueListImplNode::DeprecatedValueListImplNode() : 
    prev(NULL), 
    next(NULL)
{
}

#endif
