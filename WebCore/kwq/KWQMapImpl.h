/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#include <config.h>

#include <new>
#include <iostream>

#ifndef USING_BORROWED_QMAP


class KWQMapNodeImpl
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

class KWQMapIteratorImpl {
protected:
    KWQMapNodeImpl *node;

    KWQMapIteratorImpl();
    KWQMapIteratorImpl(const KWQMapIteratorImpl& iter);
    void incrementInternal();
};

class KWQMapImpl {
 private:
    // disallow default construction, copy construction and assignment
    KWQMapImpl();
    KWQMapImpl(const KWQMapImpl &);
    KWQMapImpl &operator=(const KWQMapImpl &);

 protected:
    typedef enum { Less = -1, Equal = 0, Greater = 1 } CompareResult;

    KWQMapImpl(KWQMapNodeImpl *node, uint count);

    KWQMapNodeImpl *findInternal(KWQMapNodeImpl *target) const;
    KWQMapNodeImpl *insertInternal(KWQMapNodeImpl *nodeToInsert, bool replaceExisting);
    void removeInternal(KWQMapNodeImpl *nodeToRemove);
    uint countInternal() const;
    void clearInternal();
    void swap(KWQMapImpl &map);
    KWQMapNodeImpl *beginInternal() const;
    KWQMapNodeImpl *endInternal() const;

    virtual CompareResult compareNodes(KWQMapNodeImpl *a, KWQMapNodeImpl *b) const = 0;
    virtual void copyNode(KWQMapNodeImpl *src, KWQMapNodeImpl *dst) = 0;
    virtual KWQMapNodeImpl *duplicateNode(KWQMapNodeImpl *src) = 0;
    virtual void swapNodes(KWQMapNodeImpl *a, KWQMapNodeImpl *b) = 0;
    virtual void deleteNode(KWQMapNodeImpl *node) = 0;

 private:
    // can't possibly have a bigger height than this in a balanced tree
    static const int MAX_STACK = 64;

    void rebalanceAfterInsert(KWQMapNodeImpl **nodes, bool *wentLeft, int height);
    void rebalanceAfterRemove(KWQMapNodeImpl *nodeToRemove, KWQMapNodeImpl **nodes, bool *wentLeft, int height);
    void rotateRight(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent);
    void rotateLeft(KWQMapNodeImpl *node, KWQMapNodeImpl *parent, bool leftParent);

    KWQMapNodeImpl *guard;
    uint numNodes;

#ifdef QMAP_TESTING
    friend bool CheckRedBlackRules(KWQMapImpl *impl);
    friend bool CheckRedBlackRulesAtNode(KWQMapImpl *impl, KWQMapNodeImpl *node, int &black_height);
#endif
};

#endif

#endif
