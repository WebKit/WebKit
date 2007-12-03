/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSGlobalObject.h"

#include "object_object.h"
#include "function_object.h"
#include "array_object.h"
#include "bool_object.h"
#include "string_object.h"
#include "number_object.h"
#include "date_object.h"
#include "regexp_object.h"
#include "error_object.h"

namespace KJS {

static inline void markIfNeeded(JSValue* v)
{
    if (v && !v->marked())
        v->mark();
}

void JSGlobalObject::mark()
{
    JSObject::mark();

    if (m_interpreter->m_currentExec)
        m_interpreter->m_currentExec->mark();

    markIfNeeded(m_interpreter->m_globalExec.exception());

    markIfNeeded(m_interpreter->m_Object);
    markIfNeeded(m_interpreter->m_Function);
    markIfNeeded(m_interpreter->m_Array);
    markIfNeeded(m_interpreter->m_Boolean);
    markIfNeeded(m_interpreter->m_String);
    markIfNeeded(m_interpreter->m_Number);
    markIfNeeded(m_interpreter->m_Date);
    markIfNeeded(m_interpreter->m_RegExp);
    markIfNeeded(m_interpreter->m_Error);
    
    markIfNeeded(m_interpreter->m_ObjectPrototype);
    markIfNeeded(m_interpreter->m_FunctionPrototype);
    markIfNeeded(m_interpreter->m_ArrayPrototype);
    markIfNeeded(m_interpreter->m_BooleanPrototype);
    markIfNeeded(m_interpreter->m_StringPrototype);
    markIfNeeded(m_interpreter->m_NumberPrototype);
    markIfNeeded(m_interpreter->m_DatePrototype);
    markIfNeeded(m_interpreter->m_RegExpPrototype);
    markIfNeeded(m_interpreter->m_ErrorPrototype);
    
    markIfNeeded(m_interpreter->m_EvalError);
    markIfNeeded(m_interpreter->m_RangeError);
    markIfNeeded(m_interpreter->m_ReferenceError);
    markIfNeeded(m_interpreter->m_SyntaxError);
    markIfNeeded(m_interpreter->m_TypeError);
    markIfNeeded(m_interpreter->m_UriError);
    
    markIfNeeded(m_interpreter->m_EvalErrorPrototype);
    markIfNeeded(m_interpreter->m_RangeErrorPrototype);
    markIfNeeded(m_interpreter->m_ReferenceErrorPrototype);
    markIfNeeded(m_interpreter->m_SyntaxErrorPrototype);
    markIfNeeded(m_interpreter->m_TypeErrorPrototype);
    markIfNeeded(m_interpreter->m_UriErrorPrototype);
}

ExecState* JSGlobalObject::globalExec()
{
    return &m_interpreter->m_globalExec;
}

} // namespace KJS
