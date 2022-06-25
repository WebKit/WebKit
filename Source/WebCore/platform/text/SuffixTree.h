/*
 * Copyright (C) 2010 Adam Barth. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class UnicodeCodebook {
public:
    static int codeWord(UChar c) { return c; }
    enum { codeSize = 1 << 8 * sizeof(UChar) };
};

class ASCIICodebook {
public:
    static int codeWord(UChar c) { return c & (codeSize - 1); }
    enum { codeSize = 1 << (8 * sizeof(char) - 1) };
};

template<typename Codebook>
class SuffixTree {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(SuffixTree)
public:
    SuffixTree(const String& text, unsigned depth)
        : m_depth(depth)
        , m_leaf(true)
    {
        build(text);
    }

    bool mightContain(const String& query)
    {
        Node* current = &m_root;
        int limit = std::min(m_depth, query.length());
        for (int i = 0; i < limit; ++i) {
            auto it = current->find(Codebook::codeWord(query[i]));
            if (it == current->end())
                return false;
            current = it->node;
        }
        return true;
    }

private:
    class Node {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        Node(bool isLeaf = false)
            : m_isLeaf(isLeaf)
        {
        }

        ~Node()
        {
            for (auto& entry : m_children) {
                auto* child = entry.node;
                if (child && !child->m_isLeaf)
                    delete child;
            }
        }

        Node*& childAt(int codeWord);

        auto find(int codeWord)
        {
            return std::find_if(m_children.begin(), m_children.end(), [codeWord](auto& entry) {
                return entry.codeWord == codeWord;
            });
        }

        auto end() { return m_children.end(); }

    private:
        struct ChildWithCodeWord {
            int codeWord;
            Node* node;
        };

        Vector<ChildWithCodeWord> m_children;
        bool m_isLeaf;
    };

    void build(const String& text)
    {
        for (unsigned base = 0; base < text.length(); ++base) {
            Node* current = &m_root;
            unsigned limit = std::min(base + m_depth, text.length());
            for (unsigned offset = 0; base + offset < limit; ++offset) {
                ASSERT(current != &m_leaf);
                Node*& child = current->childAt(Codebook::codeWord(text[base + offset]));
                if (!child)
                    child = base + offset + 1 == limit ? &m_leaf : new Node();
                current = child;
            }
        }
    }

    Node m_root;
    unsigned m_depth;

    // Instead of allocating a fresh empty leaf node for ever leaf in the tree
    // (there can be a lot of these), we alias all the leaves to this "static"
    // leaf node.
    Node m_leaf;
};

template<typename Codebook>
inline auto SuffixTree<Codebook>::Node::childAt(int codeWord) -> Node*&
{
    auto it = find(codeWord);
    if (it != m_children.end())
        return it->node;
    m_children.append(ChildWithCodeWord { codeWord, nullptr });
    return m_children.last().node;
}

} // namespace WebCore
