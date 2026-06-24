/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <concepts>
#include <format>
#include <type_traits>
#include <wtf/Assertions.h>
#include <wtf/text/TextStream.h>

namespace WTF {

template<typename T>
concept TextStreamable = requires(TextStream& ts, const T& v) { ts << v; }
    && !std::is_fundamental_v<T>
    && !std::is_pointer_v<T>
    && !std::is_array_v<T>;

} // namespace WTF

template<WTF::TextStreamable T>
struct std::formatter<T> : std::formatter<std::string_view> {
    auto format(const T &value, std::format_context &ctx) const
    {
        WTF::TextStream stream(WTF::TextStream::LineMode::SingleLine);
        stream << value;
        auto utf8 = stream.release().utf8();
        return std::formatter<std::string_view>::format(std::string_view(utf8.data(), utf8.length()), ctx);
    }
};

namespace WTF {

template<typename... Arguments>
inline void logAlways(std::format_string<Arguments...> fmt, Arguments&&... args)
{
    auto string = std::format(fmt, std::forward<Arguments>(args)...);
    WTFLogAlways("%s", string.c_str());
}

template<typename... Arguments>
[[noreturn]] inline void logAlwaysAndCrash(std::format_string<Arguments...> fmt, Arguments&&... args)
{
    auto string = std::format(fmt, std::forward<Arguments>(args)...);
    WTFLogAlwaysAndCrash("%s", string.c_str());
}

template<typename... Arguments>
inline void logToChannel(WTFLogChannel& channel, std::format_string<Arguments...> fmt, Arguments&&... args)
{
    auto string = std::format(fmt, std::forward<Arguments>(args)...);
    WTFLog(&channel, "%s", string.c_str());
}

template<typename... Arguments>
inline void logVerbose(const char* file, int line, const char* function, WTFLogChannel& channel, std::format_string<Arguments...> fmt, Arguments&&... args)
{
    auto string = std::format(fmt, std::forward<Arguments>(args)...);
    WTFLogVerbose(file, line, function, &channel, "%s", string.c_str());
}

template<typename... Arguments>
inline void logWithLevel(WTFLogChannel& channel, WTFLogLevel level, std::format_string<Arguments...> fmt, Arguments&&... args)
{
    auto string = std::format(fmt, std::forward<Arguments>(args)...);
    WTFLogWithLevel(&channel, level, "%s", string.c_str());
}

} // namespace WTF

using WTF::logAlways;
using WTF::logAlwaysAndCrash;
using WTF::logToChannel;
using WTF::logVerbose;
using WTF::logWithLevel;
