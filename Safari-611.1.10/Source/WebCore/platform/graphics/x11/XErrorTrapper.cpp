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

#include "config.h"
#include "XErrorTrapper.h"

#if PLATFORM(X11)
#include <sys/types.h>
#include <unistd.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashMap<Display*, Vector<XErrorTrapper*>>& xErrorTrappersMap()
{
    static NeverDestroyed<HashMap<Display*, Vector<XErrorTrapper*>>> trappersMap;
    return trappersMap;
}

XErrorTrapper::XErrorTrapper(Display* display, Policy policy, Vector<unsigned char>&& expectedErrors)
    : m_display(display)
    , m_policy(policy)
    , m_expectedErrors(WTFMove(expectedErrors))
{
    xErrorTrappersMap().add(m_display, Vector<XErrorTrapper*>()).iterator->value.append(this);
    m_previousErrorHandler = XSetErrorHandler([](Display* display, XErrorEvent* event) -> int {
        auto iterator = xErrorTrappersMap().find(display);
        if (iterator == xErrorTrappersMap().end())
            return 0;

        ASSERT(!iterator->value.isEmpty());
        iterator->value.last()->errorEvent(event);
        return 0;
    });
}

XErrorTrapper::~XErrorTrapper()
{
    XSync(m_display, False);
    auto iterator = xErrorTrappersMap().find(m_display);
    ASSERT(iterator != xErrorTrappersMap().end());
    auto* trapper = iterator->value.takeLast();
    ASSERT_UNUSED(trapper, trapper == this);
    if (iterator->value.isEmpty())
        xErrorTrappersMap().remove(iterator);

    XSetErrorHandler(m_previousErrorHandler);
}

unsigned char XErrorTrapper::errorCode() const
{
    XSync(m_display, False);
    return m_errorCode;
}

void XErrorTrapper::errorEvent(XErrorEvent* event)
{
    m_errorCode = event->error_code;
    if (m_policy == Policy::Ignore)
        return;

    if (m_expectedErrors.contains(m_errorCode))
        return;

    static const char errorFormatString[] = "The program with pid %d received an X Window System error.\n"
        "The error was '%s'.\n"
        "  (Details: serial %ld error_code %d request_code %d minor_code %d)\n";
    char errorMessage[64];
    XGetErrorText(m_display, m_errorCode, errorMessage, 63);
    WTFLogAlways(errorFormatString, getpid(), errorMessage, event->serial, event->error_code, event->request_code, event->minor_code);

    if (m_policy == Policy::Crash)
        CRASH();
}

} // namespace WebCore

#endif // PLATFORM(X11)
