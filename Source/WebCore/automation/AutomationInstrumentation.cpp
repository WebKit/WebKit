/*
* Copyright (C) 2024 Igalia S.L.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "AutomationInstrumentation.h"

#if ENABLE(WEBDRIVER_BIDI)

#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Observer.h>
#include <wtf/StdLibExtras.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

using namespace Inspector;

namespace {
static WeakPtr<AutomationInstrumentationClient>& automationClient()
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

void AutomationInstrumentation::addMessageToConsole(const std::unique_ptr<ConsoleMessage>& message)
{
    if (LIKELY(!automationClient()))
        return;

    WTF::ensureOnMainThread([source = message->source(), type = message->type(), level = message->level(), messageText = message->message(), timestamp = message->timestamp()] {
        if (RefPtr client = automationClient().get())
            client->addMessageToConsole(source, level, messageText, type, timestamp);
    });
}

} // namespace WebCore

#endif
