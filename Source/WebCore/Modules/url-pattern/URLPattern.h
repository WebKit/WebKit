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

#include "ExceptionOr.h"
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct URLPatternInit;
struct URLPatternOptions;
struct URLPatternResult;
enum class BaseURLStringType : bool { Pattern, URL };

class URLPattern final : public RefCounted<URLPattern> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(URLPattern);
public:
    using URLPatternInput = std::variant<String, URLPatternInit>;

    static ExceptionOr<Ref<URLPattern>> create(URLPatternInput&&, String&& baseURL, URLPatternOptions&&);
    static ExceptionOr<Ref<URLPattern>> create(std::optional<URLPatternInput>&&, URLPatternOptions&&);
    ~URLPattern();

    ExceptionOr<bool> test(std::optional<URLPatternInput>&&, String&& baseURL) const;

    ExceptionOr<std::optional<URLPatternResult>> exec(std::optional<URLPatternInput>&&, String&& baseURL) const;

    const String& protocol() const { return m_protocol; }
    const String& username() const { return m_username; }
    const String& password() const { return m_password; }
    const String& hostname() const { return m_hostname; }
    const String& port() const { return m_port; }
    const String& pathname() const { return m_pathname; }
    const String& search() const { return m_search; }
    const String& hash() const { return m_hash; }

    bool hasRegExpGroups() const { return m_hasRegExpGroups; }

private:
    explicit URLPattern(URLPatternInit&& initInput);

    String m_protocol;
    String m_username;
    String m_password;
    String m_hostname;
    String m_port;
    String m_pathname;
    String m_search;
    String m_hash;

    bool m_hasRegExpGroups { false };
};

}
