/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#pragma once

#include "LayoutDescendantIterator.h"

namespace WebCore {
namespace Layout {

class FormattingContextBoxIterator : public LayoutDescendantIterator<Box> {
public:
    FormattingContextBoxIterator(const ElementBox& root)
        : LayoutDescendantIterator<Layout::Box>(root)
    {
    }

    enum FirstTag { First };
    FormattingContextBoxIterator(const ElementBox& root, FirstTag)
        : LayoutDescendantIterator<Box>(root, Traversal::firstWithin<Box>(root))
    {
    }

    FormattingContextBoxIterator& operator++()
    {
        if (get().establishesFormattingContext())
            traverseNextSkippingChildren();
        else
            traverseNext();
        return *this;
    }
};

class FormattingContextBoxIteratorAdapter {
public:
    FormattingContextBoxIteratorAdapter(const ElementBox& root)
        : m_root(root)
    {
    }
    FormattingContextBoxIterator begin() { return { m_root, FormattingContextBoxIterator::First }; }
    FormattingContextBoxIterator end() { return { m_root }; }

private:
    const ElementBox& m_root;
};

inline FormattingContextBoxIteratorAdapter formattingContextBoxes(const ElementBox& root)
{
    ASSERT(root.establishesFormattingContext());
    return { root };
}


}
}
