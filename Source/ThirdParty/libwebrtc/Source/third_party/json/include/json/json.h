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

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <vector>

#include "absl/strings/string_view.h"

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

namespace webrtc {

// Drop-in replacements for the `webrtc::` helpers declared in
// `rtc_base/strings/json.h` and defined in `rtc_base/strings/json.cc`.
// `rtc_base/strings/json.cc` is not compiled into WebKit's libwebrtc
// (it depends on jsoncpp APIs not provided by this adapter), so unit
// tests that need the helpers reach them through this header instead.

inline bool GetStringFromJson(const Json::Value& in, std::string* out) {
    if (in.is_string()) { *out = in.get<std::string>(); return true; }
    if (in.is_boolean()) { *out = in.get<bool>() ? "true" : "false"; return true; }
    if (in.is_number_integer()) { *out = std::to_string(in.get<int64_t>()); return true; }
    if (in.is_number_unsigned()) { *out = std::to_string(in.get<uint64_t>()); return true; }
    if (in.is_number_float()) { *out = std::to_string(in.get<double>()); return true; }
    return false;
}

inline bool GetIntFromJson(const Json::Value& in, int* out) {
    if (in.is_number_unsigned()) {
        uint64_t v = in.get<uint64_t>();
        if (v > static_cast<uint64_t>(INT_MAX)) return false;
        *out = static_cast<int>(v);
        return true;
    }
    if (in.is_number_integer()) {
        int64_t v = in.get<int64_t>();
        if (v < INT_MIN || v > INT_MAX) return false;
        *out = static_cast<int>(v);
        return true;
    }
    if (in.is_string()) {
        const std::string& s = in.get_ref<const std::string&>();
        char* end_ptr;
        errno = 0;
        long val = std::strtol(s.c_str(), &end_ptr, 10);
        if (end_ptr != s.c_str() && *end_ptr == '\0' && !errno && val >= INT_MIN && val <= INT_MAX) {
            *out = static_cast<int>(val);
            return true;
        }
    }
    return false;
}

inline bool GetBoolFromJson(const Json::Value& in, bool* out) {
    if (in.is_boolean()) { *out = in.get<bool>(); return true; }
    if (in.is_string()) {
        const std::string& s = in.get_ref<const std::string&>();
        if (s == "true") { *out = true; return true; }
        if (s == "false") { *out = false; return true; }
    }
    return false;
}

inline bool GetDoubleFromJson(const Json::Value& in, double* out) {
    if (in.is_number()) { *out = in.get<double>(); return true; }
    if (in.is_string()) {
        const std::string& s = in.get_ref<const std::string&>();
        char* end_ptr;
        errno = 0;
        double val = std::strtod(s.c_str(), &end_ptr);
        if (end_ptr != s.c_str() && *end_ptr == '\0' && !errno) {
            *out = val;
            return true;
        }
    }
    return false;
}

inline bool GetValueFromJsonObject(const Json::Value& in, absl::string_view k, Json::Value* out) {
    std::string key(k);
    if (!in.is_object() || !in.contains(key)) return false;
    *out = in.at(key);
    return true;
}

inline bool GetIntFromJsonObject(const Json::Value& in, absl::string_view k, int* out) {
    Json::Value x;
    return GetValueFromJsonObject(in, k, &x) && GetIntFromJson(x, out);
}

inline bool GetStringFromJsonObject(const Json::Value& in, absl::string_view k, std::string* out) {
    Json::Value x;
    return GetValueFromJsonObject(in, k, &x) && GetStringFromJson(x, out);
}

inline bool GetBoolFromJsonObject(const Json::Value& in, absl::string_view k, bool* out) {
    Json::Value x;
    return GetValueFromJsonObject(in, k, &x) && GetBoolFromJson(x, out);
}

inline bool GetDoubleFromJsonObject(const Json::Value& in, absl::string_view k, double* out) {
    Json::Value x;
    return GetValueFromJsonObject(in, k, &x) && GetDoubleFromJson(x, out);
}

namespace json_internal {
template <typename T>
inline bool JsonArrayToVector(const Json::Value& in,
                              bool (*getter)(const Json::Value&, T*),
                              std::vector<T>* out) {
    out->clear();
    if (!in.is_array()) return false;
    for (const auto& v : in) {
        T val;
        if (!getter(v, &val)) return false;
        out->push_back(val);
    }
    return true;
}
} // namespace json_internal

inline bool JsonArrayToIntVector(const Json::Value& in, std::vector<int>* out) {
    return json_internal::JsonArrayToVector(in, GetIntFromJson, out);
}

inline bool JsonArrayToStringVector(const Json::Value& in, std::vector<std::string>* out) {
    return json_internal::JsonArrayToVector(in, GetStringFromJson, out);
}

inline bool JsonArrayToBoolVector(const Json::Value& in, std::vector<bool>* out) {
    out->clear();
    if (!in.is_array()) return false;
    for (const auto& v : in) {
        bool val;
        if (!GetBoolFromJson(v, &val)) return false;
        out->push_back(val);
    }
    return true;
}

inline bool JsonArrayToDoubleVector(const Json::Value& in, std::vector<double>* out) {
    return json_internal::JsonArrayToVector(in, GetDoubleFromJson, out);
}

} // namespace webrtc

#endif // WEBRTC_WEBKIT_BUILD
