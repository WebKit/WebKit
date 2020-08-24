/*
 *  Copyright (C) 2008-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Breakpoint.h"

#include "Debugger.h"

namespace JSC {

Breakpoint::Action::Action(Breakpoint::Action::Type type)
    : type(type)
{
}

Ref<Breakpoint> Breakpoint::create(BreakpointID id, const String& condition, ActionsVector&& actions, bool autoContinue, size_t ignoreCount)
{
    return adoptRef(*new Breakpoint(id, condition, WTFMove(actions), autoContinue, ignoreCount));
}

Breakpoint::Breakpoint(BreakpointID id, String condition, ActionsVector&& actions, bool autoContinue, size_t ignoreCount)
    : m_id(id)
    , m_condition(condition)
    , m_actions(WTFMove(actions))
    , m_autoContinue(autoContinue)
    , m_ignoreCount(ignoreCount)
{
}

bool Breakpoint::link(SourceID sourceID, unsigned lineNumber, unsigned columnNumber)
{
    ASSERT(!isLinked());
    ASSERT(!isResolved());

    m_sourceID = sourceID;
    m_lineNumber = lineNumber;
    m_columnNumber = columnNumber;
    return isLinked();
}

bool Breakpoint::resolve(unsigned lineNumber, unsigned columnNumber)
{
    ASSERT(isLinked());
    ASSERT(!isResolved());
    ASSERT(lineNumber >= m_lineNumber);
    ASSERT(columnNumber >= m_columnNumber);

    m_lineNumber = lineNumber;
    m_columnNumber = columnNumber;
    m_resolved = true;
    return isResolved();
}

bool Breakpoint::shouldPause(Debugger& debugger, JSGlobalObject* globalObject)
{
    if (++m_hitCount <= m_ignoreCount)
        return false;

    return debugger.evaluateBreakpointCondition(*this, globalObject);
}

} // namespace JSC
