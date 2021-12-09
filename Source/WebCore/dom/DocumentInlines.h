/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "MediaProducer.h"
#include "SecurityOrigin.h"
#include "TextResourceDecoder.h"

namespace WebCore {

inline PAL::TextEncoding Document::textEncoding() const
{
    if (auto* decoder = this->decoder())
        return decoder->encoding();
    return PAL::TextEncoding();
}

inline AtomString Document::encoding() const { return textEncoding().domName(); }

inline String Document::charset() const { return Document::encoding(); }

inline const Document* Document::templateDocument() const
{
    return m_templateDocumentHost ? this : m_templateDocument.get();
}

inline AXObjectCache* Document::existingAXObjectCache() const
{
    if (!hasEverCreatedAnAXObjectCache)
        return nullptr;
    return existingAXObjectCacheSlow();
}

inline Ref<Document> Document::create(const Settings& settings, const URL& url)
{
    return adoptRef(*new Document(nullptr, settings, url));
}

inline void Document::invalidateAccessKeyCache()
{
    if (UNLIKELY(m_accessKeyCache))
        invalidateAccessKeyCacheSlowCase();
}

inline bool Document::isCapturing() const
{
    return MediaProducer::isCapturing(m_mediaState);
}

inline bool Document::hasMutationObserversOfType(MutationObserverOptionType type) const
{
    return m_mutationObserverTypes.containsAny(type);
}

inline bool Document::isSameOriginAsTopDocument() const { return securityOrigin().isSameOriginAs(topOrigin()); }


// These functions are here because they require the Document class definition and we want to inline them.

inline ScriptExecutionContext* Node::scriptExecutionContext() const
{
    return &document().contextDocument();
}


}
