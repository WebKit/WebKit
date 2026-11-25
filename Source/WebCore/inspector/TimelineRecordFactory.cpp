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
#include "JSExecState.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>

namespace WebCore {

using namespace Inspector;

Ref<JSON::Object> TimelineRecordFactory::createGenericRecord(double startTime, int maxCallStackDepth)
{
    Ref<JSON::Object> record = JSON::Object::create();
    record->setDouble("startTime"_si, startTime);

    if (maxCallStackDepth) {
        Ref<ScriptCallStack> stackTrace = createScriptCallStack(JSExecState::currentState(), maxCallStackDepth);
        if (stackTrace->size())
            record->setValue("stackTrace"_si, stackTrace->buildInspectorObject());
    }
    return record;
}

Ref<JSON::Object> TimelineRecordFactory::createRenderingFrameData(StringImpl* name)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("name"_si, name);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createFunctionCallData(StringImpl* scriptName, int scriptLine, int scriptColumn)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("scriptName"_si, scriptName);
    data->setInteger("scriptLine"_si, scriptLine);
    data->setInteger("scriptColumn"_si, scriptColumn);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createConsoleProfileData(StringImpl* title)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("title"_si, title);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createProbeSampleData(JSC::BreakpointActionID actionID, unsigned sampleId)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setInteger("probeId"_si, actionID);
    data->setInteger("sampleId"_si, sampleId);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createEventDispatchData(const Event& event)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("type"_si, event.type());
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createGenericTimerData(int timerId)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setInteger("timerId"_si, timerId);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createTimerInstallData(int timerId, Seconds timeout, bool singleShot)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setInteger("timerId"_si, timerId);
    data->setInteger("timeout"_si, (int)timeout.milliseconds());
    data->setBoolean("singleShot"_si, singleShot);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createEvaluateScriptData(StringImpl* url, int lineNumber, int columnNumber)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("url"_si, url);
    data->setInteger("lineNumber"_si, lineNumber);
    data->setInteger("columnNumber"_si, columnNumber);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createTimeStampData(StringImpl* message)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("message"_si, message);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createLargestContentfulPaintData(Inspector::Protocol::DOM::NodeId nodeId, unsigned area)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setInteger("area"_si, area);
    if (nodeId)
        data->setInteger("nodeId"_si, nodeId);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createAnimationFrameData(int callbackId)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setInteger("id"_si, callbackId);
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createObserverCallbackData(StringImpl* callbackType)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("type"_si, callbackType);
    return data;
}

static Ref<JSON::Array> createQuad(const FloatQuad& quad)
{
    Ref<JSON::Array> array = JSON::Array::create();
    array->pushDouble(quad.p1().x());
    array->pushDouble(quad.p1().y());
    array->pushDouble(quad.p2().x());
    array->pushDouble(quad.p2().y());
    array->pushDouble(quad.p3().x());
    array->pushDouble(quad.p3().y());
    array->pushDouble(quad.p4().x());
    array->pushDouble(quad.p4().y());
    return array;
}

Ref<JSON::Object> TimelineRecordFactory::createPaintData(const FloatQuad& quad)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setArray("clip"_si, createQuad(quad));
    return data;
}

Ref<JSON::Object> TimelineRecordFactory::createScreenshotData(StringImpl* imageData)
{
    Ref<JSON::Object> data = JSON::Object::create();
    data->setString("imageData"_si, imageData);
    return data;
}

void TimelineRecordFactory::appendLayoutRoot(JSON::Object& data, const FloatQuad& quad)
{
    data.setArray("root"_si, createQuad(quad));
}

void TimelineRecordFactory::appendNodeId(JSON::Object& data, Inspector::Protocol::DOM::NodeId nodeId)
{
    data.setInteger("nodeId"_si, nodeId);
}

} // namespace WebCore
