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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "CapabilityRange.h"

#include "JSDOMBinding.h"
#include "MediaSourceStates.h"
#include <bindings/ScriptValue.h>
#include <interpreter/CallFrame.h>
#include <runtime/JSCJSValue.h>

using namespace JSC;

namespace WebCore {

RefPtr<CapabilityRange> CapabilityRange::create(const MediaStreamSourceCapabilityRange& rangeInfo)
{
    return adoptRef(new CapabilityRange(rangeInfo));
}

CapabilityRange::CapabilityRange(const MediaStreamSourceCapabilityRange& rangeInfo)
    : m_rangeInfo(rangeInfo)
{
}

static Deprecated::ScriptValue scriptValue(ExecState* exec, const MediaStreamSourceCapabilityRange::ValueUnion& value, MediaStreamSourceCapabilityRange::Type type)
{
    // NOTE: the spec says:
    //      ... an implementation should make a reasonable attempt to translate and scale the hardware's setting
    //      onto the mapping provided by this specification. If this is not possible due to the user agent's
    //      inability to retrieve a given capability from a source, then for CapabilityRange-typed capabilities,
    //      the min and max fields will not be present on the returned dictionary, and the supported field will be false.
    // We don't do this currently because I don't know of an instance where it isn't possible to retrieve a source
    // capability, but when we have to deal with this we will need to mark CapabilityRange.min and CapabilityRange.max as
    // "Custom" and return jsUndefined() from the custom getter to support it.
    
    switch (type) {
    case MediaStreamSourceCapabilityRange::Float:
        return Deprecated::ScriptValue(exec->vm(), JSValue(value.asFloat));
        break;
    case MediaStreamSourceCapabilityRange::ULong:
        return Deprecated::ScriptValue(exec->vm(), JSValue(value.asULong));
        break;
    case MediaStreamSourceCapabilityRange::Undefined:
        return Deprecated::ScriptValue(exec->vm(), jsUndefined());
        break;
    }

    ASSERT_NOT_REACHED();
    return Deprecated::ScriptValue(exec->vm(), jsUndefined());
}

Deprecated::ScriptValue CapabilityRange::min(ExecState* exec) const
{
    return scriptValue(exec, m_rangeInfo.min(), m_rangeInfo.type());
}

Deprecated::ScriptValue CapabilityRange::max(ExecState* exec) const
{
    return scriptValue(exec, m_rangeInfo.max(), m_rangeInfo.type());
}

} // namespace WebCore

#endif
