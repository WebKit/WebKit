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
#if ENABLE(INSPECTOR)
#include "ScriptProfiler.h"

#include "BindingVisitors.h"
#include "MemoryInstrumentation.h"
#include "RetainedDOMInfo.h"
#include "ScriptObject.h"
#include "V8ArrayBufferView.h"
#include "V8Binding.h"
#include "V8DOMMap.h"
#include "V8Node.h"
#include "WrapperTypeInfo.h"

#include <v8-profiler.h>

namespace WebCore {

void ScriptProfiler::start(ScriptState* state, const String& title)
{
    v8::HandleScope hs;
    v8::CpuProfiler::StartProfiling(v8String(title));
}

void ScriptProfiler::startForPage(Page*, const String& title)
{
    return start(0, title);
}

#if ENABLE(WORKERS)
void ScriptProfiler::startForWorkerContext(WorkerContext*, const String& title)
{
    return start(0, title);
}
#endif

PassRefPtr<ScriptProfile> ScriptProfiler::stop(ScriptState* state, const String& title)
{
    v8::HandleScope hs;
    const v8::CpuProfile* profile = state ?
        v8::CpuProfiler::StopProfiling(v8String(title), state->context()->GetSecurityToken()) :
        v8::CpuProfiler::StopProfiling(v8String(title));
    return profile ? ScriptProfile::create(profile) : 0;
}

PassRefPtr<ScriptProfile> ScriptProfiler::stopForPage(Page*, const String& title)
{
    // Use null script state to avoid filtering by context security token.
    // All functions from all iframes should be visible from Inspector UI.
    return stop(0, title);
}

#if ENABLE(WORKERS)
PassRefPtr<ScriptProfile> ScriptProfiler::stopForWorkerContext(WorkerContext*, const String& title)
{
    return stop(0, title);
}
#endif

void ScriptProfiler::collectGarbage()
{
    v8::V8::LowMemoryNotification();
}

ScriptObject ScriptProfiler::objectByHeapObjectId(unsigned id)
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
        return ScriptObject();

    v8::HandleScope scope;
    v8::Handle<v8::Value> value = node->GetHeapValue();
    if (!value->IsObject())
        return ScriptObject();

    v8::Handle<v8::Object> object = value.As<v8::Object>();
    if (object->InternalFieldCount() >= v8DefaultWrapperInternalFieldCount) {
        v8::Handle<v8::Value> wrapper = object->GetInternalField(v8DOMWrapperObjectIndex);
        // Skip wrapper boilerplates which are like regular wrappers but don't have
        // native object.
        if (!wrapper.IsEmpty() && wrapper->IsUndefined())
            return ScriptObject();
    }
    ScriptState* scriptState = ScriptState::forContext(object->CreationContext());
    return ScriptObject(scriptState, object);
}

unsigned ScriptProfiler::getHeapObjectId(const ScriptValue& value)
{
    v8::SnapshotObjectId id = v8::HeapProfiler::GetSnapshotObjectId(value.v8Value());
    return id;
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

void ScriptProfiler::initialize()
{
    v8::HeapProfiler::DefineWrapperClass(v8DOMSubtreeClassId, &retainedDOMInfo);
}

void ScriptProfiler::visitNodeWrappers(NodeWrapperVisitor* visitor)
{
    class VisitorAdapter : public DOMWrapperMap<Node>::Visitor {
    public:
        VisitorAdapter(NodeWrapperVisitor* visitor) : m_visitor(visitor) { }

        virtual void visitDOMWrapper(DOMDataStore*, Node* node, v8::Persistent<v8::Object>)
        {
            m_visitor->visitNode(node);
        }
    private:
        NodeWrapperVisitor* m_visitor;
    } adapter(visitor);
    visitDOMNodes(&adapter);
}

void ScriptProfiler::visitExternalStrings(ExternalStringVisitor* visitor)
{
    V8PerIsolateData::current()->visitExternalStrings(visitor);
}

void ScriptProfiler::visitExternalArrays(ExternalArrayVisitor* visitor)
{
    class VisitorAdapter : public DOMWrapperMap<void>::Visitor {
    public:
        VisitorAdapter(ExternalArrayVisitor* visitor) : m_visitor(visitor) { }

        virtual void visitDOMWrapper(DOMDataStore*, void* impl, v8::Persistent<v8::Object> v8Object)
        {
            WrapperTypeInfo* type = V8DOMWrapper::domWrapperType(v8Object);
            if (!type->isSubclass(&V8ArrayBufferView::info))
                return;
            ArrayBufferView* arrayBufferView = V8ArrayBufferView::toNative(v8Object);
            m_visitor->visitJSExternalArray(arrayBufferView);
        }
    private:
        ExternalArrayVisitor* m_visitor;
    } adapter(visitor);

    getDOMObjectMap().visit(0, &adapter);

}

void ScriptProfiler::collectBindingMemoryInfo(MemoryInstrumentation* instrumentation)
{
    V8PerIsolateData* data = V8PerIsolateData::current();
    instrumentation->addRootObject(data);
}

size_t ScriptProfiler::profilerSnapshotsSize()
{
    return v8::HeapProfiler::GetMemorySizeUsedByProfiler();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
