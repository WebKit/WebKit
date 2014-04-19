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
        static PassRefPtr<Inspector::InspectorObject> createGenericRecord(double startTime, int maxCallStackDepth);
        static PassRefPtr<Inspector::InspectorObject> createBackgroundRecord(double startTime, const String& thread);

        static PassRefPtr<Inspector::InspectorObject> createGCEventData(const size_t usedHeapSizeDelta);

        static PassRefPtr<Inspector::InspectorObject> createFunctionCallData(const String& scriptName, int scriptLine);

        static PassRefPtr<Inspector::InspectorObject> createProbeSampleData(const Inspector::ScriptBreakpointAction&, int hitCount);

        static PassRefPtr<Inspector::InspectorObject> createEventDispatchData(const Event&);

        static PassRefPtr<Inspector::InspectorObject> createGenericTimerData(int timerId);

        static PassRefPtr<Inspector::InspectorObject> createTimerInstallData(int timerId, int timeout, bool singleShot);

        static PassRefPtr<Inspector::InspectorObject> createXHRReadyStateChangeData(const String& url, int readyState);

        static PassRefPtr<Inspector::InspectorObject> createXHRLoadData(const String& url);

        static PassRefPtr<Inspector::InspectorObject> createEvaluateScriptData(const String&, double lineNumber);

        static PassRefPtr<Inspector::InspectorObject> createTimeStampData(const String&);

        static PassRefPtr<Inspector::InspectorObject> createResourceSendRequestData(const String& requestId, const ResourceRequest&);

        static PassRefPtr<Inspector::InspectorObject> createScheduleResourceRequestData(const String&);

        static PassRefPtr<Inspector::InspectorObject> createResourceReceiveResponseData(const String& requestId, const ResourceResponse&);

        static PassRefPtr<Inspector::InspectorObject> createReceiveResourceData(const String& requestId, int length);

        static PassRefPtr<Inspector::InspectorObject> createResourceFinishData(const String& requestId, bool didFail, double finishTime);

        static PassRefPtr<Inspector::InspectorObject> createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout);

        static PassRefPtr<Inspector::InspectorObject> createDecodeImageData(const String& imageType);

        static PassRefPtr<Inspector::InspectorObject> createResizeImageData(bool shouldCache);

        static PassRefPtr<Inspector::InspectorObject> createMarkData(bool isMainFrame);

        static PassRefPtr<Inspector::InspectorObject> createParseHTMLData(unsigned startLine);

        static PassRefPtr<Inspector::InspectorObject> createAnimationFrameData(int callbackId);

        static PassRefPtr<Inspector::InspectorObject> createPaintData(const FloatQuad&);

        static void appendLayoutRoot(Inspector::InspectorObject* data, const FloatQuad&);

        static void appendProfile(Inspector::InspectorObject*, PassRefPtr<JSC::Profile>);

#if ENABLE(WEB_SOCKETS)
        static inline PassRefPtr<Inspector::InspectorObject> createWebSocketCreateData(unsigned long identifier, const URL& url, const String& protocol)
        {
            RefPtr<Inspector::InspectorObject> data = Inspector::InspectorObject::create();
            data->setNumber("identifier", identifier);
            data->setString("url", url.string());
            if (!protocol.isNull())
                data->setString("webSocketProtocol", protocol);
            return data.release();
        }

        static inline PassRefPtr<Inspector::InspectorObject> createGenericWebSocketData(unsigned long identifier)
        {
            RefPtr<Inspector::InspectorObject> data = Inspector::InspectorObject::create();
            data->setNumber("identifier", identifier);
            return data.release();
        }
#endif
    private:
        TimelineRecordFactory() { }
    };

} // namespace WebCore

#endif // !defined(TimelineRecordFactory_h)
