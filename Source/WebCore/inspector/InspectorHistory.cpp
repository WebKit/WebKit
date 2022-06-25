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

#include "Node.h"

namespace WebCore {

class UndoableStateMark : public InspectorHistory::Action {
private:
    ExceptionOr<void> perform() final { return { }; }
    ExceptionOr<void> undo() final { return { }; }
    ExceptionOr<void> redo() final { return { }; }
    bool isUndoableStateMark() final { return true; }
};

ExceptionOr<void> InspectorHistory::perform(std::unique_ptr<Action> action)
{
    auto performResult = action->perform();
    if (performResult.hasException())
        return performResult.releaseException();

    if (!action->mergeId().isEmpty() && m_afterLastActionIndex > 0 && action->mergeId() == m_history[m_afterLastActionIndex - 1]->mergeId())
        m_history[m_afterLastActionIndex - 1]->merge(WTFMove(action));
    else {
        m_history.resize(m_afterLastActionIndex);
        m_history.append(WTFMove(action));
        ++m_afterLastActionIndex;
    }
    return { };
}

void InspectorHistory::markUndoableState()
{
    perform(makeUnique<UndoableStateMark>());
}

ExceptionOr<void> InspectorHistory::undo()
{
    while (m_afterLastActionIndex > 0 && m_history[m_afterLastActionIndex - 1]->isUndoableStateMark())
        --m_afterLastActionIndex;

    while (m_afterLastActionIndex > 0) {
        Action* action = m_history[m_afterLastActionIndex - 1].get();
        auto undoResult = action->undo();
        if (undoResult.hasException()) {
            reset();
            return undoResult.releaseException();
        }
        --m_afterLastActionIndex;
        if (action->isUndoableStateMark())
            break;
    }

    return { };
}

ExceptionOr<void> InspectorHistory::redo()
{
    while (m_afterLastActionIndex < m_history.size() && m_history[m_afterLastActionIndex]->isUndoableStateMark())
        ++m_afterLastActionIndex;

    while (m_afterLastActionIndex < m_history.size()) {
        Action* action = m_history[m_afterLastActionIndex].get();
        auto redoResult = action->redo();
        if (redoResult.hasException()) {
            reset();
            return redoResult.releaseException();
        }
        ++m_afterLastActionIndex;
        if (action->isUndoableStateMark())
            break;
    }
    return { };
}

void InspectorHistory::reset()
{
    m_afterLastActionIndex = 0;
    m_history.clear();
}

} // namespace WebCore
