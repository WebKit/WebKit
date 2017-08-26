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
    inline void logAlways(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
        UNUSED_PARAM(format);
#else
        if (!willLog(channel, WTFLogLevelAlways))
            return;

        log(channel, format, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
        if (!willLog(channel, WTFLogLevelError))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
        if (!willLog(channel, WTFLogLevelWarning))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void notice(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
        if (!willLog(channel, WTFLogLevelNotice))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
        if (!willLog(channel, WTFLogLevelInfo))
            return;

        log(channel, format, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const char* format, const Arguments&... arguments) const WTF_ATTRIBUTE_PRINTF(3, 0)
    {
        if (!willLog(channel, WTFLogLevelDebug))
            return;

        log(channel, format, arguments...);
    }

    inline bool willLog(WTFLogChannel& channel, WTFLogLevel level) const
    {
        if (level != WTFLogLevelAlways && level > channel.level)
            return false;

        if (channel.level != WTFLogLevelAlways && channel.state == WTFLogChannelOff)
            return false;

        return m_enabled;
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

#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
        String string = String::formatWithArguments(format, arguments);
#if COMPILER(GCC_OR_CLANG)
#pragma GCC diagnostic pop
#endif

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", string.utf8().data());
#else
        os_log(channel.osLogChannel, "%{public}s", string.utf8().data());
#endif

        va_end(arguments);
    }

    const void* m_owner;
    bool m_enabled { true };
};

} // namespace PAL

