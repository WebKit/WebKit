/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "CachedResource.h"
#include "CachedResourceRequest.h"
#include "ScriptType.h"
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakRef.h>

namespace WebCore {

class PreloadRequest {
    WTF_MAKE_TZONE_ALLOCATED(PreloadRequest);
public:
    PreloadRequest(ASCIILiteral initiatorType, StringImpl* resourceURL, const URL& baseURL, CachedResource::Type resourceType, StringImpl* mediaAttribute, ScriptType scriptType, const ReferrerPolicy& referrerPolicy, RequestPriority fetchPriority = RequestPriority::Auto)
        : m_initiatorType(initiatorType)
        , m_resourceURL(resourceURL)
        , m_baseURL(baseURL.isolatedCopy())
        , m_mediaAttribute(mediaAttribute)
        , m_resourceType(resourceType)
        , m_scriptType(scriptType)
        , m_referrerPolicy(referrerPolicy)
        , m_fetchPriority(fetchPriority)
    {
    }

    CachedResourceRequest resourceRequest(Document&);

    const RefPtr<StringImpl>& charset() const LIFETIME_BOUND { return m_charset; }
    const RefPtr<StringImpl>& media() const LIFETIME_BOUND { return m_mediaAttribute; }
    void setCharset(StringImpl* charset) { m_charset = isolatedCopy(charset); }
    void setCrossOriginMode(StringImpl* mode) { m_crossOriginMode = mode; }
    void setNonce(StringImpl* nonce) { m_nonceAttribute = nonce; }
    void setIntegrity(StringImpl* integrity) { m_integrityAttribute = integrity; }
    void setScriptIsAsync(bool value) { m_scriptIsAsync = value; }
    CachedResource::Type resourceType() const { return m_resourceType; }

private:
    URL completeURL(Document&);

    ASCIILiteral m_initiatorType;
    RefPtr<StringImpl> m_resourceURL;
    URL m_baseURL;
    RefPtr<StringImpl> m_charset;
    RefPtr<StringImpl> m_mediaAttribute;
    RefPtr<StringImpl> m_crossOriginMode;
    RefPtr<StringImpl> m_nonceAttribute;
    RefPtr<StringImpl> m_integrityAttribute;
    bool m_scriptIsAsync { false };
    CachedResource::Type m_resourceType;
    ScriptType m_scriptType;
    ReferrerPolicy m_referrerPolicy;
    RequestPriority m_fetchPriority;
};

typedef Vector<std::unique_ptr<PreloadRequest>> PreloadRequestStream;

class HTMLResourcePreloader : public RefCountedAndCanMakeWeakPtr<HTMLResourcePreloader> {
    WTF_MAKE_TZONE_ALLOCATED(HTMLResourcePreloader);
    WTF_MAKE_NONCOPYABLE(HTMLResourcePreloader);
public:
    NODELETE static Ref<HTMLResourcePreloader> create(Document&);
    ~HTMLResourcePreloader();

    void preload(PreloadRequestStream);
    void preload(std::unique_ptr<PreloadRequest>);

private:
    explicit HTMLResourcePreloader(Document&);

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_document;
};

} // namespace WebCore
