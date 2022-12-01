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

#include "AXObjectCache.h"
#include "AccessibilityObjectInterface.h"

namespace WebCore {

enum class AXLoggingOptions : uint8_t {
    MainThread = 1 << 0, // Logs messages on the main thread.
    OffMainThread = 1 << 1, // Logs messages off the main thread.
};

enum class AXStreamOptions : uint8_t {
    ObjectID = 1 << 0,
    Role = 1 << 1,
    ParentID = 1 << 2,
    IdentifierAttribute = 1 << 3,
    OuterHTML = 1 << 4,
    DisplayContents = 1 << 5,
    Address = 1 << 6,
};

#if !LOG_DISABLED

class AXLogger {
public:
    AXLogger() = default;
    AXLogger(const String& methodName);
    ~AXLogger();
    void log(const String&);
    void log(const char*);
    void log(const AXCoreObject&);
    void log(RefPtr<AXCoreObject>);
    void log(const Vector<RefPtr<AXCoreObject>>&);
    void log(const std::pair<RefPtr<AXCoreObject>, AXObjectCache::AXNotification>&);
    void log(const AccessibilitySearchCriteria&);
    void log(AccessibilityObjectInclusion);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void log(AXIsolatedTree&);
#endif
    void log(AXObjectCache&);
    static void add(TextStream&, const RefPtr<AXCoreObject>&, bool recursive = false);
private:
    bool shouldLog();
    String m_methodName;
};

#define AXTRACE(methodName) AXLogger axLogger(methodName)
#define AXLOG(x) axLogger.log(x)

#else

#define AXTRACE(methodName) (void)0
#define AXLOG(x) (void)0

#endif // !LOG_DISABLED

void streamAXCoreObject(TextStream&, const AXCoreObject&, const OptionSet<AXStreamOptions>&);
void streamSubtree(TextStream&, const RefPtr<AXCoreObject>&, const OptionSet<AXStreamOptions>&);

} // namespace WebCore
