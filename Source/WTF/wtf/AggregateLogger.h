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

#include <wtf/Algorithms.h>
#include <wtf/Logger.h>

namespace WTF {

class AggregateLogger : public Logger {
    WTF_MAKE_NONCOPYABLE(AggregateLogger);
public:

    static Ref<AggregateLogger> create(const void* owner)
    {
        return adoptRef(*new AggregateLogger(owner));
    }

    void addLogger(const Logger& logger)
    {
        m_loggers.add(&logger);
    }

    void removeLogger(const Logger& logger)
    {
        m_loggers.remove(&logger);
    }

    template<typename... Arguments>
    inline void logAlways(WTFLogChannel& channel, UNUSED_FUNCTION const Arguments&... arguments) const
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
#else
        log(channel, WTFLogLevel::Always, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        log(channel, WTFLogLevel::Error, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        log(channel, WTFLogLevel::Warning, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        log(channel, WTFLogLevel::Info, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        log(channel, WTFLogLevel::Debug, arguments...);
    }

    inline bool willLog(const WTFLogChannel& channel, WTFLogLevel level) const
    {
        return allOf(m_loggers, [channel, level] (auto& logger) { return logger->willLog(channel, level); });
    }

private:
    AggregateLogger(const void* owner)
        : Logger(owner)
    {
    }

    template<typename... Argument>
    inline void log(WTFLogChannel& channel, WTFLogLevel level, const Argument&... arguments) const
    {
        if (!willLog(channel, level))
            return;

        Logger::log(channel, level, arguments...);

        for (auto logger : m_loggers) {
            for (Observer& observer : logger->observers())
                observer.didLogMessage(channel, level, { ConsoleLogValue<Argument>::toValue(arguments)... });
        }
    }

    HashSet<RefPtr<const Logger>> m_loggers;
};

} // namespace WTF

using WTF::AggregateLogger;
