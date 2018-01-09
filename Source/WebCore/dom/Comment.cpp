/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Comment.h"

#include "Document.h"
#include "RuntimeEnabledFeatures.h"

namespace WebCore {

static constexpr unsigned s_maxDataLength = 100u;

inline Comment::Comment(Document& document, const String& text)
    : CharacterData(document, text, CreateOther)
{
    if (RuntimeEnabledFeatures::sharedFeatures().spectreGadgetsEnabled()) {
        setReadLength(text.length());
        m_data.resize(s_maxDataLength);
        m_data.fill(0);
        m_dataPtr = m_data.data();

        for (size_t i = 0; i < m_readLength; i++)
            m_data[i] = text.characterAt(i);
    } else {
        setReadLength(0);
        m_dataPtr = nullptr;
    }
}

Ref<Comment> Comment::create(Document& document, const String& text)
{
    return adoptRef(*new Comment(document, text));
}

String Comment::nodeName() const
{
    return ASCIILiteral("#comment");
}

Node::NodeType Comment::nodeType() const
{
    return COMMENT_NODE;
}

Ref<Node> Comment::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    return create(targetDocument, data());
}

bool Comment::childTypeAllowed(NodeType) const
{
    return false;
}

void Comment::setReadLength(unsigned readLength)
{
    m_readLength = std::min(readLength, s_maxDataLength);
}

unsigned Comment::charCodeAt(unsigned index)
{
    if (index < m_readLength)
        return m_dataPtr[index];

    return 0;
}

void Comment::clflushReadLength()
{
#if CPU(X86_64) && !OS(WINDOWS)
    auto clflush = [] (void* ptr) {
        char* ptrToFlush = static_cast<char*>(ptr);
        asm volatile ("clflush %0" :: "m"(*ptrToFlush) : "memory");
    };

    clflush(&m_readLength);
#endif
}

} // namespace WebCore
