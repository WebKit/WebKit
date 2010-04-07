/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "TimelineRecordFactory.h"

#if ENABLE(INSPECTOR)

#include "Event.h"
#include "InspectorFrontend.h"
#include "IntRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "ScriptArray.h"
#include "ScriptCallStack.h"
#include "ScriptObject.h"

namespace WebCore {

ScriptObject TimelineRecordFactory::createGenericRecord(InspectorFrontend* frontend, double startTime)
{
    ScriptObject record = frontend->newScriptObject();
    record.set("startTime", startTime);

    String sourceName;
    int sourceLineNumber;
    String functionName;
    if (ScriptCallStack::callLocation(&sourceName, &sourceLineNumber, &functionName) && sourceName != "undefined") {
        record.set("callerScriptName", sourceName);
        record.set("callerScriptLine", sourceLineNumber);
        record.set("callerFunctionName", functionName);
    }
    return record;
}

ScriptObject TimelineRecordFactory::createGCEventData(InspectorFrontend* frontend, const size_t usedHeapSizeDelta)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("usedHeapSizeDelta", usedHeapSizeDelta);
    return data;
}

ScriptObject TimelineRecordFactory::createFunctionCallData(InspectorFrontend* frontend, const String& scriptName, int scriptLine)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("scriptName", scriptName);
    data.set("scriptLine", scriptLine);
    return data;
}

ScriptObject TimelineRecordFactory::createEventDispatchData(InspectorFrontend* frontend, const Event& event)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("type", event.type().string());
    return data;
}

ScriptObject TimelineRecordFactory::createGenericTimerData(InspectorFrontend* frontend, int timerId)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("timerId", timerId);
    return data;
}

ScriptObject TimelineRecordFactory::createTimerInstallData(InspectorFrontend* frontend, int timerId, int timeout, bool singleShot)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("timerId", timerId);
    data.set("timeout", timeout);
    data.set("singleShot", singleShot);
    return data;
}

ScriptObject TimelineRecordFactory::createXHRReadyStateChangeData(InspectorFrontend* frontend, const String& url, int readyState)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    data.set("readyState", readyState);
    return data;
}

ScriptObject TimelineRecordFactory::createXHRLoadData(InspectorFrontend* frontend, const String& url)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    return data;
}

ScriptObject TimelineRecordFactory::createEvaluateScriptData(InspectorFrontend* frontend, const String& url, double lineNumber) 
{
    ScriptObject data = frontend->newScriptObject();
    data.set("url", url);
    data.set("lineNumber", lineNumber);
    return data;
}

ScriptObject TimelineRecordFactory::createMarkTimelineData(InspectorFrontend* frontend, const String& message) 
{
    ScriptObject data = frontend->newScriptObject();
    data.set("message", message);
    return data;
}


ScriptObject TimelineRecordFactory::createResourceSendRequestData(InspectorFrontend* frontend, unsigned long identifier, bool isMainResource, const ResourceRequest& request)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("url", request.url().string());
    data.set("requestMethod", request.httpMethod());
    data.set("isMainResource", isMainResource);
    return data;
}

ScriptObject TimelineRecordFactory::createResourceReceiveResponseData(InspectorFrontend* frontend, unsigned long identifier, const ResourceResponse& response)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("statusCode", response.httpStatusCode());
    data.set("mimeType", response.mimeType());
    data.set("expectedContentLength", response.expectedContentLength());
    return data;
}

ScriptObject TimelineRecordFactory::createResourceFinishData(InspectorFrontend* frontend, unsigned long identifier, bool didFail)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    data.set("didFail", didFail);
    return data;
}

ScriptObject TimelineRecordFactory::createReceiveResourceData(InspectorFrontend* frontend, unsigned long identifier)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("identifier", identifier);
    return data;
}
    
ScriptObject TimelineRecordFactory::createPaintData(InspectorFrontend* frontend, const IntRect& rect)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("x", rect.x());
    data.set("y", rect.y());
    data.set("width", rect.width());
    data.set("height", rect.height());
    return data;
}

ScriptObject TimelineRecordFactory::createParseHTMLData(InspectorFrontend* frontend, unsigned int length, unsigned int startLine)
{
    ScriptObject data = frontend->newScriptObject();
    data.set("length", length);
    data.set("startLine", startLine);
    return data;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
