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

#ifndef IntrusiveDOMWrapperMap_h
#define IntrusiveDOMWrapperMap_h

#include "DOMDataStore.h"
#include "V8Node.h"
#include "WebCoreMemoryInstrumentation.h"
#include <wtf/HashSet.h>

namespace WebCore {

class IntrusiveDOMWrapperMap : public AbstractWeakReferenceMap<Node, v8::Object> {
public:
    IntrusiveDOMWrapperMap(v8::WeakReferenceCallback callback)
        : AbstractWeakReferenceMap<Node, v8::Object>(callback)
    {
    }

    virtual v8::Persistent<v8::Object> get(Node* node)
    {
        return node->wrapper();
    }

    virtual void set(Node* node, v8::Persistent<v8::Object> wrapper)
    {
        ASSERT(node && node->wrapper().IsEmpty());
        m_nodes.add(node);
        node->setWrapper(wrapper);
        wrapper.MakeWeak(node, weakReferenceCallback());
    }

    virtual bool contains(Node* node)
    {
        bool nodeHasWrapper = !node->wrapper().IsEmpty();
        ASSERT(nodeHasWrapper == m_nodes.contains(node));
        return nodeHasWrapper;
    }

    virtual void visit(DOMDataStore* store, Visitor* visitor)
    {
        for (HashSet<Node*>::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
            visitor->visitDOMWrapper(store, *it, (*it)->wrapper());
    }

    virtual bool removeIfPresent(Node* node, v8::Persistent<v8::Object> value)
    {
        ASSERT(node);
        v8::Persistent<v8::Object> wrapper = node->wrapper();
        if (wrapper.IsEmpty())
            return false;
        if (wrapper != value)
            return false;
        node->disposeWrapper();
        m_nodes.remove(node);
        return true;
    }

    virtual void clear()
    {
        m_nodes.clear();
    }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
        info.addMember(m_nodes);
    }

private:
    HashSet<Node*> m_nodes;
};

} // namespace WebCore

#endif
