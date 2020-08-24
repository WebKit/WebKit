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
 
#pragma once

#include <JavaScriptCore/DebuggerPrimitives.h>
#include <wtf/Forward.h>
#include <wtf/JSONValues.h>
#include <wtf/Seconds.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Event;
class FloatQuad;

class TimelineRecordFactory {
public:
    static Ref<JSON::Object> createGenericRecord(double startTime, int maxCallStackDepth);

    static Ref<JSON::Object> createFunctionCallData(const String& scriptName, int scriptLine, int scriptColumn);
    static Ref<JSON::Object> createConsoleProfileData(const String& title);
    static Ref<JSON::Object> createProbeSampleData(JSC::BreakpointActionID, unsigned sampleId);
    static Ref<JSON::Object> createEventDispatchData(const Event&);
    static Ref<JSON::Object> createGenericTimerData(int timerId);
    static Ref<JSON::Object> createTimerInstallData(int timerId, Seconds timeout, bool singleShot);
    static Ref<JSON::Object> createEvaluateScriptData(const String&, int lineNumber, int columnNumber);
    static Ref<JSON::Object> createTimeStampData(const String&);
    static Ref<JSON::Object> createAnimationFrameData(int callbackId);
    static Ref<JSON::Object> createObserverCallbackData(const String& callbackType);
    static Ref<JSON::Object> createPaintData(const FloatQuad&);

    static void appendLayoutRoot(JSON::Object* data, const FloatQuad&);

private:
    TimelineRecordFactory() { }
};

} // namespace WebCore
