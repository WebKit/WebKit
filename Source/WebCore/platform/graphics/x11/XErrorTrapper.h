/*
 * Copyright (C) 2016 Igalia S.L
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

#pragma once

#if PLATFORM(X11)
#include <X11/Xlib.h>
#include <wtf/Vector.h>

namespace WebCore {

class XErrorTrapper {
    WTF_MAKE_NONCOPYABLE(XErrorTrapper);
public:
    enum class Policy { Ignore, Warn, Crash };
    XErrorTrapper(::Display*, Policy = Policy::Ignore, Vector<unsigned char>&& expectedErrors = { });
    ~XErrorTrapper();

    unsigned char errorCode() const;

private:
    void errorEvent(XErrorEvent*);

    ::Display* m_display { nullptr };
    Policy m_policy { Policy::Ignore };
    Vector<unsigned char> m_expectedErrors;
    XErrorHandler m_previousErrorHandler { nullptr };
    unsigned char m_errorCode { 0 };
};

} // namespace WebCore

#endif // PLATFORM(X11)
