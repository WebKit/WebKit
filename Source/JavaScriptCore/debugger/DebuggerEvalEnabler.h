/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#pragma once

#include "CallFrame.h"
#include "JSGlobalObject.h"

namespace JSC {

class DebuggerEvalEnabler {
public:
    enum class Mode {
        EvalOnCurrentCallFrame,
        EvalOnGlobalObjectAtDebuggerEntry,
    };

    DebuggerEvalEnabler(JSGlobalObject* globalObject, Mode mode = Mode::EvalOnCurrentCallFrame)
        : m_globalObject(globalObject)
#if ASSERT_ENABLED
        , m_mode(mode)
#endif
        
    {
        UNUSED_PARAM(mode);
        if (globalObject) {
            m_evalWasDisabled = !globalObject->evalEnabled();
            if (m_evalWasDisabled)
                globalObject->setEvalEnabled(true, globalObject->evalDisabledErrorMessage());
#if ASSERT_ENABLED
            if (m_mode == Mode::EvalOnGlobalObjectAtDebuggerEntry)
                globalObject->setGlobalObjectAtDebuggerEntry(globalObject);
#endif
        }
    }

    ~DebuggerEvalEnabler()
    {
        if (m_globalObject) {
            JSGlobalObject* globalObject = m_globalObject;
            if (m_evalWasDisabled)
                globalObject->setEvalEnabled(false, globalObject->evalDisabledErrorMessage());
#if ASSERT_ENABLED
            if (m_mode == Mode::EvalOnGlobalObjectAtDebuggerEntry)
                globalObject->setGlobalObjectAtDebuggerEntry(nullptr);
#endif
        }
    }

private:
    JSGlobalObject* m_globalObject;
    bool m_evalWasDisabled { false };
#if ASSERT_ENABLED
    DebuggerEvalEnabler::Mode m_mode;
#endif
};

} // namespace JSC
