/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Adapter for Google's jsoncpp project using json.hpp from nlohmann-json.
// See: <rdar://117694188> Add jsoncpp project to libwebrtc

#if WEBRTC_WEBKIT_BUILD

#define JSON_NOEXCEPTION
#include <nlohmann/v3.8/json.hpp>

namespace Json {

using String = nlohmann::json;
using Value = nlohmann::json;

class CharReader {
public:
    CharReader() = default;
    ~CharReader() = default;

    bool parse(char const* begin, char const* end, Value* root, String* /*error*/) {
        if (!root)
            return false;
        *root = nlohmann::json::parse(begin, end);
        return true;
    }
};

class CharReaderBuilder {
public:
    CharReaderBuilder() = default;
    ~CharReaderBuilder() = default;

    CharReader* newCharReader() { return new CharReader(); }
};

} // namespace Json

#endif // WEBRTC_WEBKIT_BUILD
