/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef MediaPlayerPrivateFullscreenWindow_h
#define MediaPlayerPrivateFullscreenWindow_h

#include <wtf/RefPtr.h>
#include <wtf/WindowsExtras.h>

#if USE(CA)
#include "CACFLayerTreeHostClient.h"
#endif

namespace WebCore {

#if USE(CA)
class CACFLayerTreeHost;
class PlatformCALayer;
#endif

class MediaPlayerPrivateFullscreenClient {
public:
    virtual LRESULT fullscreenClientWndProc(HWND, UINT message, WPARAM, LPARAM) = 0;
protected:
    virtual ~MediaPlayerPrivateFullscreenClient() = default;
};

class MediaPlayerPrivateFullscreenWindow {
    WTF_MAKE_FAST_ALLOCATED;
public:
    MediaPlayerPrivateFullscreenWindow(MediaPlayerPrivateFullscreenClient*);
    ~MediaPlayerPrivateFullscreenWindow();

    void createWindow(HWND ownerWindow);
    
    HWND hwnd() const { return m_hwnd; }

#if USE(CA)
    PlatformCALayer* rootChildLayer() const { return m_rootChild.get(); }
    void setRootChildLayer(Ref<PlatformCALayer>&&);
#endif

private:
    static LRESULT __stdcall staticWndProc(HWND, UINT message, WPARAM, LPARAM);
    LRESULT wndProc(HWND, UINT message, WPARAM, LPARAM);

    MediaPlayerPrivateFullscreenClient* m_client;
#if USE(CA)
    RefPtr<CACFLayerTreeHost> m_layerTreeHost;
    RefPtr<PlatformCALayer> m_rootChild;
#endif
    HWND m_hwnd;
};

}

#endif
