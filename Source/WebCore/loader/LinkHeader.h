/*
 * Copyright 2015 The Chromium Authors. All rights reserved.
 * Copyright (C) 2016 Akamai Technologies Inc. All rights reserved.
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class LinkHeader {
public:
    template<typename CharacterType> LinkHeader(StringParsingBuffer<CharacterType>&);

    const RefPtr<StringImpl>& url() const LIFETIME_BOUND { return m_url; }
    const RefPtr<StringImpl>& rel() const LIFETIME_BOUND { return m_rel; }
    const RefPtr<StringImpl>& as() const LIFETIME_BOUND { return m_as; }
    const RefPtr<StringImpl>& mimeType() const LIFETIME_BOUND { return m_mimeType; }
    const RefPtr<StringImpl>& media() const LIFETIME_BOUND { return m_media; }
    const RefPtr<StringImpl>& crossOrigin() const LIFETIME_BOUND { return m_crossOrigin; }
    const RefPtr<StringImpl>& imageSrcSet() const LIFETIME_BOUND { return m_imageSrcSet; }
    const RefPtr<StringImpl>& imageSizes() const LIFETIME_BOUND { return m_imageSizes; }
    const RefPtr<StringImpl>& nonce() const LIFETIME_BOUND { return m_nonce; }
    const RefPtr<StringImpl>& referrerPolicy() const LIFETIME_BOUND { return m_referrerPolicy; }
    const RefPtr<StringImpl>& fetchPriority() const LIFETIME_BOUND { return m_fetchPriority; }
    bool valid() const { return m_isValid; }
    bool isViewportDependent() const { return !isEmpty(media()) || !isEmpty(imageSrcSet()) || !isEmpty(imageSizes()); }

    enum LinkParameterName {
        LinkParameterRel,
        LinkParameterAnchor,
        LinkParameterTitle,
        LinkParameterMedia,
        LinkParameterType,
        LinkParameterRev,
        LinkParameterHreflang,
        // Beyond this point, only link-extension parameters
        LinkParameterUnknown,
        LinkParameterCrossOrigin,
        LinkParameterAs,
        LinkParameterImageSrcSet,
        LinkParameterImageSizes,
        LinkParameterNonce,
        LinkParameterReferrerPolicy,
        LinkParameterFetchPriority,
    };

private:
    void setValue(LinkParameterName, RefPtr<StringImpl>&& value);

    RefPtr<StringImpl> m_url;
    RefPtr<StringImpl> m_rel;
    RefPtr<StringImpl> m_as;
    RefPtr<StringImpl> m_mimeType;
    RefPtr<StringImpl> m_media;
    RefPtr<StringImpl> m_crossOrigin;
    RefPtr<StringImpl> m_imageSrcSet;
    RefPtr<StringImpl> m_imageSizes;
    RefPtr<StringImpl> m_nonce;
    RefPtr<StringImpl> m_referrerPolicy;
    RefPtr<StringImpl> m_fetchPriority;
    bool m_isValid { true };
};

class LinkHeaderSet {
public:
    WEBCORE_EXPORT LinkHeaderSet(StringImpl* header);

    Vector<LinkHeader>::const_iterator begin() const LIFETIME_BOUND { return m_headerSet.begin(); }
    Vector<LinkHeader>::const_iterator end() const LIFETIME_BOUND { return m_headerSet.end(); }

private:
    Vector<LinkHeader> m_headerSet;
};

} // namespace WebCore

