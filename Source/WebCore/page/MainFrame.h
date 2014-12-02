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

#ifndef MainFrame_h
#define MainFrame_h

#include "Frame.h"

namespace WebCore {

class DiagnosticLoggingClient;
class PageConfiguration;
class PageOverlayController;
class ServicesOverlayController;

class MainFrame final : public Frame {
public:
    static RefPtr<MainFrame> create(Page&, PageConfiguration&);

    void selfOnlyRef();
    void selfOnlyDeref();

    PageOverlayController& pageOverlayController() { return *m_pageOverlayController; }

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    ServicesOverlayController& servicesOverlayController() { return *m_servicesOverlayController; }
#endif // ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)

    DiagnosticLoggingClient* diagnosticLoggingClient() const { return m_diagnosticLoggingClient; }

private:
    MainFrame(Page&, PageConfiguration&);

    void dropChildren();

    unsigned m_selfOnlyRefCount;
    
#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    std::unique_ptr<ServicesOverlayController> m_servicesOverlayController;
#endif
    std::unique_ptr<PageOverlayController> m_pageOverlayController;
    DiagnosticLoggingClient* m_diagnosticLoggingClient;
};

inline bool Frame::isMainFrame() const
{
    return this == &m_mainFrame;
}

}

#endif
