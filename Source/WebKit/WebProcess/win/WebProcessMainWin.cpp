/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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

#include "config.h"
#include "WebProcessMain.h"

#include "AuxiliaryProcessMain.h"
#include "WebProcess.h"
#include <Objbase.h>
#include <wtf/win/SoftLinking.h>

SOFT_LINK_LIBRARY(user32);
SOFT_LINK_OPTIONAL(user32, SetProcessDpiAwarenessContext, BOOL, STDAPICALLTYPE, (DPI_AWARENESS_CONTEXT));

namespace WebKit {
using namespace WebCore;

class WebProcessMainWin final : public AuxiliaryProcessMainBase<WebProcess> {
public:
    bool platformInitialize() override
    {
        if (SetProcessDpiAwarenessContextPtr())
            // Playwright begin
            SetProcessDpiAwarenessContextPtr()(DPI_AWARENESS_CONTEXT_UNAWARE);
            // Playwright end
        else
            SetProcessDPIAware();
        return true;
    }
};

int WebProcessMain(int argc, char** argv)
{
    // WebProcess uses DirectX
    HRESULT hr = ::CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    RELEASE_ASSERT(SUCCEEDED(hr));
    return AuxiliaryProcessMain<WebProcessMainWin>(argc, argv);
}

} // namespace WebKit
