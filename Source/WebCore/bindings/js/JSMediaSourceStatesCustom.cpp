/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "JSMediaSourceStates.h"

#include "JSDOMBinding.h"

using namespace JSC;

namespace WebCore {

JSValue JSMediaSourceStates::width(ExecState&) const
{
    if (!wrapped().hasVideoSource())
        return jsUndefined();

    return jsNumber(wrapped().width());
}

JSValue JSMediaSourceStates::height(ExecState&) const
{
    if (!wrapped().hasVideoSource())
        return jsUndefined();
    
    return jsNumber(wrapped().height());
}

JSValue JSMediaSourceStates::frameRate(ExecState&) const
{
    if (!wrapped().hasVideoSource())
        return jsUndefined();
    
    return jsNumber(wrapped().frameRate());
}

JSValue JSMediaSourceStates::aspectRatio(ExecState&) const
{
    if (!wrapped().hasVideoSource())
        return jsUndefined();
    
    return jsNumber(wrapped().aspectRatio());
}

JSValue JSMediaSourceStates::facingMode(ExecState& state) const
{
    if (!wrapped().hasVideoSource())
        return jsUndefined();

    const AtomicString& mode = wrapped().facingMode();
    if (mode.isEmpty())
        return jsUndefined();
    
    return jsStringWithCache(&state, wrapped().facingMode());
}

JSValue JSMediaSourceStates::volume(ExecState&) const
{
    if (wrapped().hasVideoSource())
        return jsUndefined();
    
    return jsNumber(wrapped().volume());
}

} // namespace WebCore

#endif
