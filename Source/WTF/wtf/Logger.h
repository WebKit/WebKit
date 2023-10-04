/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringBuilder.h>

#if ENABLE(JOURNALD_LOG)
#define SD_JOURNAL_SUPPRESS_LOCATION
#include <systemd/sd-journal.h>
#endif

namespace WTF {

template<typename T>
struct LogArgument {
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, bool>, String> toString(bool argument) { return argument ? "true"_s : "false"_s; }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, int>, String> toString(int argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, unsigned>, String> toString(unsigned argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, unsigned long>, String> toString(unsigned long argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, long>, String> toString(long argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, unsigned long long>, String> toString(unsigned long long argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, long long>, String> toString(long long argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, unsigned short>, String> toString(unsigned short argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, short>, String> toString(short argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_enum_v<U>, String> toString(U argument) { return String::number(static_cast<typename std::underlying_type<U>::type>(argument)); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, float>, String> toString(float argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, double>, String> toString(double argument) { return String::number(argument); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<typename std::remove_reference_t<U>, AtomString>, String> toString(const AtomString& argument) { return argument.string(); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<typename std::remove_reference_t<U>, String>, String> toString(String argument) { return argument; }
    template<typename U = T> static std::enable_if_t<std::is_same_v<typename std::remove_reference_t<U>, StringBuilder*>, String> toString(StringBuilder* argument) { return argument->toString(); }
    template<typename U = T> static std::enable_if_t<std::is_same_v<U, const char*>, String> toString(const char* argument) { return String::fromLatin1(argument); }
#ifdef __OBJC__
    template<typename U = T> static std::enable_if_t<std::is_base_of_v<NSError, std::remove_pointer_t<U>>, String> toString(NSError *argument) { return String(argument.localizedDescription); }
    template<typename U = T> static std::enable_if_t<std::is_base_of_v<NSObject, std::remove_pointer_t<U>>, String> toString(NSObject *argument) { return String(argument.description); }
    template<typename U = T> static std::enable_if_t<std::is_base_of_v<NSProxy, std::remove_pointer_t<U>>, String> toString(NSProxy *argument) { return String(argument.description); }
#endif
    template<size_t length> static String toString(const char (&argument)[length]) { return String::fromLatin1(argument); }
};

struct JSONLogValue {
    enum class Type { String, JSON };
    Type type { Type::JSON };
    String value;
};

template<class C>
class HasToJSONString {
    template <class T> static std::true_type testSignature(String (T::*)() const);

    template <class T> static decltype(testSignature(&T::toJSONString)) test(std::nullptr_t);
    template <class T> static std::false_type test(...);

public:
    static constexpr bool value = decltype(test<C>(nullptr))::value;
};

template<typename Argument, bool hasJSON = HasToJSONString<Argument>::value>
struct ConsoleLogValueImpl;

template<typename Argument>
struct ConsoleLogValueImpl<Argument, true> {
    static JSONLogValue toValue(const Argument& value)
    {
        return JSONLogValue { JSONLogValue::Type::JSON, value.toJSONString() };
    }
};

template<typename Argument>
struct ConsoleLogValueImpl<Argument, false> {
    static JSONLogValue toValue(const Argument& value)
    {
        return JSONLogValue { JSONLogValue::Type::String, LogArgument<Argument>::toString(value) };
    }
};

template<typename Argument, bool hasJSON = std::is_class<Argument>::value>
struct ConsoleLogValue;

template<typename Argument>
struct ConsoleLogValue<Argument, true> {
    static JSONLogValue toValue(const Argument& value)
    {
        return ConsoleLogValueImpl<Argument>::toValue(value);
    }
};

// Specialization for non-class types
template<typename Argument>
struct ConsoleLogValue<Argument, false> {
    template<typename T>
    static JSONLogValue toValue(T value)
    {
        return JSONLogValue { JSONLogValue::Type::String, LogArgument<T>::toString(value) };
    }
};

WTF_EXPORT_PRIVATE extern Lock loggerObserverLock;

class Logger : public ThreadSafeRefCounted<Logger> {
    WTF_MAKE_NONCOPYABLE(Logger);
public:

    class Observer {
    public:
        virtual ~Observer() = default;
        // Can be called on any thread.
        virtual void didLogMessage(const WTFLogChannel&, WTFLogLevel, Vector<JSONLogValue>&&) = 0;
    };

    static Ref<Logger> create(const void* owner)
    {
        return adoptRef(*new Logger(owner));
    }

    template<typename... Arguments>
    inline void logAlways(WTFLogChannel& channel, UNUSED_VARIADIC_PARAMS const Arguments&... arguments) const
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
#else
        if (!willLog(channel, WTFLogLevel::Always))
            return;

        log(channel, WTFLogLevel::Always, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void error(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Error))
            return;

        log(channel, WTFLogLevel::Error, arguments...);
    }

    template<typename... Arguments>
    inline void warning(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Warning))
            return;

        log(channel, WTFLogLevel::Warning, arguments...);
    }

    template<typename... Arguments>
    inline void info(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Info))
            return;

        log(channel, WTFLogLevel::Info, arguments...);
    }

    template<typename... Arguments>
    inline void debug(WTFLogChannel& channel, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Debug))
            return;

        log(channel, WTFLogLevel::Debug, arguments...);
    }

    template<typename... Arguments>
    inline void logAlwaysVerbose(WTFLogChannel& channel, const char* file, const char* function, int line, UNUSED_VARIADIC_PARAMS const Arguments&... arguments) const
    {
#if RELEASE_LOG_DISABLED
        // "Standard" WebCore logging goes to stderr, which is captured in layout test output and can generally be a problem
        //  on some systems, so don't allow it.
        UNUSED_PARAM(channel);
        UNUSED_PARAM(file);
        UNUSED_PARAM(function);
        UNUSED_PARAM(line);
#else
        if (!willLog(channel, WTFLogLevel::Always))
            return;

        logVerbose(channel, WTFLogLevel::Always, file, function, line, arguments...);
#endif
    }

    template<typename... Arguments>
    inline void errorVerbose(WTFLogChannel& channel, const char* file, const char* function, int line, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Error))
            return;

        logVerbose(channel, WTFLogLevel::Error, file, function, line, arguments...);
    }

    template<typename... Arguments>
    inline void warningVerbose(WTFLogChannel& channel, const char* file, const char* function, int line, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Warning))
            return;

        logVerbose(channel, WTFLogLevel::Warning, file, function, line, arguments...);
    }

    template<typename... Arguments>
    inline void infoVerbose(WTFLogChannel& channel, const char* file, const char* function, int line, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Info))
            return;

