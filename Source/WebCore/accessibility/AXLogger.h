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

#if !LOG_DISABLED

#include "AXObjectCache.h"
#include "AccessibilityObjectInterface.h"

namespace WebCore {

class AXLogger {
public:
    AXLogger() = default;
    AXLogger(const String& methodName);
    ~AXLogger();
    static void log(const String&);
    static void log(RefPtr<AXCoreObject>);
    static void log(const Vector<RefPtr<AXCoreObject>>&);
    static void log(const std::pair<RefPtr<AXCoreObject>, AXObjectCache::AXNotification>&);
    static void log(const AccessibilitySearchCriteria&);
    static void log(AccessibilityObjectInclusion);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    static void log(AXIsolatedTree&);
#endif
    static void log(AXObjectCache&);
    static void add(TextStream&, const RefPtr<AXCoreObject>&, bool recursive = false);
private:
    String m_methodName;
};

#define AXTRACE(methodName) AXLogger axLogger(methodName)
#define AXLOG(x) AXLogger::log(x)

} // namespace WebCore

#else

#define AXTRACE(methodName) (void)0
#define AXLOG(x) (void)0

#endif // !LOG_DISABLED
