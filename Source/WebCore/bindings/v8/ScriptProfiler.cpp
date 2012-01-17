/*
 * Copyright (c) 2011, Google Inc. All rights reserved.
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

#include "DOMWrapperVisitor.h"
#include "InjectedScript.h"
#include "InspectorValues.h"
#include "RetainedDOMInfo.h"
#include "V8Binding.h"
#include "V8DOMMap.h"
#include "V8Node.h"

#include <v8-profiler.h>

namespace WebCore {

#if ENABLE(INSPECTOR)
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
    v8::V8::LowMemoryNotification();
}

PassRefPtr<InspectorValue> ScriptProfiler::objectByHeapObjectId(unsigned id, InjectedScriptManager* injectedScriptManager)
{
    // As ids are unique, it doesn't matter which HeapSnapshot owns HeapGraphNode.
    // We need to find first HeapSnapshot containing a node with the specified id.
    const v8::HeapGraphNode* node = 0;
    for (int i = 0, l = v8::HeapProfiler::GetSnapshotsCount(); i < l; ++i) {
        const v8::HeapSnapshot* snapshot = v8::HeapProfiler::GetSnapshot(i);
        node = snapshot->GetNodeById(id);
        if (node)
            break;
    }
    if (!node)
        return InspectorValue::null();

    v8::HandleScope scope;
    v8::Handle<v8::Value> value = node->GetHeapValue();
    if (!value->IsObject())
        return InspectorValue::null();

    v8::Handle<v8::Object> object(value.As<v8::Object>());
    v8::Local<v8::Context> creationContext = object->CreationContext();
    v8::Context::Scope creationScope(creationContext);
    ScriptState* scriptState = ScriptState::forContext(creationContext);
    InjectedScript injectedScript = injectedScriptManager->injectedScriptFor(scriptState);
    return !injectedScript.hasNoValue() ?
            RefPtr<InspectorValue>(injectedScript.wrapObject(value, "")).release() : InspectorValue::null();
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
    ASSERT(control);
    ActivityControlAdapter adapter(control);
    const v8::HeapSnapshot* snapshot = v8::HeapProfiler::TakeSnapshot(v8String(title), v8::HeapSnapshot::kFull, &adapter);
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
#endif // ENABLE(INSPECTOR)

void ScriptProfiler::initialize()
{
#if ENABLE(INSPECTOR)
    v8::HeapProfiler::DefineWrapperClass(v8DOMSubtreeClassId, &retainedDOMInfo);
#endif // ENABLE(INSPECTOR)
}

void ScriptProfiler::visitJSDOMWrappers(DOMWrapperVisitor* visitor)
{
    class VisitorAdapter : public DOMWrapperMap<Node>::Visitor {
    public:
        VisitorAdapter(DOMWrapperVisitor* visitor) : m_visitor(visitor) { }

        virtual void visitDOMWrapper(DOMDataStore*, Node* node, v8::Persistent<v8::Object>)
        {
            m_visitor->visitNode(node);
        }
    private:
        DOMWrapperVisitor* m_visitor;
    } adapter(visitor);
    visitDOMNodes(&adapter);
}

void ScriptProfiler::visitExternalJSStrings(DOMWrapperVisitor* visitor)
{
    V8BindingPerIsolateData::current()->visitJSExternalStrings(visitor);
}

} // namespace WebCore
