/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ShadowRootList_h
#define ShadowRootList_h

#include <wtf/DoublyLinkedList.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class ShadowRoot;
class Element;

class ShadowRootList {
public:
    ShadowRootList();
    ~ShadowRootList();

    bool hasShadowRoot();
    ShadowRoot* youngestShadowRoot();
    ShadowRoot* oldestShadowRoot();

    void pushShadowRoot(ShadowRoot*);
    ShadowRoot* popShadowRoot();

    void insertedIntoDocument();
    void removedFromDocument();
    void insertedIntoTree(bool deep);
    void removedFromTree(bool deep);
    void willRemove();

    void hostChildrenChanged();

    void attach();
    void detach();

private:
    DoublyLinkedList<ShadowRoot> m_shadowRoots;
    WTF_MAKE_NONCOPYABLE(ShadowRootList);
};

inline bool ShadowRootList::hasShadowRoot()
{
    return !m_shadowRoots.isEmpty();
}

inline ShadowRoot* ShadowRootList::youngestShadowRoot()
{
    return m_shadowRoots.head();
}

inline ShadowRoot* ShadowRootList::oldestShadowRoot()
{
    return m_shadowRoots.tail();
}

} // namespace

#endif
