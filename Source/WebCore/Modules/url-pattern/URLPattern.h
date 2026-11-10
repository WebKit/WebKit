/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#include <WebCore/URLPatternComponent.h>
#include <WebCore/URLPatternInit.h>
#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct URLPatternOptions;
struct URLPatternResult;
template<typename> class ExceptionOr;

enum class BaseURLStringType : bool { Pattern, URL };

namespace URLPatternUtilities {
class URLPatternComponent;
}

class URLPattern final : public RefCounted<URLPattern> {
    WTF_MAKE_TZONE_ALLOCATED(URLPattern);
public:
    using URLPatternInput = Variant<RefPtr<StringImpl>, URLPatternInit>;

    WEBCORE_EXPORT static ExceptionOr<Ref<URLPattern>> create(URLPatternInput&&, RefPtr<StringImpl>&& baseURL, URLPatternOptions&&);
    WEBCORE_EXPORT static ExceptionOr<Ref<URLPattern>> create(URLPatternInput&&, URLPatternOptions&&);

    WEBCORE_EXPORT static ExceptionOr<Ref<URLPattern>> createWithoutRegExpSupport(URLPatternInput&&, RefPtr<StringImpl>&& baseURL, URLPatternOptions&&);
    WEBCORE_EXPORT bool testWithoutRegExp(const URL&) const;

    using Compatible = Variant<RefPtr<StringImpl>, URLPatternInit, Ref<URLPattern>>;
    static ExceptionOr<Ref<URLPattern>> create(Compatible&&, StringImpl*);

    WEBCORE_EXPORT ~URLPattern();

    WEBCORE_EXPORT ExceptionOr<bool> test(URLPatternInput&&, RefPtr<StringImpl>&& baseURL) const;

    WEBCORE_EXPORT ExceptionOr<std::optional<URLPatternResult>> exec(URLPatternInput&&, RefPtr<StringImpl>&& baseURL) const;

    const RefPtr<StringImpl>& protocol() const LIFETIME_BOUND { return m_protocolComponent.patternString(); }
    const RefPtr<StringImpl>& username() const LIFETIME_BOUND { return m_usernameComponent.patternString(); }
    const RefPtr<StringImpl>& password() const LIFETIME_BOUND { return m_passwordComponent.patternString(); }
    const RefPtr<StringImpl>& hostname() const LIFETIME_BOUND { return m_hostnameComponent.patternString(); }
    const RefPtr<StringImpl>& port() const LIFETIME_BOUND { return m_portComponent.patternString(); }
    const RefPtr<StringImpl>& pathname() const LIFETIME_BOUND { return m_pathnameComponent.patternString(); }
    const RefPtr<StringImpl>& search() const LIFETIME_BOUND { return m_searchComponent.patternString(); }
    const RefPtr<StringImpl>& hash() const LIFETIME_BOUND { return m_hashComponent.patternString(); }

    NODELETE WEBCORE_EXPORT bool hasRegExpGroups() const;
    bool shouldIgnoreCase() const { return m_shouldIgnoreCase; }

private:
    explicit URLPattern(bool shouldIgnoreCase)
        : m_shouldIgnoreCase(shouldIgnoreCase)
    {
    }

    enum class CompileMode : bool { WithRegExp, WithoutRegExp };
    ExceptionOr<void> compileAllComponents(URLPatternInit&&, CompileMode);
    ExceptionOr<std::optional<URLPatternResult>> match(Variant<URL, URLPatternInput>&&, RefPtr<StringImpl>&& baseURLString) const;

    const bool m_shouldIgnoreCase;
    URLPatternUtilities::URLPatternComponent m_protocolComponent;
    URLPatternUtilities::URLPatternComponent m_usernameComponent;
    URLPatternUtilities::URLPatternComponent m_passwordComponent;
    URLPatternUtilities::URLPatternComponent m_hostnameComponent;
    URLPatternUtilities::URLPatternComponent m_pathnameComponent;
    URLPatternUtilities::URLPatternComponent m_portComponent;
    URLPatternUtilities::URLPatternComponent m_searchComponent;
    URLPatternUtilities::URLPatternComponent m_hashComponent;
};

}
