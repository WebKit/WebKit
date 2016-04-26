/*
 * Copyright (C) 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef TextAutoSizing_h
#define TextAutoSizing_h

#if ENABLE(IOS_TEXT_AUTOSIZING)

#include "RenderStyle.h"
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class Node;

class TextAutoSizingKey {
public:
    TextAutoSizingKey() = default;
    enum DeletedTag { Deleted };
    explicit TextAutoSizingKey(DeletedTag);
    explicit TextAutoSizingKey(const RenderStyle*);
    TextAutoSizingKey(TextAutoSizingKey&&) = default;

    TextAutoSizingKey& operator=(TextAutoSizingKey&&) = default;

    const RenderStyle* style() const { return m_style.get(); }
    inline bool isDeleted() const { return m_isDeleted; }

private:
    std::unique_ptr<RenderStyle> m_style;
    bool m_isDeleted { false };
};

inline bool operator==(const TextAutoSizingKey& a, const TextAutoSizingKey& b)
{
    if (a.isDeleted() || b.isDeleted())
        return false;
    if (!a.style() || !b.style())
        return a.style() == b.style();
    return a.style()->equalForTextAutosizing(b.style());
}

struct TextAutoSizingHash {
    static unsigned hash(const TextAutoSizingKey& key) { return key.style()->hashForTextAutosizing(); }
    static bool equal(const TextAutoSizingKey& a, const TextAutoSizingKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

class TextAutoSizingValue : public RefCounted<TextAutoSizingValue> {
public:
    static Ref<TextAutoSizingValue> create()
    {
        return adoptRef(*new TextAutoSizingValue);
    }

    void addNode(Node*, float size);
    bool adjustNodeSizes();
    int numNodes() const;
    void reset();
private:
    TextAutoSizingValue() { }
    HashSet<RefPtr<Node> > m_autoSizedNodes;
};

} // namespace WebCore

#endif // ENABLE(IOS_TEXT_AUTOSIZING)

#endif // TextAutoSizing_h
