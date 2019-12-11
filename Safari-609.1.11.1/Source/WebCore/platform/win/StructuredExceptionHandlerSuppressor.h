/*
 * Copyright (C) 2011, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef StructuredExceptionHandlerSuppressor_h
#define StructuredExceptionHandlerSuppressor_h

#include <excpt.h>
#include <wtf/Noncopyable.h>

extern "C" EXCEPTION_DISPOSITION __stdcall exceptionHandler(struct _EXCEPTION_RECORD* exceptionRecord, void* establisherFrame, struct _CONTEXT* contextRecord, void* dispatcherContext);

namespace WebCore {

struct ExceptionRegistration {
    ExceptionRegistration* prev;
    void* handler;
};

class StructuredExceptionHandlerSuppressor {
    WTF_MAKE_NONCOPYABLE(StructuredExceptionHandlerSuppressor);
public:
    StructuredExceptionHandlerSuppressor(ExceptionRegistration&);
    ~StructuredExceptionHandlerSuppressor();

private:
#if defined(_M_IX86)
    void* m_savedExceptionRegistration;
#endif
};

} // namespace WebCore

#endif // StructuredExceptionHandlerSuppressor_h
