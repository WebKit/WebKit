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
#include <wtf/HexNumber.h>
#include <wtf/Noncopyable.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/WTFString.h>

namespace PAL {

template<typename T>
struct LogArgument {
    template<typename U = T> static typename std::enable_if<std::is_same<U, int>::value, String>::type toString(int argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, float>::value, String>::type toString(float argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, double>::value, String>::type toString(double argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, String>::value, String>::type toString(String argument) { return argument; }
    template<typename U = T> static typename std::enable_if<std::is_same<U, const char*>::value, String>::type toString(const char* argument) { return String(argument); }
    template<size_t length> static String toString(const char (&argument)[length]) { return String(argument); }
};

class Logger : public RefCounted<Logger> {
    WTF_MAKE_NONCOPYABLE(Logger);
public:
    static Ref<Logger> create(const void* owner)
    {
        return adoptRef(*new Logger(owner));
    }

    template<typename... Arguments>
    inline void logAlways(WTFLogChannel& channel, const Arguments&... arguments)
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
#else
        if (!willLog(channel, WTFLogLevelAlways))
            return;

        log(channel, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const Arguments&... arguments)
    {
        if (!willLog(channel, WTFLogLevelError))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const Arguments&... arguments)
    {
        if (!willLog(channel, WTFLogLevelWarning))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void notice(WTFLogChannel& channel, const Arguments&... arguments)
    {
        if (!willLog(channel, WTFLogLevelNotice))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const Arguments&... arguments)
    {
        if (!willLog(channel, WTFLogLevelInfo))
            return;

        log(channel, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const Arguments&... arguments)
    {
        if (!willLog(channel, WTFLogLevelDebug))
            return;

        log(channel, arguments...);
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

    struct MethodAndPointer {
        MethodAndPointer(const char* methodName, void* objectPtr)
            : methodName(methodName)
            , objectPtr(reinterpret_cast<uintptr_t>(objectPtr))
        {
        }

        const char* methodName;
        uintptr_t objectPtr;
    };

private:
    Logger(const void* owner) { m_owner = owner; }

    template<typename... Argument>
    static inline void log(WTFLogChannel& channel, const Argument&... arguments)
    {
        String string = makeString(LogArgument<Argument>::toString(arguments)...);

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", string.utf8().data());
#else
        os_log(channel.osLogChannel, "%{public}s", string.utf8().data());
#endif
    }

    const void* m_owner;
    bool m_enabled { true };
};

template <>
struct LogArgument<Logger::MethodAndPointer> {
    static String toString(const Logger::MethodAndPointer& value)
    {
        StringBuilder builder;
        builder.append(value.methodName);
        builder.appendLiteral("(0x");
        appendUnsigned64AsHex(value.objectPtr, builder);
        builder.appendLiteral(") ");
        return builder.toString();
    }
};

} // namespace PAL

