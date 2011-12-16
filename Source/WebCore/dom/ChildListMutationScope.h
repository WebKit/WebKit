/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
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

#ifndef ChildListMutationScope_h
#define ChildListMutationScope_h

#if ENABLE(MUTATION_OBSERVERS)

#include "Document.h"
#include "Node.h"
#include "WebKitMutationObserver.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class ChildListMutationScope {
    WTF_MAKE_NONCOPYABLE(ChildListMutationScope);
public:
    ChildListMutationScope(Node* target)
        : m_target(target->document()->hasMutationObserversOfType(WebKitMutationObserver::ChildList) ? target : 0)
    {
        if (m_target)
            MutationAccumulationRouter::instance()->incrementScopingLevel(m_target);
    }

    ~ChildListMutationScope()
    {
        if (m_target)
            MutationAccumulationRouter::instance()->decrementScopingLevel(m_target);
    }

    void childAdded(Node* child)
    {
        if (m_target)
            MutationAccumulationRouter::instance()->childAdded(m_target, child);
    }

    void willRemoveChild(Node* child)
    {
        if (m_target)
            MutationAccumulationRouter::instance()->willRemoveChild(m_target, child);
    }

private:
    class MutationAccumulator;

    class MutationAccumulationRouter {
        WTF_MAKE_NONCOPYABLE(MutationAccumulationRouter);
    public:
        ~MutationAccumulationRouter();

        static MutationAccumulationRouter* instance();

        void incrementScopingLevel(Node*);
        void decrementScopingLevel(Node*);

        void childAdded(Node* target, Node* child);
        void willRemoveChild(Node* target, Node* child);

    private:
        MutationAccumulationRouter();
        static void initialize();

        typedef HashMap<Node*, unsigned> ScopingLevelMap;
        ScopingLevelMap m_scopingLevels;
        HashMap<Node*, OwnPtr<MutationAccumulator> > m_accumulations;

        static MutationAccumulationRouter* s_instance;
    };

    Node* m_target;
};

} // namespace WebCore

#endif // ENABLE(MUTATION_OBSERVERS)

#endif // ChildListMutationScope_h