        logVerbose(channel, WTFLogLevel::Info, file, function, line, arguments...);
    }

    template<typename... Arguments>
    inline void debugVerbose(WTFLogChannel& channel, const char* file, const char* function, int line, const Arguments&... arguments) const
    {
        if (!willLog(channel, WTFLogLevel::Debug))
            return;

        logVerbose(channel, WTFLogLevel::Debug, file, function, line, arguments...);
    }

    inline bool willLog(const WTFLogChannel& channel, WTFLogLevel level) const
    {
        if (!m_enabled)
            return false;

#if ENABLE(JOURNALD_LOG)
        if (channel.state == WTFLogChannelState::Off)
            return false;
#endif

        if (level <= WTFLogLevel::Error)
            return true;

        if (channel.state == WTFLogChannelState::Off || level > channel.level)
            return false;

        return true;
    }

    bool enabled() const { return m_enabled; }
    void setEnabled(const void* owner, bool enabled)
    {
        ASSERT(owner == m_owner);
        if (owner == m_owner)
            m_enabled = enabled;
    }

    struct LogSiteIdentifier {
        LogSiteIdentifier(const char* methodName, const void* objectPtr)
            : methodName { methodName }
            , objectPtr { reinterpret_cast<uintptr_t>(objectPtr) }
        {
        }

        LogSiteIdentifier(const char* className, const char* methodName, const void* objectPtr)
            : className { className }
            , methodName { methodName }
            , objectPtr { reinterpret_cast<uintptr_t>(objectPtr) }
        {
        }

        WTF_EXPORT_PRIVATE String toString() const;

        const char* className { nullptr };
        const char* methodName { nullptr };
        const uintptr_t objectPtr { 0 };
    };

