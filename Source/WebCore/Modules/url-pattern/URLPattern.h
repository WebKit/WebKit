// JBG TODO notes
// TODO1: Refactor to use RefPtr & CheckedPtr
//          import with #include <wtf/checkedPtr.h>
// TODO2: Use std::variant types with the overlapping definitions
// TODO3: Undefined means void so please remember that when creating
//          record object
// TODO4: Input needs to be refactored from String to URLPatternInput


/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <wtf/Forward.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct URLPatternInit;
struct URLPatternOptions;
struct URLPatternResult;

// struct URLPatternCompatible {
//      TODO body
// }

class URLPattern final : public RefCounted<URLPattern> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(URLPattern);
public:
    using URLPatternInput = std::variant<String, URLPatternInit>;

    static Ref<URLPattern> create(const URLPatternInput input, const String&& baseURL, URLPatternOptions&& options);
    static Ref<URLPattern> create(std::optional<URLPatternInput> input, URLPatternOptions&& options);
    ~URLPattern();

    ExceptionOr<bool> test(std::optional<URLPatternInput> input, std::optional<String>&& baseURL);

    void exec(std::optional<String>&& input, std::optional<String>&& baseURL);

    ExceptionOr<String> protocol() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> username() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> password() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> hostname() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> port() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> pathname() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> search() const { return Exception { ExceptionCode::NotSupportedError }; }
    ExceptionOr<String> hash() const { return Exception { ExceptionCode::NotSupportedError }; }

    ExceptionOr<bool> hasRegExpGroups() const { return Exception { ExceptionCode::NotSupportedError }; }

private:

    URLPattern();

    String m_protocol;
    String m_username;
    String m_password;
    String m_hostname;
    String m_port;
    String m_pathname;
    String m_search;
    String m_hash;

    bool m_hasRegExpGroups;
    bool m_ignoreCase = false;
};

}

