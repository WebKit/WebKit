/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/FastFloat.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
#include "fast_float/fast_float.h"
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

namespace WTF {

double parseDouble(std::span<const Latin1Character> string, size_t& parsedLength)
{
    double doubleValue = 0;
    auto stringData = byteCast<char>(string);
    auto result = fast_float::from_chars(std::to_address(stringData.begin()), std::to_address(stringData.end()), doubleValue, fast_float::chars_format::general | fast_float::chars_format::no_infnan | fast_float::chars_format::allow_leading_plus);
    parsedLength = result.ptr - stringData.data();
    return doubleValue;
}

double parseDouble(std::span<const char16_t> string, size_t& parsedLength)
{
    double doubleValue = 0;
    auto result = fast_float::from_chars(std::to_address(string.begin()), std::to_address(string.end()), doubleValue, fast_float::chars_format::general | fast_float::chars_format::no_infnan | fast_float::chars_format::allow_leading_plus);
    parsedLength = result.ptr - string.data();
    return doubleValue;
}

std::optional<double> parseJSONDouble(std::span<const Latin1Character> string, size_t& parsedLength)
{
    double doubleValue = 0;
    auto stringData = byteCast<char>(string);
    auto result = fast_float::from_chars(std::to_address(stringData.begin()), std::to_address(stringData.end()), doubleValue, fast_float::chars_format::json);
    if (!result) [[unlikely]] {
        if (result.ec != std::errc::result_out_of_range)
            return std::nullopt;
    }
    parsedLength = result.ptr - stringData.data();
    return doubleValue;
}

std::optional<double> parseJSONDouble(std::span<const char16_t> string, size_t& parsedLength)
{
    double doubleValue = 0;
    auto result = fast_float::from_chars(std::to_address(string.begin()), std::to_address(string.end()), doubleValue, fast_float::chars_format::json);
    if (!result) [[unlikely]] {
        if (result.ec != std::errc::result_out_of_range)
            return std::nullopt;
    }
    parsedLength = result.ptr - string.data();
    return doubleValue;
}

} // namespace WTF
