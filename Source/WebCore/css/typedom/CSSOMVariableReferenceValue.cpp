/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "CSSOMVariableReferenceValue.h"

#include "CSSUnparsedValue.h"
#include "ExceptionOr.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CSSOMVariableReferenceValue);

ExceptionOr<Ref<CSSOMVariableReferenceValue>> CSSOMVariableReferenceValue::create(String&& variable, RefPtr<CSSUnparsedValue>&& fallback)
{
    if (!variable.startsWith("--"_s))
        return Exception { TypeError, "Custom Variable Reference needs to have \"--\" prefix."_s };
    
    return adoptRef(*new CSSOMVariableReferenceValue(WTFMove(variable), WTFMove(fallback)));
}

ExceptionOr<void> CSSOMVariableReferenceValue::setVariable(String&& variable)
{
    if (!variable.startsWith("--"_s))
        return Exception { TypeError, "Custom Variable Reference needs to have \"--\" prefix."_s };
    
    m_variable = WTFMove(variable);
    return { };
}

String CSSOMVariableReferenceValue::toString() const
{
    StringBuilder builder;
    serialize(builder, { });
    return builder.toString();
}

void CSSOMVariableReferenceValue::serialize(StringBuilder& builder, OptionSet<SerializationArguments> arguments) const
{
    builder.append("var(");
    builder.append(m_variable);
    if (m_fallback) {
        builder.append(", ");
        m_fallback->serialize(builder, arguments);
    }
    builder.append(')');
}

} // namespace WebCore
