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
#include "TimelineRecordFactory.h"

#include "Event.h"
#include "FloatQuad.h"
#include "IntRect.h"
#include "JSMainThreadExecState.h"
#include "LayoutRect.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <inspector/InspectorProtocolObjects.h>
#include <inspector/ScriptBreakpoint.h>
#include <inspector/ScriptCallStack.h>
#include <inspector/ScriptCallStackFactory.h>
#include <profiler/Profile.h>

using namespace Inspector;

namespace WebCore {

Ref<InspectorObject> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth)
{
    Ref<InspectorObject> record = InspectorObject::create();
    record->setDouble("startTime", startTime);

    if (maxCallStackDepth) {
        RefPtr<ScriptCallStack> stackTrace = createScriptCallStack(JSMainThreadExecState::currentState(), maxCallStackDepth);
        if (stackTrace && stackTrace->size())
            record->setValue("stackTrace", stackTrace->buildInspectorArray());
    }
    return WTF::move(record);
}

Ref<InspectorObject> TimelineRecordFactory::createBackgroundRecord(double startTime, const String& threadName)
{
    Ref<InspectorObject> record = InspectorObject::create();
    record->setDouble("startTime", startTime);
    record->setString("thread", threadName);
    return WTF::move(record);
}

Ref<InspectorObject> TimelineRecordFactory::createGCEventData(const size_t usedHeapSizeDelta)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("usedHeapSizeDelta", usedHeapSizeDelta);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createFunctionCallData(const String& scriptName, int scriptLine)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("scriptName", scriptName);
    data->setInteger("scriptLine", scriptLine);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createConsoleProfileData(const String& title)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("title", title);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createProbeSampleData(const ScriptBreakpointAction& action, unsigned sampleId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger(ASCIILiteral("probeId"), action.identifier);
    data->setInteger(ASCIILiteral("sampleId"), sampleId);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("type", event.type().string());
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("timerId", timerId);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createTimerInstallData(int timerId, int timeout, bool singleShot)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("timerId", timerId);
    data->setInteger("timeout", timeout);
    data->setBoolean("singleShot", singleShot);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createXHRReadyStateChangeData(const String& url, int readyState)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    data->setInteger("readyState", readyState);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createXHRLoadData(const String& url)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createEvaluateScriptData(const String& url, double lineNumber)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("url", url);
    data->setInteger("lineNumber", lineNumber);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createTimeStampData(const String& message)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setString("message", message);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createLayoutData(unsigned dirtyObjects, unsigned totalObjects, bool partialLayout)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("dirtyObjects", dirtyObjects);
    data->setInteger("totalObjects", totalObjects);
    data->setBoolean("partialLayout", partialLayout);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createMarkData(bool isMainFrame)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setBoolean("isMainFrame", isMainFrame);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createParseHTMLData(unsigned startLine)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("startLine", startLine);
    return WTF::move(data);
}

Ref<InspectorObject> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setInteger("id", callbackId);
    return WTF::move(data);
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
    return WTF::move(array);
}

Ref<InspectorObject> TimelineRecordFactory::createPaintData(const FloatQuad& quad)
{
    Ref<InspectorObject> data = InspectorObject::create();
    data->setArray("clip", createQuad(quad));
    return WTF::move(data);
}

void TimelineRecordFactory::appendLayoutRoot(InspectorObject* data, const FloatQuad& quad)
{
    data->setArray("root", createQuad(quad));
}

static Ref<Protocol::Timeline::CPUProfileNodeAggregateCallInfo> buildAggregateCallInfoInspectorObject(const JSC::ProfileNode* node)
{
    double startTime = node->calls()[0].startTime();
    double endTime = node->calls().last().startTime() + node->calls().last().elapsedTime();

    double totalTime = 0;
    for (const JSC::ProfileNode::Call& call : node->calls())
        totalTime += call.elapsedTime();

    return Protocol::Timeline::CPUProfileNodeAggregateCallInfo::create()
        .setCallCount(node->calls().size())
        .setStartTime(startTime)
        .setEndTime(endTime)
        .setTotalTime(totalTime)
        .release();
}

static Ref<Protocol::Timeline::CPUProfileNode> buildInspectorObject(const JSC::ProfileNode* node)
{
    auto result = Protocol::Timeline::CPUProfileNode::create()
        .setId(node->id())
        .setCallInfo(buildAggregateCallInfoInspectorObject(node))
        .release();

    if (!node->functionName().isEmpty())
        result->setFunctionName(node->functionName());

    if (!node->url().isEmpty()) {
        result->setUrl(node->url());
        result->setLineNumber(node->lineNumber());
        result->setColumnNumber(node->columnNumber());
    }

    if (!node->children().isEmpty()) {
        auto children = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
        for (RefPtr<JSC::ProfileNode> profileNode : node->children())
            children->addItem(buildInspectorObject(profileNode.get()));
        result->setChildren(WTF::move(children));
    }

    return WTF::move(result);
}

static Ref<Protocol::Timeline::CPUProfile> buildProfileInspectorObject(const JSC::Profile* profile)
{
    auto rootNodes = Protocol::Array<Protocol::Timeline::CPUProfileNode>::create();
    for (RefPtr<JSC::ProfileNode> profileNode : profile->rootNode()->children())
        rootNodes->addItem(buildInspectorObject(profileNode.get()));

    return Protocol::Timeline::CPUProfile::create()
        .setRootNodes(WTF::move(rootNodes))
        .release();
}

void TimelineRecordFactory::appendProfile(InspectorObject* data, RefPtr<JSC::Profile>&& profile)
{
    data->setValue(ASCIILiteral("profile"), buildProfileInspectorObject(profile.get()));
}

} // namespace WebCore
