/*
 * Copyright (C) 2013 Google Inc. All Rights Reserved.
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

namespace WebCore {

class PreloadRequest {
    WTF_MAKE_FAST_ALLOCATED;
public:
    enum class ModuleScript {
        Yes,
        No,
    };
    PreloadRequest(const String& initiator, const String& resourceURL, const URL& baseURL, CachedResource::Type resourceType, const String& mediaAttribute, ModuleScript moduleScript, const ReferrerPolicy& referrerPolicy)
        : m_initiator(initiator)
        , m_resourceURL(resourceURL)
        , m_baseURL(baseURL.isolatedCopy())
        , m_resourceType(resourceType)
        , m_mediaAttribute(mediaAttribute)
        , m_moduleScript(moduleScript)
        , m_referrerPolicy(referrerPolicy)
    {
    }

    CachedResourceRequest resourceRequest(Document&);

    const String& charset() const { return m_charset; }
    const String& media() const { return m_mediaAttribute; }
    void setCharset(const String& charset) { m_charset = charset.isolatedCopy(); }
    void setCrossOriginMode(const String& mode) { m_crossOriginMode = mode; }
    void setNonce(const String& nonce) { m_nonceAttribute = nonce; }
    void setScriptIsAsync(bool value) { m_scriptIsAsync = value; }
    CachedResource::Type resourceType() const { return m_resourceType; }

private:
    URL completeURL(Document&);

    String m_initiator;
    String m_resourceURL;
    URL m_baseURL;
    String m_charset;
    CachedResource::Type m_resourceType;
    String m_mediaAttribute;
    String m_crossOriginMode;
    String m_nonceAttribute;
    bool m_scriptIsAsync { false };
    ModuleScript m_moduleScript;
    ReferrerPolicy m_referrerPolicy;
};

typedef Vector<std::unique_ptr<PreloadRequest>> PreloadRequestStream;

class HTMLResourcePreloader : public CanMakeWeakPtr<HTMLResourcePreloader> {
    WTF_MAKE_NONCOPYABLE(HTMLResourcePreloader); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit HTMLResourcePreloader(Document& document)
        : m_document(document)
    {
    }

    void preload(PreloadRequestStream);
    void preload(std::unique_ptr<PreloadRequest>);

private:
    Document& m_document;
};

} // namespace WebCore
