/*
 * Copyright (c) 2010, Google Inc. All rights reserved.
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
#include "ScriptProfiler.h"

#include "InspectorValues.h"
#include "RetainedDOMInfo.h"
#include "V8Binding.h"
#include "V8Node.h"

#include <v8-profiler.h>

namespace WebCore {

void ScriptProfiler::start(ScriptState* state, const String& title)
{
    v8::HandleScope hs;
    v8::CpuProfiler::StartProfiling(v8String(title));
}

PassRefPtr<ScriptProfile> ScriptProfiler::stop(ScriptState* state, const String& title)
{
    v8::HandleScope hs;
    const v8::CpuProfile* profile = state ?
        v8::CpuProfiler::StopProfiling(v8String(title), state->context()->GetSecurityToken()) :
        v8::CpuProfiler::StopProfiling(v8String(title));
    return profile ? ScriptProfile::create(profile) : 0;
}

void ScriptProfiler::collectGarbage()
{
    // NOTE : There is currently no direct way to collect memory from the v8 C++ API
    // but notifying low-memory forces a mark-compact, which is exactly what we want
    // in this case.
    v8::V8::LowMemoryNotification();
}

namespace {

class ActivityControlAdapter : public v8::ActivityControl {
public:
    ActivityControlAdapter(ScriptProfiler::HeapSnapshotProgress* progress)
            : m_progress(progress), m_firstReport(true) { }
    ControlOption ReportProgressValue(int done, int total)
    {
        ControlOption result = m_progress->isCanceled() ? kAbort : kContinue;
        if (m_firstReport) {
            m_firstReport = false;
            m_progress->Start(total);
        } else
            m_progress->Worked(done);
        if (done >= total)
            m_progress->Done();
        return result;
    }
private:
    ScriptProfiler::HeapSnapshotProgress* m_progress;
    bool m_firstReport;
};

} // namespace

PassRefPtr<ScriptHeapSnapshot> ScriptProfiler::takeHeapSnapshot(const String& title, HeapSnapshotProgress* control)
{
    v8::HandleScope hs;
    const v8::HeapSnapshot* snapshot = 0;
    if (control) {
        ActivityControlAdapter adapter(control);
        snapshot = v8::HeapProfiler::TakeSnapshot(v8String(title), v8::HeapSnapshot::kFull, &adapter);
    } else
        snapshot = v8::HeapProfiler::TakeSnapshot(v8String(title), v8::HeapSnapshot::kAggregated);
    return snapshot ? ScriptHeapSnapshot::create(snapshot) : 0;
}

static v8::RetainedObjectInfo* retainedDOMInfo(uint16_t classId, v8::Handle<v8::Value> wrapper)
{
    ASSERT(classId == v8DOMSubtreeClassId);
    if (!wrapper->IsObject())
        return 0;
    Node* node = V8Node::toNative(wrapper.As<v8::Object>());
    return node ? new RetainedDOMInfo(node) : 0;
}

void ScriptProfiler::initialize()
{
    v8::HeapProfiler::DefineWrapperClass(v8DOMSubtreeClassId, &retainedDOMInfo);
}


} // namespace WebCore
