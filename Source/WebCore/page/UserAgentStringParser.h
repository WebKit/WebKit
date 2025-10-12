/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/Variant.h>

namespace WebCore {
struct UserAgentStringData;
/*
 * This class takes in a user agent string and returns a UserAgentStringData class
 */
class UserAgentStringParser : public RefCountedAndCanMakeWeakPtr<UserAgentStringParser> {
public:
    static Ref<UserAgentStringParser> create(const String& userAgentString);
    std::optional<Ref<UserAgentStringData>> parse();

private:
    UserAgentStringParser(const String& userAgentString);

    void consumeProduct();
    void consumeComment();
    void consumeRWS();
    void consumeToken();
    void consumeQuotedPair();

    void populateUserAgentData();

    inline char16_t peek();
    inline void increment();
    inline bool atEnd();
    inline String getSubstring();

    bool malformed { false };
    const String& m_userAgentString;
    size_t pos { 0 };
    size_t start { 0 };
    Ref<UserAgentStringData> data;

    struct Product {
        String name;
        String version;
    };

    struct Comment {
        Vector<String> parts; // split on ;
    };

    using Segment = WTF::Variant<Product, Comment>;
    Vector<Segment> segments;
};
}
