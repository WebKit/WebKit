/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TextAutosizer_h
#define TextAutosizer_h

#if ENABLE(TEXT_AUTOSIZING)

#include "LayoutTypes.h"
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {

class Document;
class RenderBox;
class RenderObject;
class RenderStyle;
class RenderText;

class TextAutosizer {
    WTF_MAKE_NONCOPYABLE(TextAutosizer);

public:
    static PassOwnPtr<TextAutosizer> create(Document* document) { return adoptPtr(new TextAutosizer(document)); }

    virtual ~TextAutosizer();

    bool processSubtree(RenderObject* layoutRoot);

private:
    explicit TextAutosizer(Document*);

    void processBox(RenderBox*, const IntSize& windowSize);
    void setMultiplier(RenderObject*, float);

    static bool isNotAnAutosizingContainer(const RenderObject*);

    typedef bool (*RenderObjectFilterFunctor)(const RenderObject*);
    // Use to traverse the tree of descendants, excluding any subtrees within that whose root doesn't pass the filter.
    static RenderObject* nextInPreOrderMatchingFilter(RenderObject* current, const RenderObject* stayWithin, RenderObjectFilterFunctor);

    Document* m_document;
};

} // namespace WebCore

#endif // ENABLE(TEXT_AUTOSIZING)

#endif // TextAutosizer_h
