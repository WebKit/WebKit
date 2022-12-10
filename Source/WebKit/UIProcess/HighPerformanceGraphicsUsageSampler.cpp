/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "HighPerformanceGraphicsUsageSampler.h"

#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <WebCore/DiagnosticLoggingKeys.h>

namespace WebKit {
using namespace WebCore;

static const Seconds samplingInterval { 10_min };

HighPerformanceGraphicsUsageSampler::HighPerformanceGraphicsUsageSampler(WebProcessPool& webProcessPool)
    : m_webProcessPool(webProcessPool)
    , m_timer(RunLoop::main(), this, &HighPerformanceGraphicsUsageSampler::timerFired)
{
    m_timer.startRepeating(samplingInterval);
}

void HighPerformanceGraphicsUsageSampler::timerFired()
{
    bool isUsingHighPerformanceWebGL = false;
    bool isUsingHighPerformanceWebGLInVisibleView = false;

    RefPtr<WebPageProxy> firstPage;
    for (auto& webProcess : m_webProcessPool.processes()) {
        for (auto& page : webProcess->pages()) {
            if (!page)
                continue;
            if (!firstPage)
                firstPage = page;

            if (page->isUsingHighPerformanceWebGL()) {
                isUsingHighPerformanceWebGL = true;
                if (page->isViewVisible()) {
                    isUsingHighPerformanceWebGLInVisibleView = true;
                    break;
                }
            }
        }
    }

    if (!firstPage)
        return;

    String state = DiagnosticLoggingKeys::inactiveKey();
    if (isUsingHighPerformanceWebGLInVisibleView)
        state = DiagnosticLoggingKeys::activeInForegroundTabKey();
    else if (isUsingHighPerformanceWebGL)
        state = DiagnosticLoggingKeys::activeInBackgroundTabOnlyKey();

    firstPage->logDiagnosticMessage(DiagnosticLoggingKeys::webGLStateKey(), state, ShouldSample::No);
}

} // namespace WebKit
