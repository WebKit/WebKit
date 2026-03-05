/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "AXCoreObject.h"
#include "AXObjectCache.h"
#include <tuple>
#include <wtf/MonotonicTime.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

enum class AXLoggingOptions : uint8_t {
    MainThread = 1 << 0, // Logs messages on the main thread.
    OffMainThread = 1 << 1, // Logs messages off the main thread.
};

enum class AXStreamOptions : uint16_t {
    ObjectID = 1 << 0,
    Role = 1 << 1,
    ParentID = 1 << 2,
    IdentifierAttribute = 1 << 3,
    OuterHTML = 1 << 4,
    DisplayContents = 1 << 5,
    Address = 1 << 6,
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    TextRuns = 1 << 7,
#endif
    RendererOrNode = 1 << 8,
};

#if !LOG_DISABLED

class AXLogger final {
public:
    AXLogger() = default;
    AXLogger(const String& methodName);
    ~AXLogger();
    void log(const String&);
    void log(const char*);
    void log(const AXCoreObject&);
    void log(RefPtr<AXCoreObject>);
    void log(const Vector<Ref<AXCoreObject>>&);
    void log(const std::pair<Ref<AccessibilityObject>, AXNotificationWithData>&);
    void log(const std::pair<RefPtr<AXCoreObject>, AXNotification>&);
    void log(const AccessibilitySearchCriteria&);
    void log(AccessibilityObjectInclusion);
    void log(AXRelation);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void log(AXIsolatedTree&);
#endif
    void log(AXObjectCache&);
    static void add(TextStream&, const RefPtr<AXCoreObject>&, bool recursive = false);
    void log(const String&, const AXObjectCache::DeferredCollection&);
private:
    bool shouldLog();
    String m_methodName;
    MonotonicTime m_startTime;
};

#define AXTRACE(methodName) AXLogger axLogger(methodName)
#define AXLOG(x) axLogger.log(x)
#define AXLOGDeferredCollection(name, collection) axLogger.log(name, collection)

#else

#define AXTRACE(methodName) (void)0
#define AXLOG(x) (void)0
#define AXLOGDeferredCollection(name, collection) (void)0

#endif // !LOG_DISABLED

void streamAXCoreObject(TextStream&, const AXCoreObject&, const OptionSet<AXStreamOptions>&);
void streamSubtree(TextStream&, const Ref<AXCoreObject>&, const OptionSet<AXStreamOptions>&);

// Converts the following types to strings:
//  - AXCoreObject& and AXCoreObject*
//  - RefPtr/Ref AXCoreObject
//  - WTF::String
//  - Everything else -> passed through
template<typename T>
decltype(auto) convertAXLogArg(T&& arg)
{
    if constexpr (std::is_pointer_v<std::remove_cvref_t<T>> && std::derived_from<std::remove_pointer_t<std::remove_cvref_t<T>>, AXCoreObject>)
        return arg ? arg->debugDescription().utf8() : CString("(null)"_s);
    else if constexpr (std::derived_from<std::remove_cvref_t<T>, AXCoreObject>)
        return arg.debugDescription().utf8();
    else if constexpr (requires { { *arg } -> std::convertible_to<const AXCoreObject&>; })
        return arg ? arg->debugDescription().utf8() : CString("(null)"_s);
    else if constexpr (std::same_as<std::remove_cvref_t<T>, String>)
        return arg.utf8();
    else
        return std::forward<T>(arg);
}

inline const char* extractAXLogArg(const CString& string) { return string.data(); }

template<typename T> requires (!std::same_as<std::remove_cvref_t<T>, CString>)
decltype(auto) extractAXLogArg(T&& arg) { return std::forward<T>(arg); }

// Used like WTFLogAlways, but auto-converts String and AXCoreObject arguments for use with the %s format specifier.
template<typename... Args>
ALWAYS_INLINE void AXLogMessage(const char* format, Args&&... args)
{
    auto holder = std::tuple { convertAXLogArg(std::forward<Args>(args))... };
    std::apply([&](auto&&... held) {
        ALLOW_NONLITERAL_FORMAT_BEGIN
        WTFLogAlways(format, extractAXLogArg(std::forward<decltype(held)>(held))...);
        ALLOW_NONLITERAL_FORMAT_END
    }, WTF::move(holder));
}

} // namespace WebCore
