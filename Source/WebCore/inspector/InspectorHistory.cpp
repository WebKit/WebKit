/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "InspectorHistory.h"

#if ENABLE(INSPECTOR)

#include "Node.h"

namespace WebCore {

namespace {

class UndoableStateMark : public InspectorHistory::Action {
public:
    UndoableStateMark() : InspectorHistory::Action("[UndoableState]") { }

    virtual bool perform(ErrorString*) { return true; }

    virtual bool undo(ErrorString*) { return true; }

    virtual bool isUndoableStateMark() { return true; }
};

}

InspectorHistory::Action::Action(const String& name) : m_name(name)
{
}

InspectorHistory::Action::~Action()
{
}

String InspectorHistory::Action::toString()
{
    return m_name;
}

bool InspectorHistory::Action::isUndoableStateMark()
{
    return false;
}

String InspectorHistory::Action::mergeId()
{
    return "";
}

void InspectorHistory::Action::merge(PassOwnPtr<Action>)
{
}

InspectorHistory::InspectorHistory() { }

InspectorHistory::~InspectorHistory() { }

bool InspectorHistory::perform(PassOwnPtr<Action> action, ErrorString* errorString)
{
    if (!action->perform(errorString))
        return false;

    if (!m_history.isEmpty() && !action->mergeId().isEmpty() && action->mergeId() == m_history.first()->mergeId())
        m_history.first()->merge(action);
    else
        m_history.prepend(action);

    return true;
}

void InspectorHistory::markUndoableState()
{
    m_history.prepend(adoptPtr(new UndoableStateMark()));
}

bool InspectorHistory::undo(ErrorString* errorString)
{
    while (!m_history.isEmpty() && m_history.first()->isUndoableStateMark())
        m_history.removeFirst();

    while (!m_history.isEmpty()) {
        OwnPtr<Action> first = m_history.takeFirst();
        if (!first->undo(errorString)) {
            m_history.clear();
            return false;
        }

        if (first->isUndoableStateMark())
            break;
    }

    return true;
}

void InspectorHistory::reset()
{
    m_history.clear();
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
