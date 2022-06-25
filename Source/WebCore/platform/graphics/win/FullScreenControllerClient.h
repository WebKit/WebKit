/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef FullScreenControllerClient_h
#define FullScreenControllerClient_h

#if ENABLE(FULLSCREEN_API)

namespace WebCore {

class FullScreenControllerClient {
public:
    virtual HWND fullScreenClientWindow() const = 0;
    virtual HWND fullScreenClientParentWindow() const = 0;
    virtual void fullScreenClientSetParentWindow(HWND) = 0;
    virtual void fullScreenClientWillEnterFullScreen() = 0;
    virtual void fullScreenClientDidEnterFullScreen() = 0;
    virtual void fullScreenClientWillExitFullScreen() = 0;
    virtual void fullScreenClientDidExitFullScreen() = 0;
    virtual void fullScreenClientForceRepaint() = 0;
    virtual void fullScreenClientSaveScrollPosition() = 0;
    virtual void fullScreenClientRestoreScrollPosition() = 0;
protected:
    virtual ~FullScreenControllerClient() = default;
};

}

#endif

#endif
