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

#pragma once

#if ENABLE(WEBDRIVER_BIDI)

#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/ConsoleTypes.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/GlobalWindowIdentifier.h>
#include <WebCore/ScriptExecutionContextIdentifier.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Function.h>
#include <wtf/MemoryPressureHandler.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefPtr.h>
#include <wtf/WallTime.h>

namespace Inspector {
class ConsoleMessage;
}

namespace WebCore {

class DOMWindow;
class DOMWrapperWorld;
class LocalFrame;

class WEBCORE_EXPORT AutomationInstrumentationClient : public AbstractRefCountedAndCanMakeWeakPtr<AutomationInstrumentationClient> {
public:
    virtual ~AutomationInstrumentationClient() = default;

    virtual void addMessageToConsole(FrameIdentifier, const JSC::MessageSource&, const JSC::MessageLevel&, const String&, const JSC::MessageType&, const WallTime&) = 0;
    // The window identifies the realm: a frame gets a new window with each new document, and the window's JS global object
    // may not exist yet.
    virtual void scriptRealmCreated(FrameIdentifier, const GlobalWindowIdentifier&, const SecurityOriginData&) = 0;
    virtual void scriptRealmDestroyed(FrameIdentifier, const GlobalWindowIdentifier&) = 0;
    virtual void scriptDedicatedWorkerRealmCreated(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier, const SecurityOriginData&) = 0;
    virtual void scriptDedicatedWorkerRealmDestroyed(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier) = 0;
};


class WEBCORE_EXPORT AutomationInstrumentation {
public:
    NODELETE static void setClient(const AutomationInstrumentationClient&);
    NODELETE static void clearClient();

    // Only for frames, on the main thread. The UI process only reports console messages of pages that are controlled by
    // automation, so it needs the frame.
    // FIXME: Report console messages of workers too. A dedicated worker can be attributed to its owner document's frame,
    // but shared and service workers need another way to tell whether a page that is controlled by automation uses them.
    // https://bugs.webkit.org/show_bug.cgi?id=330988
    static void addMessageToConsole(FrameIdentifier, const std::unique_ptr<Inspector::ConsoleMessage>&);
    // A window's realm exists while the window's document is its frame's document, whether or not script has created the
    // window's JS global object. Reporting a realm again, or the end of a realm that a frame no longer has, is harmless.
    static void scriptRealmCreated(LocalFrame&, DOMWindow&, DOMWrapperWorld&);
    static void scriptRealmDestroyed(LocalFrame&, DOMWindow&, DOMWrapperWorld&);
    static void scriptDedicatedWorkerRealmCreated(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier, const SecurityOriginData&);
    static void scriptDedicatedWorkerRealmDestroyed(const String& workerIdentifier, FrameIdentifier ownerFrameIdentifier, ScriptExecutionContextIdentifier ownerDocumentIdentifier);
};

} // namespace WebCore

#endif
