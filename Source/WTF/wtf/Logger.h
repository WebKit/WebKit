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

#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/StringBuilder.h>

namespace WTF {

template<typename T>
struct LogArgument {
    template<typename U = T> static typename std::enable_if<std::is_same<U, bool>::value, String>::type toString(bool argument) { return argument ? "true"_s : "false"_s; }
    template<typename U = T> static typename std::enable_if<std::is_same<U, int>::value, String>::type toString(int argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, unsigned>::value, String>::type toString(unsigned argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, unsigned long>::value, String>::type toString(unsigned long argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, long>::value, String>::type toString(long argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, unsigned long long>::value, String>::type toString(unsigned long long argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, long long>::value, String>::type toString(long long argument) { return String::number(argument); }
    template<typename U = T> static typename std::enable_if<std::is_enum<U>::value, String>::type toString(U argument) { return String::number(static_cast<typename std::underlying_type<U>::type>(argument)); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, float>::value, String>::type toString(float argument) { return String::numberToStringFixedPrecision(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, double>::value, String>::type toString(double argument) { return String::numberToStringFixedPrecision(argument); }
    template<typename U = T> static typename std::enable_if<std::is_same<typename std::remove_reference<U>::type, AtomString>::value, String>::type toString(const AtomString& argument) { return argument.string(); }
    template<typename U = T> static typename std::enable_if<std::is_same<typename std::remove_reference<U>::type, String>::value, String>::type toString(String argument) { return argument; }
    template<typename U = T> static typename std::enable_if<std::is_same<typename std::remove_reference<U>::type, StringBuilder*>::value, String>::type toString(StringBuilder* argument) { return argument->toString(); }
    template<typename U = T> static typename std::enable_if<std::is_same<U, const char*>::value, String>::type toString(const char* argument) { return String(argument); }
    template<size_t length> static String toString(const char (&argument)[length]) { return String(argument); }
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
    static const bool value = decltype(test<C>(nullptr))::value;
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
    inline void logAlways(WTFLogChannel& channel, UNUSED_FUNCTION const Arguments&... arguments) const
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

    inline bool willLog(const WTFLogChannel& channel, WTFLogLevel level) const
    {
        if (!m_enabled)
            return false;

        if (level <= WTFLogLevel::Error)
            return true;

        if (channel.state == WTFLogChannelState::Off || level > channel.level)
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

        WTF_EXPORT String toString() const;

        const char* className { nullptr };
        const char* methodName { nullptr };
        const uintptr_t objectPtr { 0 };
    };

    static inline void addObserver(Observer& observer)
    {
        auto lock = holdLock(observerLock());
        observers().append(observer);
    }
    static inline void removeObserver(Observer& observer)
    {
        auto lock = holdLock(observerLock());
        observers().removeFirstMatching([&observer](auto anObserver) {
            return &anObserver.get() == &observer;
        });
    }

private:
    friend class AggregateLogger;

    Logger(const void* owner)
        : m_owner { owner }
    {
    }

    template<typename... Argument>
    static inline void log(WTFLogChannel& channel, WTFLogLevel level, const Argument&... arguments)
    {
        String logMessage = makeString(LogArgument<Argument>::toString(arguments)...);

#if RELEASE_LOG_DISABLED
        WTFLog(&channel, "%s", logMessage.utf8().data());
#else
        os_log(channel.osLogChannel, "%{public}s", logMessage.utf8().data());
#endif

        if (channel.state == WTFLogChannelState::Off || level > channel.level)
            return;

        auto lock = tryHoldLock(observerLock());
        if (!lock)
            return;

        for (Observer& observer : observers())
            observer.didLogMessage(channel, level, { ConsoleLogValue<Argument>::toValue(arguments)... });
    }

    static Vector<std::reference_wrapper<Observer>>& observers()
    {
        static NeverDestroyed<Vector<std::reference_wrapper<Observer>>> observers;
        return observers;
    }

    static Lock& observerLock()
    {
        static NeverDestroyed<Lock> observerLock;
        return observerLock;
    }


    bool m_enabled { true };
    const void* m_owner;
};

template<> struct LogArgument<Logger::LogSiteIdentifier> {
    static String toString(const Logger::LogSiteIdentifier& value) { return value.toString(); }
};
template<> struct LogArgument<const void*> {
    WTF_EXPORT static String toString(const void*);
};

} // namespace WTF

using WTF::Logger;
using WTF::JSONLogValue;
