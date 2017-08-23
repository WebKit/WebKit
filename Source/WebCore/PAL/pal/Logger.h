/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace PAL {

class Logger : public RefCounted<Logger> {
    WTF_MAKE_NONCOPYABLE(Logger);
public:
    static Ref<Logger> create(const void* owner)
    {
        return adoptRef(*new Logger(owner));
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelError))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelWarning))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void notice(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelNotice))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelInfo))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevelDebug))
            return;

        log(channel, format, arguments...);
    }

    inline bool willLog(WTFLogChannel& channel, WTFLogLevel level) const
    {
        return m_enabled && channel.level >= level && channel.state != WTFLogChannelOff;
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(const void* owner, bool enabled)
    {
        ASSERT(owner == m_owner);
        if (owner == m_owner)
            m_enabled = enabled;
    }

private:
    Logger(const void* owner) { m_owner = owner; }

    static inline void log(WTFLogChannel& channel, const char* format, ...)
    {
        va_list arguments;
        va_start(arguments, format);

#if COMPILER(CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, format, arguments);
#else
        String string = String::format(format, arguments);
        os_log(channel.osLogChannel, "%{public}s", string.utf8().data());
#endif

#if COMPILER(CLANG)
#pragma clang diagnostic pop
#endif

        va_end(arguments);
    }

    const void* m_owner;
    bool m_enabled { true };
};

} // namespace PAL

