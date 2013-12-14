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

#ifndef FTLFormattedValue_h
#define FTLFormattedValue_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "FTLAbbreviations.h"
#include "FTLValueFormat.h"

namespace JSC { namespace FTL {

// This class is mostly used for OSR; it's a way of specifying how a value is formatted
// in cases where it wouldn't have been obvious from looking at other indicators (like
// the type of the LLVMValueRef or the type of the DFG::Node). Typically this arises
// because LLVMValueRef doesn't give us the granularity we need to begin with, and we
// use this in situations where there is no good way to say what node the value came
// from.

class FormattedValue {
public:
    FormattedValue()
        : m_format(InvalidValueFormat)
        , m_value(0)
    {
    }
    
    FormattedValue(ValueFormat format, LValue value)
        : m_format(format)
        , m_value(value)
    {
    }
    
    bool operator!() const
    {
        ASSERT((m_format == InvalidValueFormat) == !m_value);
        return m_format == InvalidValueFormat;
    }
    
    ValueFormat format() const { return m_format; }
    LValue value() const { return m_value; }

private:
    ValueFormat m_format;
    LValue m_value;
};

static inline FormattedValue noValue() { return FormattedValue(); }
static inline FormattedValue int32Value(LValue value) { return FormattedValue(ValueFormatInt32, value); }
static inline FormattedValue booleanValue(LValue value) { return FormattedValue(ValueFormatBoolean, value); }
static inline FormattedValue jsValueValue(LValue value) { return FormattedValue(ValueFormatJSValue, value); }
static inline FormattedValue doubleValue(LValue value) { return FormattedValue(ValueFormatDouble, value); }

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLFormattedValue_h

