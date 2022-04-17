/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "FrameView.h"
#include "RenderView.h"

namespace WebCore {

class UpToDateLayoutScope {
    WTF_MAKE_NONCOPYABLE(UpToDateLayoutScope);
public:
    UpToDateLayoutScope(Document& document)
        : m_document { &document }
    {
        m_document->incrementUpToDateLayoutScopeCount();
        RELEASE_ASSERT(!needsLayout(document));
    }
    
    UpToDateLayoutScope(UpToDateLayoutScope&& source)
        : m_document { std::exchange(source.m_document, nullptr) }
    {
    }

    ~UpToDateLayoutScope()
    {
        if (m_document)
            m_document->decrementUpToDateLayoutScopeCount();
    }

    static std::optional<UpToDateLayoutScope> scopeIfLayoutIsUpToUpdate(Document& document)
    {
        if (needsLayout(document))
            return std::nullopt;
        return UpToDateLayoutScope { document };
    }

    static bool needsLayout(Document& document)
    {
        if (document.needsStyleRecalc())
            return true;
        RefPtr frameView = document.view();
        return frameView && frameView->layoutContext().needsLayout();
    }

private:
    RefPtr<Document> m_document;
};

}
