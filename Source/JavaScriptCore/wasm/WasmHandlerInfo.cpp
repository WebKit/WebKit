/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WasmHandlerInfo.h"

#if ENABLE(WEBASSEMBLY)

#include "WasmInstance.h"
#include "WasmTag.h"

namespace JSC {
namespace Wasm {

void HandlerInfo::initialize(const UnlinkedHandlerInfo& unlinkedInfo, CodePtr<ExceptionHandlerPtrTag> label)
{
    m_type = unlinkedInfo.m_type;
    m_start = unlinkedInfo.m_start;
    m_end = unlinkedInfo.m_end;
    m_target = unlinkedInfo.m_target;
    m_tryDepth = unlinkedInfo.m_tryDepth;
    m_nativeCode = label;

    switch (m_type) {
    case HandlerType::Catch:
        m_tag = unlinkedInfo.m_exceptionIndexOrDelegateTarget;
        break;

    case HandlerType::CatchAll:
        break;

    case HandlerType::Delegate:
        m_delegateTarget = unlinkedInfo.m_exceptionIndexOrDelegateTarget;
        break;
    }
}

const HandlerInfo* HandlerInfo::handlerForIndex(Instance& instance, const FixedVector<HandlerInfo>& exeptionHandlers, unsigned index, const Wasm::Tag* exceptionTag)
{
    bool delegating = false;
    unsigned delegateTarget = 0;
    for (auto& handler : exeptionHandlers) {
        // Handlers are ordered innermost first, so the first handler we encounter
        // that contains the source address is the correct handler to use.
        // This index used is either the BytecodeOffset or a CallSiteIndex.
        if (handler.m_start <= index && handler.m_end > index) {
            if (delegating) {
                if (handler.m_tryDepth != delegateTarget)
                    continue;
                delegating = false;
            }

            bool match = false;
            switch (handler.m_type) {
            case HandlerType::Catch:
                match = exceptionTag && instance.tag(handler.m_tag) == *exceptionTag;
                break;
            case HandlerType::CatchAll:
                match = true;
                break;
            case HandlerType::Delegate:
                delegating = true;
                delegateTarget = handler.m_delegateTarget;
                break;
            }

            if (!match)
                continue;

            return &handler;
        }
    }

    return nullptr;
}

} } // namespace JSC::Wasm

#endif // ENABLE(WEBASSEMBLY)
