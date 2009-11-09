/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebNode_h
#define WebNode_h

#include "WebCommon.h"
#include "WebString.h"

namespace WebCore { class Node; }
#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
#endif

namespace WebKit {
class WebFrame;

// Provides readonly access to some properties of a DOM node.
class WebNode {
public:
    virtual ~WebNode() { reset(); }

    WebNode() : m_private(0) { }
    WebNode(const WebNode& n) : m_private(0) { assign(n); }
    WebNode& operator=(const WebNode& n)
    {
        assign(n);
        return *this;
    }

    WEBKIT_API void reset();
    WEBKIT_API void assign(const WebNode&);

    bool isNull() const { return !m_private; }

#if WEBKIT_IMPLEMENTATION
    WebNode(const WTF::PassRefPtr<WebCore::Node>&);
    WebNode& operator=(const WTF::PassRefPtr<WebCore::Node>&);
    operator WTF::PassRefPtr<WebCore::Node>() const;
#endif

    WEBKIT_API WebNode parentNode() const;
    WEBKIT_API WebString nodeName() const;
    WebFrame* frame();

    template<typename T> T toElement()
    {
        T res;
        res.m_private = m_private;
        return res;
    }

protected:
    typedef WebCore::Node WebNodePrivate;
    void assign(WebNodePrivate*);
    WebNodePrivate* m_private;
    
    template<typename T> T* unwrap()
    {
        return static_cast<T*>(m_private);
    }

    template<typename T> const T* constUnwrap() const
    {
        return static_cast<const T*>(m_private);
    }
};

} // namespace WebKit

#endif
