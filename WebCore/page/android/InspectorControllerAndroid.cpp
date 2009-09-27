/*
 * Copyright 2007, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorController.h"

#include "Frame.h"
#include "Node.h"
#include "Profile.h"

// This stub file was created to avoid building and linking in all the
// Inspector codebase. If you would like to enable the Inspector, do the
// following steps:
// 1. Replace this file in WebCore/Android.mk with the common
//    implementation, ie page/InsepctorController.cpp
// 2. Add the JS API files to JavaScriptCore/Android.mk:
// ?  API/JSBase.cpp \
//      API/JSCallbackConstructor.cpp \
//      API/JSCallbackFunction.cpp \
//      API/JSCallbackObject.cpp \
//      API/JSClassRef.cpp \
//      API/JSContextRef.cpp \
//      API/JSObjectRef.cpp \
//      API/JSStringRef.cpp \
//      API/JSValueRef.cpp
// 3. Add the following LOCAL_C_INCLUDES to JavaScriptCore/Android.mk:
// ?$(LOCAL_PATH)/API \
//      $(LOCAL_PATH)/ForwardingHeaders \
//      $(LOCAL_PATH)/../../WebKit \
// 4. Rebuild WebKit
//
// Note, for a functional Inspector, you must implement InspectorClientAndroid.

namespace WebCore {

struct InspectorResource : public RefCounted<InspectorResource> {
};

struct InspectorDatabaseResource : public RefCounted<InspectorDatabaseResource> {
};

InspectorController::InspectorController(Page*, InspectorClient*)
    : m_startProfiling(this, 0)
{
}

InspectorController::~InspectorController() {}

void InspectorController::windowScriptObjectAvailable() {}
void InspectorController::didCommitLoad(DocumentLoader*) {}
void InspectorController::identifierForInitialRequest(unsigned long, DocumentLoader*, ResourceRequest const&) {}
void InspectorController::willSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, ResourceResponse const&) {}
void InspectorController::didReceiveResponse(DocumentLoader*, unsigned long, ResourceResponse const&) {}
void InspectorController::didReceiveContentLength(DocumentLoader*, unsigned long, int) {}
void InspectorController::didFinishLoading(DocumentLoader*, unsigned long) {}
void InspectorController::didLoadResourceFromMemoryCache(DocumentLoader*, ResourceRequest const&, ResourceResponse const&, int) {}
void InspectorController::frameDetachedFromParent(Frame*) {}

void InspectorController::addMessageToConsole(MessageSource, MessageLevel, JSC::ExecState*, JSC::ArgList const&, unsigned int, String const&) {}
void InspectorController::addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, const String& sourceID) {}
#if ENABLE(DATABASE)
void InspectorController::didOpenDatabase(Database*, String const&, String const&, String const&) {}
#endif
bool InspectorController::enabled() const { return false; }
void InspectorController::inspect(Node*) {}
bool InspectorController::windowVisible() { return false; }
void InspectorController::addProfile(PassRefPtr<JSC::Profile>, unsigned int, const JSC::UString&) {}
void InspectorController::inspectedPageDestroyed() {}
void InspectorController::resourceRetrievedByXMLHttpRequest(unsigned long identifier, JSC::UString& sourceString) {}

void InspectorController::inspectedWindowScriptObjectCleared(Frame* frame) {}
void InspectorController::startGroup(MessageSource source, JSC::ExecState* exec, const JSC::ArgList& arguments, unsigned lineNumber, const String& sourceURL) {}
void InspectorController::endGroup(MessageSource source, unsigned lineNumber, const String& sourceURL) {}
void InspectorController::startTiming(const JSC::UString& title) {}
bool InspectorController::stopTiming(const JSC::UString& title, double& elapsed) { return false; }
void InspectorController::count(const JSC::UString& title, unsigned lineNumber, const String& sourceID) {}

void InspectorController::mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags) {}
void InspectorController::handleMousePressOnNode(Node*) {}
void InspectorController::failedToParseSource(JSC::ExecState* exec, const JSC::SourceCode& source, int errorLine, const JSC::UString& errorMessage) {}    
void InspectorController::didParseSource(JSC::ExecState* exec, const JSC::SourceCode& source) {}
void InspectorController::didPause() {}

}  // namespace WebCore
