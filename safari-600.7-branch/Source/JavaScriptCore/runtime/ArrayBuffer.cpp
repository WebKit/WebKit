/*
 * Copyright (C) 2009, 2013 Apple Inc. All rights reserved.
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
#include "ArrayBuffer.h"

#include "ArrayBufferNeuteringWatchpoint.h"
#include "JSArrayBufferView.h"
#include "JSCInlines.h"
#include <wtf/RefPtr.h>

namespace JSC {

bool ArrayBuffer::transfer(ArrayBufferContents& result)
{
    Ref<ArrayBuffer> protect(*this);

    if (!m_contents.m_data) {
        result.m_data = 0;
        return false;
    }

    bool isNeuterable = !m_pinCount;

    if (isNeuterable)
        m_contents.transfer(result);
    else {
        m_contents.copyTo(result);
        if (!result.m_data)
            return false;
    }

    for (size_t i = numberOfIncomingReferences(); i--;) {
        JSCell* cell = incomingReferenceAt(i);
        if (JSArrayBufferView* view = jsDynamicCast<JSArrayBufferView*>(cell))
            view->neuter();
        else if (ArrayBufferNeuteringWatchpoint* watchpoint = jsDynamicCast<ArrayBufferNeuteringWatchpoint*>(cell))
            watchpoint->set()->fireAll();
    }
    return true;
}

} // namespace JSC

