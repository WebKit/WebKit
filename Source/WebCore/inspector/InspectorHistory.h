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

#pragma once

#include "ExceptionOr.h"
#include <wtf/Vector.h>

namespace WebCore {

class InspectorHistory final {
    WTF_MAKE_NONCOPYABLE(InspectorHistory);
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Action {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        virtual ~Action() = default;

        virtual String mergeId() { return emptyString(); }
        virtual void merge(std::unique_ptr<Action>) { };

        virtual ExceptionOr<void> perform() = 0;

        virtual ExceptionOr<void> undo() = 0;
        virtual ExceptionOr<void> redo() = 0;

        virtual bool isUndoableStateMark() { return false; }

    private:
        String m_name;
    };

    InspectorHistory() = default;

    ExceptionOr<void> perform(std::unique_ptr<Action>);
    void markUndoableState();

    ExceptionOr<void> undo();
    ExceptionOr<void> redo();
    void reset();

private:
    Vector<std::unique_ptr<Action>> m_history;
    size_t m_afterLastActionIndex { 0 };
};

} // namespace WebCore
