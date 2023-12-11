/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#include "RemoteInspectionTarget.h"
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>

namespace Inspector {
class FrontendChannel;
enum class DisconnectReason;
}

namespace JSC {

class JSGlobalObject;

class JSGlobalObjectDebuggable final : public Inspector::RemoteInspectionTarget {
    WTF_MAKE_TZONE_ALLOCATED(JSGlobalObjectDebuggable);
    WTF_MAKE_NONCOPYABLE(JSGlobalObjectDebuggable);
public:
    JSGlobalObjectDebuggable(JSGlobalObject&);
    ~JSGlobalObjectDebuggable() final { }

    Inspector::RemoteControllableTarget::Type type() const final { return m_type; }
    void setIsITML() { m_type = Inspector::RemoteControllableTarget::Type::ITML; }

    String name() const final;
    bool hasLocalDebugger() const final { return false; }

    void connect(Inspector::FrontendChannel&, bool isAutomaticConnection = false, bool immediatelyPause = false) final;
    void disconnect(Inspector::FrontendChannel&) final;
    void dispatchMessageFromRemote(String&& message) final;

    bool automaticInspectionAllowed() const final { return true; }
    void pauseWaitingForAutomaticInspection() final;

private:
    JSGlobalObject& m_globalObject;
    Inspector::RemoteControllableTarget::Type m_type { Inspector::RemoteControllableTarget::Type::JavaScript };
};

} // namespace JSC

SPECIALIZE_TYPE_TRAITS_BEGIN(JSC::JSGlobalObjectDebuggable)
    static bool isType(const Inspector::RemoteControllableTarget& target)
    {
        return target.type() == Inspector::RemoteControllableTarget::Type::JavaScript
            || target.type() == Inspector::RemoteControllableTarget::Type::ITML;
    }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(REMOTE_INSPECTOR)
