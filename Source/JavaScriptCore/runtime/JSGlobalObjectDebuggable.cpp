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

#include "config.h"
#include "JSGlobalObjectDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "InspectorAgentBase.h"
#include "InspectorFrontendChannel.h"
#include "JSGlobalObject.h"
#include "JSGlobalObjectInspectorController.h"
#include "JSLock.h"
#include "RemoteInspector.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/threads/BinarySemaphore.h>

using namespace Inspector;

namespace JSC {

WTF_MAKE_TZONE_ALLOCATED_IMPL(JSGlobalObjectDebuggable);

Ref<JSGlobalObjectDebuggable> JSGlobalObjectDebuggable::create(JSGlobalObject& globalObject)
{
    return adoptRef(*new JSGlobalObjectDebuggable(globalObject));
}

JSGlobalObjectDebuggable::JSGlobalObjectDebuggable(JSGlobalObject& globalObject)
    : m_globalObject(&globalObject)
{
}

String JSGlobalObjectDebuggable::name() const
{
    if (m_globalObject && !m_globalObject->name().isEmpty())
        return m_globalObject->name();
    return "JSContext"_s;
}

void JSGlobalObjectDebuggable::connect(FrontendChannel& frontendChannel, bool automaticInspection, bool immediatelyPause)
{
    if (!m_globalObject)
        return;

    JSLockHolder locker(&m_globalObject->vm());
    m_globalObject->inspectorController().connectFrontend(frontendChannel, automaticInspection, immediatelyPause);
}

void JSGlobalObjectDebuggable::disconnect(FrontendChannel& frontendChannel)
{
    if (!m_globalObject)
        return;

    JSLockHolder locker(&m_globalObject->vm());

    m_globalObject->inspectorController().disconnectFrontend(frontendChannel);
}

void JSGlobalObjectDebuggable::dispatchMessageFromRemote(String&& message)
{
    if (!m_globalObject)
        return;

    JSLockHolder locker(&m_globalObject->vm());

    m_globalObject->inspectorController().dispatchMessageFromFrontend(WTFMove(message));
}

void JSGlobalObjectDebuggable::pauseWaitingForAutomaticInspection()
{
    if (!m_globalObject)
        return;

    JSC::JSLock::DropAllLocks dropAllLocks(&m_globalObject->vm());
    RemoteInspectionTarget::pauseWaitingForAutomaticInspection();
}

void JSGlobalObjectDebuggable::globalObjectDestroyed()
{
    m_globalObject = nullptr;
}

} // namespace JSC

#endif // ENABLE(REMOTE_INSPECTOR)
