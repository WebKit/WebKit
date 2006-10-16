/*
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
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

#include "config.h"
#include "BrowserExtensionWin.h"

#include "FrameLoadRequest.h"
#include "FrameWin.h"
#include <windows.h>

namespace WebCore {

#define notImplemented() do { \
    char buf[256] = {0}; \
    _snprintf(buf, sizeof(buf), "FIXME: UNIMPLEMENTED: %s:%d\n", __FILE__, __LINE__); \
    OutputDebugStringA(buf); \
} while (0)


BrowserExtensionWin::BrowserExtensionWin(WebCore::FrameWin* frame) : m_frame(frame)
{

}

void BrowserExtensionWin::setTypedIconURL(KURL const&, const String&)
{
}

void BrowserExtensionWin::setIconURL(KURL const&)
{

}

int BrowserExtensionWin::getHistoryLength()
{
    return 0;
}

bool BrowserExtensionWin::canRunModal()
{
    notImplemented();
    return 0;
}

void BrowserExtensionWin::createNewWindow(const FrameLoadRequest& request)
{
    m_frame->createNewWindow(request);
}

void BrowserExtensionWin::createNewWindow(const FrameLoadRequest& request,
                                          const WindowArgs& args,
                                          Frame*& frame)
{
    m_frame->createNewWindow(request, args, frame);
}

bool BrowserExtensionWin::canRunModalNow()
{
    notImplemented();
    return 0;
}

void BrowserExtensionWin::runModal()
{
    notImplemented();
}

void BrowserExtensionWin::goBackOrForward(int)
{
    notImplemented();
}

KURL BrowserExtensionWin::historyURL(int distance)
{
    notImplemented();
    return KURL();
}

} // namespace WebCore
