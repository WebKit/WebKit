/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSWebGLArrayHelper_h
#define JSWebGLArrayHelper_h

#include "ExceptionCode.h"
#include "JSDOMBinding.h"
#include <interpreter/CallFrame.h>
#include <runtime/ArgList.h>
#include <runtime/Error.h>
#include <runtime/JSObject.h>
#include <runtime/JSValue.h>

namespace WebCore {

template <class T>
JSC::JSValue setWebGLArrayFromArray(JSC::ExecState* exec, T* webGLArray, JSC::ArgList const& args)
{
    if (args.at(0).isObject()) {
        // void set(in sequence<long> array, [Optional] in unsigned long offset);
        JSC::JSObject* array = JSC::asObject(args.at(0));
        unsigned offset = 0;
        if (args.size() == 2)
            offset = args.at(1).toInt32(exec);
        int length = array->get(exec, JSC::Identifier(exec, "length")).toInt32(exec);
        if (offset + length > webGLArray->length())
            setDOMException(exec, INDEX_SIZE_ERR);
        else {
            for (int i = 0; i < length; i++) {
                JSC::JSValue v = array->get(exec, i);
                if (exec->hadException())
                    return JSC::jsUndefined();
                webGLArray->set(i + offset, v.toNumber(exec));
            }
        }

        return JSC::jsUndefined();
    }

    return JSC::throwError(exec, JSC::SyntaxError);
}

}

#endif // JSWebGLArrayHelper_h
