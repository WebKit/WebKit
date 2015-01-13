/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington.
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
 
#ifndef TimelineRecordFactory_h
#define TimelineRecordFactory_h

#include "URL.h"
#include <inspector/InspectorValues.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class Profile;
}

namespace Inspector {
struct ScriptBreakpointAction;
}

namespace WebCore {

    class Event;
    class FloatQuad;
    class ResourceRequest;
    class ResourceResponse;
    class ScriptProfile;

    class TimelineRecordFactory {
    public:
        static Ref<Inspector::InspectorObject> createGenericRecord(double startTime, int maxCallStackDepth);
        static Ref<Inspector::InspectorObject> createBackgroundRecord(double startTime, const String& thread);

        static Ref<Inspector::InspectorObject> createGCEventData(const size_t usedHeapSizeDelta);

        static Ref<Inspector::InspectorObject> createFunctionCallData(const String& scriptName, int scriptLine);
        static Ref<Inspector::InspectorObject> createConsoleProfileData(const String& title);

        static Ref<Inspector::InspectorObject> createProbeSampleData(const Inspector::ScriptBreakpointAction&, unsigned sampleId);

        static Ref<Inspector::InspectorObject> createEventDispatchData(const Event&);

        static Ref<Inspector::InspectorObject> createGenericTimerData(int timerId);

        static Ref<Inspector::InspectorObject> createTimerInstallData(int timerId, int timeout, bool singleShot);

        static Ref<Inspector::InspectorObject> createXHRReadyStateChangeData(const String& url, int readyState);

        static Ref<Inspector::InspectorObject> createXHRLoadData(const String& url);

        static Ref<Inspector::InspectorObject> createEvaluateScriptData(const String&, double lineNumber);

        static Ref<Inspector::InspectorObject> createTimeStampData(const String&);

        static Ref<Inspector::InspectorObject> createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout);

        static Ref<Inspector::InspectorObject> createMarkData(bool isMainFrame);

        static Ref<Inspector::InspectorObject> createParseHTMLData(unsigned startLine);

        static Ref<Inspector::InspectorObject> createAnimationFrameData(int callbackId);

        static Ref<Inspector::InspectorObject> createPaintData(const FloatQuad&);

        static void appendLayoutRoot(Inspector::InspectorObject* data, const FloatQuad&);

        static void appendProfile(Inspector::InspectorObject*, RefPtr<JSC::Profile>&&);

#if ENABLE(WEB_SOCKETS)
        static inline Ref<Inspector::InspectorObject> createWebSocketCreateData(unsigned long identifier, const URL& url, const String& protocol)
        {
            Ref<Inspector::InspectorObject> data = Inspector::InspectorObject::create();
            data->setInteger("identifier", identifier);
            data->setString("url", url.string());
            if (!protocol.isNull())
                data->setString("webSocketProtocol", protocol);
            return WTF::move(data);
        }

        static inline Ref<Inspector::InspectorObject> createGenericWebSocketData(unsigned long identifier)
        {
            Ref<Inspector::InspectorObject> data = Inspector::InspectorObject::create();
            data->setInteger("identifier", identifier);
            return WTF::move(data);
        }
#endif
    private:
        TimelineRecordFactory() { }
    };

} // namespace WebCore

#endif // !defined(TimelineRecordFactory_h)
