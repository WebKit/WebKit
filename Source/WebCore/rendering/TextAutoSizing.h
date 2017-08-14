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

#pragma once

#if ENABLE(TEXT_AUTOSIZING)

#include "RenderStyle.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class Text;

// FIXME: We can probably get rid of this class entirely and use std::unique_ptr<RenderStyle> as key
// as long as we use the right hash traits.
class TextAutoSizingKey {
public:
    TextAutoSizingKey() = default;
    enum DeletedTag { Deleted };
    explicit TextAutoSizingKey(DeletedTag);
    TextAutoSizingKey(const RenderStyle&, unsigned hash);

    const RenderStyle* style() const { ASSERT(!isDeleted()); return m_style.get(); }
    bool isDeleted() const { return HashTraits<std::unique_ptr<RenderStyle>>::isDeletedValue(m_style); }

    unsigned hash() const { return m_hash; }

private:
    std::unique_ptr<RenderStyle> m_style;
    unsigned m_hash { 0 };
};

inline bool operator==(const TextAutoSizingKey& a, const TextAutoSizingKey& b)
{
    if (a.isDeleted() || b.isDeleted())
        return false;
    if (!a.style() || !b.style())
        return a.style() == b.style();
    return a.style()->equalForTextAutosizing(*b.style());
}

struct TextAutoSizingHash {
    static unsigned hash(const TextAutoSizingKey& key) { return key.hash(); }
    static bool equal(const TextAutoSizingKey& a, const TextAutoSizingKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = true;
};

struct TextAutoSizingHashTranslator {
    static unsigned hash(const RenderStyle& style)
    {
        return style.hashForTextAutosizing();
    }

    static bool equal(const TextAutoSizingKey& key, const RenderStyle& style)
    {
        if (key.isDeleted() || !key.style())
            return false;
        return key.style()->equalForTextAutosizing(style);
    }

    static void translate(TextAutoSizingKey& key, const RenderStyle& style, unsigned hash)
    {
        key = { style, hash };
    }
};

class TextAutoSizingValue {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextAutoSizingValue() = default;
    ~TextAutoSizingValue();

    void addTextNode(Text&, float size);

    enum class StillHasNodes { No, Yes };
    StillHasNodes adjustTextNodeSizes();

private:
    void reset();

    HashSet<RefPtr<Text>> m_autoSizedNodes;
};

struct TextAutoSizingTraits : WTF::GenericHashTraits<TextAutoSizingKey> {
    static const bool emptyValueIsZero = true;
    static void constructDeletedValue(TextAutoSizingKey& slot) { new (NotNull, &slot) TextAutoSizingKey(TextAutoSizingKey::Deleted); }
    static bool isDeletedValue(const TextAutoSizingKey& value) { return value.isDeleted(); }
};

class TextAutoSizing {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextAutoSizing() = default;

    void addTextNode(Text&, float size);
    void updateRenderTree();
    void reset();

private:
    HashMap<TextAutoSizingKey, std::unique_ptr<TextAutoSizingValue>, TextAutoSizingHash, TextAutoSizingTraits> m_textNodes;
};

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)
