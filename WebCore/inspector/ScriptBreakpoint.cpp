/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScriptBreakpoint.h"

#if ENABLE(INSPECTOR)

#include "InspectorValues.h"

namespace WebCore {

void ScriptBreakpoint::sourceBreakpointsFromInspectorObject(PassRefPtr<InspectorObject> breakpoints, SourceBreakpoints* sourceBreakpoints)
{
    for (InspectorObject::iterator it = breakpoints->begin(); it != breakpoints->end(); ++it) {
        bool ok;
        int lineNumber = it->first.toInt(&ok);
        if (!ok)
            continue;
        RefPtr<InspectorObject> breakpoint = it->second->asObject();
        if (!breakpoint)
            continue;
        bool enabled;
        RefPtr<InspectorValue> enabledValue = breakpoint->get("enabled");
        if (!enabledValue || !enabledValue->asBoolean(&enabled))
            continue;
        String condition;
        RefPtr<InspectorValue> conditionValue = breakpoint->get("condition");
        if (!conditionValue || !conditionValue->asString(&condition))
            continue;
        sourceBreakpoints->set(lineNumber, ScriptBreakpoint(enabled, condition));
    }
}

PassRefPtr<InspectorObject> ScriptBreakpoint::inspectorObjectFromSourceBreakpoints(const SourceBreakpoints& sourceBreakpoints)
{
    RefPtr<InspectorObject> breakpoints = InspectorObject::create();
    for (SourceBreakpoints::const_iterator it = sourceBreakpoints.begin(); it != sourceBreakpoints.end(); ++it) {
        RefPtr<InspectorObject> breakpoint = InspectorObject::create();
        breakpoint->setBoolean("enabled", it->second.enabled);
        breakpoint->setString("condition", it->second.condition);
        breakpoints->setObject(String::number(it->first), breakpoint);
    }
    return breakpoints.release();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
