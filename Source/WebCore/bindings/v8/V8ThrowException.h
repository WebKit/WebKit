/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8ThrowException_h
#define V8ThrowException_h

#include <v8.h>

namespace WebCore {

enum ErrorType {
    RangeError,
    ReferenceError,
    SyntaxError,
    TypeError,
    GeneralError
};

class V8ThrowException {
public:
    static v8::Handle<v8::Value> setDOMException(int, v8::Isolate*);

    static v8::Handle<v8::Value> throwError(ErrorType, const char*, v8::Isolate* = 0);
    static v8::Handle<v8::Value> throwError(v8::Local<v8::Value>, v8::Isolate* = 0);

    static v8::Handle<v8::Value> throwTypeError(const char* = 0, v8::Isolate* = 0);
    static v8::Handle<v8::Value> throwNotEnoughArgumentsError(v8::Isolate*);
};

} // namespace WebCore

#endif // V8ThrowException_h
