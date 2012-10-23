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
#include "V8DOMMap.h"
#include "V8Node.h"
#include "WebCoreMemoryInstrumentation.h"

namespace WebCore {

class DOMNodeWrapperMap : public AbstractWeakReferenceMap<Node, v8::Object> {
public:
    DOMNodeWrapperMap(v8::WeakReferenceCallback callback)
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
        ASSERT(wrapper.WrapperClassId() == v8DOMSubtreeClassId);
        node->setWrapper(wrapper);
        wrapper.MakeWeak(node, weakReferenceCallback());
    }

    virtual bool contains(Node* node)
    {
        return !node->wrapper().IsEmpty();
    }

    virtual void visit(DOMDataStore* store, Visitor* visitor)
    {
        ASSERT_NOT_REACHED();
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
        return true;
    }

    virtual void clear()
    {
        ASSERT_NOT_REACHED();
    }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
    {
        // This data structure doesn't use any extra memory.
    }
};

class ActiveDOMNodeWrapperMap : public DOMWrapperMap<Node> {
public:
    ActiveDOMNodeWrapperMap(v8::WeakReferenceCallback callback)
        : DOMWrapperMap<Node>(callback)
    {
    }

    virtual v8::Persistent<v8::Object> get(Node* node) OVERRIDE
    {
        return node->wrapper();
    }

    virtual void set(Node* node, v8::Persistent<v8::Object> wrapper) OVERRIDE
    {
        ASSERT(node && node->wrapper().IsEmpty());
        ASSERT(wrapper.WrapperClassId() == v8DOMSubtreeClassId);
        node->setWrapper(wrapper);
        DOMWrapperMap<Node>::set(node, wrapper);
    }

    virtual bool removeIfPresent(Node* node, v8::Persistent<v8::Object> value) OVERRIDE
    {
        if (!DOMWrapperMap<Node>::removeIfPresent(node, value))
            return false;
        // Notice that we call "clearWrapper" here rather than disposeWrapper because
        // our base class will call Dispose on the wrapper for us.
        node->clearWrapper();
        return true;
    }

    virtual void clear() OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }
};

} // namespace WebCore

#endif
