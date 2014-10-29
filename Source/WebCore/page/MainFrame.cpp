/*

Copyright (C) 2013 Apple Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include "config.h"
#include "MainFrame.h"

#include "PageOverlayController.h"

#if PLATFORM(MAC)
#include "ServicesOverlayController.h"
#endif

namespace WebCore {

inline MainFrame::MainFrame(Page& page, FrameLoaderClient& client)
    : Frame(page, nullptr, client)
    , m_selfOnlyRefCount(0)
#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    , m_servicesOverlayController(std::make_unique<ServicesOverlayController>(*this))
#endif
    , m_pageOverlayController(std::make_unique<PageOverlayController>(*this))
{
}

RefPtr<MainFrame> MainFrame::create(Page& page, FrameLoaderClient& client)
{
    return adoptRef(new MainFrame(page, client));
}

void MainFrame::selfOnlyRef()
{
    if (m_selfOnlyRefCount++)
        return;

    ref();
}

void MainFrame::selfOnlyDeref()
{
    ASSERT(m_selfOnlyRefCount);
    if (--m_selfOnlyRefCount)
        return;

    if (hasOneRef())
        dropChildren();

    deref();
}

void MainFrame::dropChildren()
{
    while (Frame* child = tree().firstChild())
        tree().removeChild(child);
}

}
