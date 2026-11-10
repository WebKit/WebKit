/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Forward.h>

namespace WebCore {

class URLDecomposition {
public:
    RefPtr<StringImpl> origin() const;

    WEBCORE_EXPORT RefPtr<StringImpl> protocol() const;
    void setProtocol(StringView);

    RefPtr<StringImpl> username() const;
    void setUsername(StringView);

    RefPtr<StringImpl> password() const;
    void setPassword(StringView);

    WEBCORE_EXPORT RefPtr<StringImpl> host() const;
    void setHost(StringView);

    WEBCORE_EXPORT RefPtr<StringImpl> hostname() const;
    void setHostname(StringView);

    WEBCORE_EXPORT RefPtr<StringImpl> port() const;
    void setPort(StringView);

    WEBCORE_EXPORT RefPtr<StringImpl> pathname() const;
    void setPathname(StringView);

    WEBCORE_EXPORT RefPtr<StringImpl> search() const;
    void setSearch(StringImpl*);

    WEBCORE_EXPORT RefPtr<StringImpl> hash() const;
    void setHash(StringView);

    NODELETE static std::optional<uint16_t> parsePort(StringView);

protected:
    virtual ~URLDecomposition() = default;

private:
    virtual URL fullURL() const = 0;
    virtual void setFullURL(const URL&) = 0;
};

} // namespace WebCore
