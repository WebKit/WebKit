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
#include "ScriptProfile.h"

#include "InspectorValues.h"
#include "V8Binding.h"
#include <v8-profiler.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

String ScriptProfile::title() const
{
    v8::HandleScope scope;
    return toWebCoreString(m_profile->GetTitle());
}

unsigned int ScriptProfile::uid() const
{
    return m_profile->GetUid();
}

PassRefPtr<ScriptProfileNode> ScriptProfile::head() const
{
    return ScriptProfileNode::create(m_profile->GetTopDownRoot());
}

static PassRefPtr<InspectorObject> buildInspectorObjectFor(const v8::CpuProfileNode* node)
{
    v8::HandleScope handleScope;
    RefPtr<InspectorObject> result = InspectorObject::create();
    result->setString("functionName", toWebCoreString(node->GetFunctionName()));
    result->setString("url", toWebCoreString(node->GetScriptResourceName()));
    result->setNumber("lineNumber", node->GetLineNumber());
    result->setNumber("totalTime", node->GetTotalTime());
    result->setNumber("selfTime", node->GetSelfTime());
    result->setNumber("numberOfCalls", 0);
    result->setBoolean("visible", true);
    result->setNumber("callUID", node->GetCallUid());

    RefPtr<InspectorArray> children = InspectorArray::create();
    const int childrenCount = node->GetChildrenCount();
    for (int i = 0; i < childrenCount; i++) {
        const v8::CpuProfileNode* child = node->GetChild(i);
        children->pushObject(buildInspectorObjectFor(child));
    }
    result->setArray("children", children);
    return result.release();
}

PassRefPtr<InspectorObject> ScriptProfile::buildInspectorObjectForHead() const
{
    return buildInspectorObjectFor(m_profile->GetTopDownRoot());
}

} // namespace WebCore