    static inline void addObserver(Observer& observer)
    {
        Locker locker { observerLock() };
        observers().append(observer);
    }
    static inline void removeObserver(Observer& observer)
    {
        Locker locker { observerLock() };
        observers().removeFirstMatching([&observer](auto anObserver) {
            return &anObserver.get() == &observer;
        });
    }

private:
    friend class AggregateLogger;
    friend class NativePromiseBase;

    Logger(const void* owner)
        : m_owner { owner }
    {
    }

    template<typename... Argument>
    static inline void log(WTFLogChannel& channel, WTFLogLevel level, const Argument&... arguments)
    {
        auto logMessage = makeString(LogArgument<Argument>::toString(arguments)...);

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", logMessage.utf8().data());
#elif USE(OS_LOG)
        os_log(channel.osLogChannel, "%{public}s", logMessage.utf8().data());
#elif ENABLE(JOURNALD_LOG)
        sd_journal_send("WEBKIT_SUBSYSTEM=%s", channel.subsystem, "WEBKIT_CHANNEL=%s", channel.name, "MESSAGE=%s", logMessage.utf8().data(), nullptr);
#else
        fprintf(stderr, "[%s:%s:-] %s\n", channel.subsystem, channel.name, logMessage.utf8().data());
#endif

        if (channel.state == WTFLogChannelState::Off || level > channel.level)
            return;

        if (!observerLock().tryLock())
            return;

        Locker locker { AdoptLock, observerLock() };
        for (Observer& observer : observers())
            observer.didLogMessage(channel, level, { ConsoleLogValue<Argument>::toValue(arguments)... });
    }

    template<typename... Argument>
    static inline void logVerbose(WTFLogChannel& channel, WTFLogLevel level, const char* file, const char* function, int line, const Argument&... arguments)
    {
        auto logMessage = makeString(LogArgument<Argument>::toString(arguments)...);

#if RELEASE_LOG_DISABLED
        WTFLogVerbose(file, line, function, &channel, "%s", logMessage.utf8().data());
#elif USE(OS_LOG)
        os_log(channel.osLogChannel, "%{public}s", logMessage.utf8().data());
        UNUSED_PARAM(file);
        UNUSED_PARAM(line);
        UNUSED_PARAM(function);
#elif ENABLE(JOURNALD_LOG)
        auto fileString = makeString("CODE_FILE="_s, file);
        auto lineString = makeString("CODE_LINE="_s, line);
        sd_journal_send_with_location(fileString.utf8().data(), lineString.utf8().data(), function, "WEBKIT_SUBSYSTEM=%s", channel.subsystem, "WEBKIT_CHANNEL=%s", channel.name, "MESSAGE=%s", logMessage.utf8().data(), nullptr);
#else
        fprintf(stderr, "[%s:%s:-] %s FILE=%s:%d %s\n", channel.subsystem, channel.name, logMessage.utf8().data(), file, line, function);
#endif

        if (channel.state == WTFLogChannelState::Off || level > channel.level)
            return;

        if (!observerLock().tryLock())
            return;

        Locker locker { AdoptLock, observerLock() };
        for (Observer& observer : observers())
            observer.didLogMessage(channel, level, { ConsoleLogValue<Argument>::toValue(arguments)... });
    }

    WTF_EXPORT_PRIVATE static Vector<std::reference_wrapper<Observer>>& observers() WTF_REQUIRES_LOCK(observerLock());

    static Lock& observerLock() WTF_RETURNS_LOCK(loggerObserverLock)
    {
        return loggerObserverLock;
    }


    bool m_enabled { true };
    const void* m_owner;
};

template<> struct LogArgument<Logger::LogSiteIdentifier> {
    static String toString(const Logger::LogSiteIdentifier& value) { return value.toString(); }
};
template<> struct LogArgument<const void*> {
    WTF_EXPORT_PRIVATE static String toString(const void*);
};

#ifdef __OBJC__
template<> struct LogArgument<id> {
    static String toString(id argument)
    {
        if ([argument respondsToSelector:@selector(description)])
            return String([argument description]);
        return LogArgument<const void*>::toString((__bridge const void*)argument);
    }
};
#endif

} // namespace WTF

using WTF::Logger;
using WTF::JSONLogValue;
