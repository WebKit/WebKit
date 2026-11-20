/*
* Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "AutomationInstrumentation.h"

#if ENABLE(WEBDRIVER_BIDI)

#include "DOMWrapperWorld.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Observer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

using namespace Inspector;

namespace {
NODELETE static WeakPtr<AutomationInstrumentationClient>& automationClient()
{
    static NeverDestroyed<WeakPtr<AutomationInstrumentationClient>> s_client;
    return s_client.get();
}
}

void AutomationInstrumentation::setClient(const AutomationInstrumentationClient &client)
{
    ASSERT(!automationClient());
    automationClient() = client;
}

void AutomationInstrumentation::clearClient()
{
    automationClient().clear();
}

void AutomationInstrumentation::addMessageToConsole(FrameIdentifier frameID, const std::unique_ptr<ConsoleMessage>& message)
{
    RELEASE_ASSERT(isMainThread());
    if (RefPtr client = automationClient()) [[unlikely]]
        client->addMessageToConsole(frameID, message->source(), message->level(), message->message(), message->type(), message->timestamp());
}

void AutomationInstrumentation::scriptRealmCreated(FrameIdentifier frameID, const SecurityOriginData& origin, DOMWrapperWorld& world)
{
    if (!automationClient()) [[likely]]
        return;

    // Only notify for normal world (main page execution), not user/internal worlds.
    if (!world.isNormal())
        return;

    WTF::ensureOnMainThread([frameID, origin = origin.isolatedCopy()] {
        if (RefPtr client = automationClient().get())
            client->scriptRealmCreated(frameID, origin);
    });
}

void AutomationInstrumentation::scriptRealmDestroyed(FrameIdentifier frameID, DOMWrapperWorld& world)
{
    if (!automationClient()) [[likely]]
        return;

    // Only notify for normal world (main page execution), not user/internal worlds.
    if (!world.isNormal())
        return;

    WTF::ensureOnMainThread([frameID] {
        if (RefPtr client = automationClient().get())
            client->scriptRealmDestroyed(frameID);
    });
}

void AutomationInstrumentation::scriptDedicatedWorkerRealmCreated(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier, const SecurityOriginData& origin)
{
    if (!automationClient()) [[likely]]
        return;

    WTF::ensureOnMainThread([workerIdentifier = workerIdentifier.isolatedCopy(), ownerFrameIdentifier, ownerDocumentIdentifier, origin = origin.isolatedCopy()] {
        if (RefPtr client = automationClient())
            client->scriptDedicatedWorkerRealmCreated(workerIdentifier, ownerFrameIdentifier, ownerDocumentIdentifier, origin);
    });
}

void AutomationInstrumentation::scriptDedicatedWorkerRealmDestroyed(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier)
{
    if (!automationClient()) [[likely]]
        return;

    WTF::ensureOnMainThread([workerIdentifier = workerIdentifier.isolatedCopy(), ownerFrameIdentifier, ownerDocumentIdentifier] {
        if (RefPtr client = automationClient())
            client->scriptDedicatedWorkerRealmDestroyed(workerIdentifier, ownerFrameIdentifier, ownerDocumentIdentifier);
    });
}

} // namespace WebCore

#endif
