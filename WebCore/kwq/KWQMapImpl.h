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

#ifndef KWQ_MAP_IMPL_H
#define KWQ_MAP_IMPL_H

#include <new>

#include "KWQRefPtr.h"
#include "kxmlcore/FastMalloc.h"

class KWQMapImpl;

class KWQMapNodeImpl : public FastAllocated
{
protected:
    typedef enum { Red = 0, Black = 1 } KWQMapNodeColor;

    KWQMapNodeImpl();

    KWQMapNodeImpl *left();
    const KWQMapNodeImpl *left() const;
    KWQMapNodeImpl *right();
    const KWQMapNodeImpl *right() const;
    KWQMapNodeImpl *predecessor();
    const KWQMapNodeImpl *predecessor() const;
    KWQMapNodeImpl *successor();
    const KWQMapNodeImpl *successor() const;

    KWQMapNodeImpl *prev;
    KWQMapNodeImpl *next;
    bool prevIsChild;
    bool nextIsChild;
    KWQMapNodeColor color;

    friend class KWQMapImpl;
    friend class KWQMapIteratorImpl;

#ifdef QMAP_TESTING
    friend bool CheckRedBlackRules(KWQMapImpl *impl);
    friend bool CheckRedBlackRulesAtNode(KWQMapImpl *impl, KWQMapNodeImpl *node, int &black_height);
#endif
};

class KWQMapIteratorImpl : public FastAllocated {
protected:
    KWQMapNodeImpl *node;

    KWQMapIteratorImpl();
    void incrementInternal();
};

class KWQMapImpl {
 protected:
    typedef enum { Less = -1, Equal = 0, Greater = 1 } CompareResult;

    KWQMapImpl(KWQMapNodeImpl *guard, void (*deleteNode)(KWQMapNodeImpl *));
    virtual ~KWQMapImpl();

    KWQMapImpl(const KWQMapImpl &);
    KWQMapImpl &operator=(const KWQMapImpl &);

    KWQMapNodeImpl *findInternal(KWQMapNodeImpl *target) const;
    KWQMapNodeImpl *insertInternal(KWQMapNodeImpl *nodeToInsert, bool replaceExisting);
    void removeEqualInternal(KWQMapNodeImpl *nodeToRemove, bool samePointer = false);
    uint countInternal() const;
    void clearInternal();
    void swap(KWQMapImpl &map);
    const KWQMapNodeImpl *beginInternal() const;
    const KWQMapNodeImpl *endInternal() const;
    KWQMapNodeImpl *beginInternal();
    KWQMapNodeImpl *endInternal();

    virtual CompareResult compareNodes(const KWQMapNodeImpl *a, const KWQMapNodeImpl *b) const = 0;
    virtual void copyNode(const KWQMapNodeImpl *src, KWQMapNodeImpl *dst) const = 0;
    virtual KWQMapNodeImpl *duplicateNode(const KWQMapNodeImpl *src) const = 0;
    virtual void swapNodes(KWQMapNodeImpl *a, KWQMapNodeImpl *b) const = 0;

 private:
    // can't possibly have a bigger height than this in a balanced tree
    static const int MAX_STACK = 64;

    void copyOnWrite();
    KWQMapNodeImpl *copyTree(const KWQMapNodeImpl *node, 
			     KWQMapNodeImpl *subtreePredecessor, 
			     KWQMapNodeImpl *subtreeSuccessor) const;
    void rebalanceAfterInsert(KWQMapNodeImpl **nodes, bool *wentLeft, int height);
    void rebalanceAfterRemove(KWQMapNodeImpl *nodeToRemove, KWQMapNodeImpl **nodes, bool *wentLeft, int height);
    void rotateRight(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent);
    void rotateLeft(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent);

    class KWQMapPrivate;

    KWQRefPtr<KWQMapPrivate> d;

#ifdef QMAP_TESTING
    friend bool CheckRedBlackRules(KWQMapImpl *impl);
    friend bool CheckRedBlackRulesAtNode(KWQMapImpl *impl, KWQMapNodeImpl *node, int &black_height);
#endif
};

#endif
