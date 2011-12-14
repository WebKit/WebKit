/*
 * Copyright (C) 2006, 2007, 2008, 2009 Google Inc. All rights reserved.
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

#ifndef V8NodeFilterCondition_h
#define V8NodeFilterCondition_h

#include "NodeFilterCondition.h"
#include <v8.h>
#include <wtf/PassRefPtr.h>

// NodeFilter is a JavaScript function that takes a Node as parameter and returns a short (ACCEPT, SKIP, REJECT) as the result.
namespace WebCore {

    class Node;
    class ScriptState;

    // NodeFilterCondition is a wrapper around a NodeFilter JS function.
    class V8NodeFilterCondition : public NodeFilterCondition {
    public:
        static PassRefPtr<V8NodeFilterCondition> create(v8::Handle<v8::Value> filter)
        {
            return adoptRef(new V8NodeFilterCondition(filter));
        }

        virtual ~V8NodeFilterCondition();

        virtual short acceptNode(ScriptState*, Node*) const;

    private:
        explicit V8NodeFilterCondition(v8::Handle<v8::Value> filter);

        mutable v8::Persistent<v8::Value> m_filter;
    };

} // namespace WebCore

#endif // V8NodeFilterCondition_h
