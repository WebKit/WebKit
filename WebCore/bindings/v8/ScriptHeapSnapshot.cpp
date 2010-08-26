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
#include "ScriptHeapSnapshot.h"

#include "InspectorValues.h"
#include "V8Binding.h"
#include <v8-profiler.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

String ScriptHeapSnapshot::title() const
{
    v8::HandleScope scope;
    return toWebCoreString(m_snapshot->GetTitle());
}

unsigned int ScriptHeapSnapshot::uid() const
{
    return m_snapshot->GetUid();
}

static PassRefPtr<InspectorObject> buildInspectorObjectFor(const v8::HeapGraphNode* root)
{
    v8::HandleScope scope;
    RefPtr<InspectorObject> result = InspectorObject::create();
    RefPtr<InspectorObject> lowLevels = InspectorObject::create();
    RefPtr<InspectorObject> entries = InspectorObject::create();
    RefPtr<InspectorObject> children = InspectorObject::create();
    for (int i = 0, count = root->GetChildrenCount(); i < count; ++i) {
        const v8::HeapGraphNode* node = root->GetChild(i)->GetToNode();
        if (node->GetType() == v8::HeapGraphNode::kInternal) {
            RefPtr<InspectorObject> lowLevel = InspectorObject::create();
            lowLevel->setNumber("count", node->GetInstancesCount());
            lowLevel->setNumber("size", node->GetSelfSize());
            lowLevel->setString("type", toWebCoreString(node->GetName()));
            lowLevels->setObject(toWebCoreString(node->GetName()), lowLevel);
        } else if (node->GetInstancesCount()) {
            RefPtr<InspectorObject> entry = InspectorObject::create();
            entry->setString("constructorName", toWebCoreString(node->GetName()));
            entry->setNumber("count", node->GetInstancesCount());
            entry->setNumber("size", node->GetSelfSize());
            entries->setObject(toWebCoreString(node->GetName()), entry);
        } else {
            RefPtr<InspectorObject> entry = InspectorObject::create();
            entry->setString("constructorName", toWebCoreString(node->GetName()));
            for (int j = 0, count = node->GetChildrenCount(); j < count; ++j) {
                const v8::HeapGraphEdge* v8Edge = node->GetChild(j);
                const v8::HeapGraphNode* v8Child = v8Edge->GetToNode();
                RefPtr<InspectorObject> child = InspectorObject::create();
                child->setString("constructorName", toWebCoreString(v8Child->GetName()));
                child->setNumber("count", v8Edge->GetName()->ToInteger()->Value());
                entry->setObject(String::number(reinterpret_cast<unsigned long long>(v8Child)), child);
            }
            children->setObject(String::number(reinterpret_cast<unsigned long long>(node)), entry);
        }
    }
    result->setObject("lowlevels", lowLevels);
    result->setObject("entries", entries);
    result->setObject("children", children);
    return result.release();
}

PassRefPtr<InspectorObject> ScriptHeapSnapshot::buildInspectorObjectForHead() const
{
    return buildInspectorObjectFor(m_snapshot->GetRoot());
}

} // namespace WebCore
