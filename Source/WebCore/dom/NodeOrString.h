/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NodeOrString_h
#define NodeOrString_h

#include "Node.h"
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class NodeOrString {
public:
    enum class Type {
        String,
        Node
    };

    NodeOrString(const String& string)
        : m_type(Type::String)
    {
        m_data.string = string.impl();
        m_data.string->ref();
    }

    NodeOrString(Node* node)
        : m_type(Type::Node)
    {
        m_data.node = node;
        m_data.node->ref();
    }

    NodeOrString(const NodeOrString& other)
        : m_type(other.m_type)
    {
        switch (m_type) {
        case Type::String:
            m_data.string = other.m_data.string;
            m_data.string->ref();
            break;
        case Type::Node:
            m_data.node = other.m_data.node;
            m_data.node->ref();
            break;
        }
    }

    NodeOrString(NodeOrString&& other)
        : m_type(other.m_type)
    {
        switch (m_type) {
        case Type::String:
            m_data.string = std::exchange(other.m_data.string, nullptr);
            break;
        case Type::Node:
            m_data.node = std::exchange(other.m_data.node, nullptr);
            break;
        }
    }

    NodeOrString& operator=(const NodeOrString& other)
    {
        if (this == &other)
            return *this;

        derefData();

        m_type = other.m_type;
        switch (m_type) {
        case Type::String:
            m_data.string = other.m_data.string;
            m_data.string->ref();
            break;
        case Type::Node:
            m_data.node = other.m_data.node;
            m_data.node->ref();
            break;
        }

        return *this;
    }

    NodeOrString& operator=(NodeOrString&& other)
    {
        if (this == &other)
            return *this;

        derefData();

        m_type = other.m_type;
        switch (m_type) {
        case Type::String:
            m_data.string = std::exchange(other.m_data.string, nullptr);
            break;
        case Type::Node:
            m_data.node = std::exchange(other.m_data.node, nullptr);
            break;
        }

        return *this;
    }

    ~NodeOrString()
    {
        derefData();
    }

    Type type() const { return m_type; }

    Node& node() const { ASSERT(m_type == Type::Node); ASSERT(m_data.node); return *m_data.node; }
    StringImpl& string() const { ASSERT(m_type == Type::String); ASSERT(m_data.string); return *m_data.string; }
    
private:
    void derefData()
    {
        switch (m_type) {
        case Type::String:
            if (m_data.string)
                m_data.string->deref();
            break;
        case Type::Node:
            if (m_data.node)
                m_data.node->deref();
            break;
        }
    }

    Type m_type;
    union {
        StringImpl* string;
        Node* node;
    } m_data;
};

RefPtr<Node> convertNodesOrStringsIntoNode(Node& context, Vector<NodeOrString>&&, ExceptionCode&);

} // namespace WebCore

#endif // NodeOrString_h
