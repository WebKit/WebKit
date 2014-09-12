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

#include "config.h"

#if ENABLE(INSPECTOR)

#include "TimelineRecordFactory.h"

#include "Event.h"
#include "FloatQuad.h"
#include "InspectorWebProtocolTypes.h"
#include "IntRect.h"
#include "JSMainThreadExecState.h"
#include "LayoutRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <inspector/ScriptBreakpoint.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <profiler/Profile.h>
#include <wtf/CurrentTime.h>

using namespace Inspector;

namespace WebCore {

PassRefPtr<InspectorObject> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth)
{
    RefPtr<InspectorObject> record = InspectorObject::create();
    record->setDouble("startTime", startTime);

    if (maxCallStackDepth) {
        RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(JSMainThreadExecState::currentState(), maxCallStackDepth);
        if (stackTrace && stackTrace->size())
            record->setValue("stackTrace", stackTrace->buildInspectorArray());
    }
    return record.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createBackgroundRecord(double startTime, const String& threadName)
{
    RefPtr<InspectorObject> record = InspectorObject::create();
    record->setDouble("startTime", startTime);
    record->setString("thread", threadName);
    return record.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createGCEventData(const size_t usedHeapSizeDelta)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("usedHeapSizeDelta", usedHeapSizeDelta);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createFunctionCallData(const String& scriptName, int scriptLine)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("scriptName", scriptName);
    data->setInteger("scriptLine", scriptLine);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createConsoleProfileData(const String& title)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("title", title);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createProbeSampleData(const ScriptBreakpointAction& action, int hitCount)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("probeId"), action.identifier);
    data->setInteger(ASCIILiteral("hitCount"), hitCount);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("type", event.type().string());
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("timerId", timerId);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createTimerInstallData(int timerId, int timeout, bool singleShot)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("timerId", timerId);
    data->setInteger("timeout", timeout);
    data->setBoolean("singleShot", singleShot);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createXHRReadyStateChangeData(const String& url, int readyState)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    data->setInteger("readyState", readyState);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createXHRLoadData(const String& url)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createEvaluateScriptData(const String& url, double lineNumber)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    data->setInteger("lineNumber", lineNumber);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createTimeStampData(const String& message)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("message", message);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createScheduleResourceRequestData(const String& url)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createResourceSendRequestData(const String& requestId, const ResourceRequest& request)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("requestId", requestId);
    data->setString("url", request.url().string());
    data->setString("requestMethod", request.httpMethod());
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createResourceReceiveResponseData(const String& requestId, const ResourceResponse& response)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("requestId", requestId);
    data->setInteger("statusCode", response.httpStatusCode());
    data->setString("mimeType", response.mimeType());
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createResourceFinishData(const String& requestId, bool didFail, double finishTime)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("requestId", requestId);
    data->setBoolean("didFail", didFail);
    if (finishTime)
        data->setDouble("networkTime", finishTime);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createReceiveResourceData(const String& requestId, int length)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("requestId", requestId);
    data->setInteger("encodedDataLength", length);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("dirtyObjects", dirtyObjects);
    data->setInteger("totalObjects", totalObjects);
    data->setBoolean("partialLayout", partialLayout);
    return data.release();
}
    
PassRefPtr<InspectorObject> TimelineRecordFactory::createDecodeImageData(const String& imageType)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setString("imageType", imageType);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createResizeImageData(bool shouldCache)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setBoolean("cached", shouldCache);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createMarkData(bool isMainFrame)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setBoolean("isMainFrame", isMainFrame);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createParseHTMLData(unsigned startLine)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("startLine", startLine);
    return data.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setInteger("id", callbackId);
    return data.release();
}

static PassRefPtr<InspectorArray> createQuad(const FloatQuad& quad)
{
    RefPtr<InspectorArray> array = InspectorArray::create();
    array->pushDouble(quad.p1().x());
    array->pushDouble(quad.p1().y());
    array->pushDouble(quad.p2().x());
    array->pushDouble(quad.p2().y());
    array->pushDouble(quad.p3().x());
    array->pushDouble(quad.p3().y());
    array->pushDouble(quad.p4().x());
    array->pushDouble(quad.p4().y());
    return array.release();
}

PassRefPtr<InspectorObject> TimelineRecordFactory::createPaintData(const FloatQuad& quad)
{
    RefPtr<InspectorObject> data = InspectorObject::create();
    data->setArray("clip", createQuad(quad));
    return data.release();
}

void TimelineRecordFactory::appendLayoutRoot(InspectorObject* data, const FloatQuad& quad)
{
    data->setArray("root", createQuad(quad));
}

static PassRefPtr<Protocol::Timeline::CPUProfileNodeCall> buildInspectorObject(const JSC::ProfileNode::Call& call)
{
    RefPtr<Protocol::Timeline::CPUProfileNodeCall> result = Protocol::Timeline::CPUProfileNodeCall::create()
        .setStartTime(call.startTime())
        .setTotalTime(call.totalTime());
    return result.release();
}

static PassRefPtr<Protocol::Timeline::CPUProfileNode> buildInspectorObject(const JSC::ProfileNode* node)
{
    RefPtr<Protocol::Array<Protocol::Timeline::CPUProfileNodeCall>> calls = Protocol::Array<Protocol::Timeline::CPUProfileNodeCall>::create();
    for (const JSC::ProfileNode::Call& call : node->calls())
        calls->addItem(buildInspectorObject(call));

    RefPtr<Protocol::Timeline::CPUProfileNode> result = Protocol::Timeline::CPUProfileNode::create()
        .setId(node->id())
        .setCalls(calls.release());

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        RefPtr<Protocol::Array<Protocol::Timeline::CPUProfileNode>> children = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObject(profileNode.get()));
        result->setChildren(children);
    }

    return result.release();
}

static PassRefPtr<Protocol::Timeline::CPUProfile> buildProfileInspectorObject(const JSC::Profile* profile)
{
    RefPtr<Protocol::Array<Protocol::Timeline::CPUProfileNode>> rootNodes = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : profile->rootNode()->children())
        rootNodes->addItem(buildInspectorObject(profileNode.get()));

    RefPtr<Protocol::Timeline::CPUProfile> result = Protocol::Timeline::CPUProfile::create()
        .setRootNodes(rootNodes);

    return result.release();
}

void TimelineRecordFactory::appendProfile(InspectorObject* data, PassRefPtr<JSC::Profile> profile)
{
    data->setValue(ASCIILiteral("profile"), buildProfileInspectorObject(profile.get()));
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
