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

#include "DOMWrapperMap.h"

namespace WebCore {

class DOMNodeWrapperMap : public DOMWrapperMap<Node> {
public:
    virtual v8::Persistent<v8::Object> get(Node* node) OVERRIDE
    {
        return node->wrapper();
    }

    virtual void set(Node* node, v8::Persistent<v8::Object> wrapper) OVERRIDE
    {
        ASSERT(node && node->wrapper().IsEmpty());
        ASSERT(wrapper.WrapperClassId() == v8DOMNodeClassId);
        node->setWrapper(wrapper);
        wrapper.MakeWeak(node, weakCallback);
    }

    virtual void visit(DOMDataStore* store, DOMWrapperVisitor<Node>* visitor) OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void clear() OVERRIDE
    {
        ASSERT_NOT_REACHED();
    }

    virtual void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const OVERRIDE
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Binding);
    }

private:
    static void weakCallback(v8::Persistent<v8::Value> value, void* context)
    {
        Node* node = static_cast<Node*>(context);
        ASSERT(value->IsObject());
        ASSERT(node->wrapper() == v8::Persistent<v8::Object>::Cast(value));

        node->clearWrapper();
        value.Dispose();
        node->deref();
    }
};

} // namespace WebCore

#endif
