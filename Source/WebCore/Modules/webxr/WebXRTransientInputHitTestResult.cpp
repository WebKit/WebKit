/*
 * Copyright (C) 2025 Igalia S.L. All rights reserved.
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
#include "WebXRTransientInputHitTestResult.h"

#include <wtf/TZoneMallocInlines.h>

#if ENABLE(WEBXR_HIT_TEST)
#include "WebXRHitTestResult.h"
#include "WebXRInputSource.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebXRTransientInputHitTestResult);

Ref<WebXRTransientInputHitTestResult> WebXRTransientInputHitTestResult::create(Ref<WebXRInputSource>&& inputSource, Vector<Ref<WebXRHitTestResult>>&& results)
{
    return adoptRef(*new WebXRTransientInputHitTestResult(WTF::move(inputSource), WTF::move(results)));
}

WebXRTransientInputHitTestResult::WebXRTransientInputHitTestResult(Ref<WebXRInputSource>&& inputSource, Vector<Ref<WebXRHitTestResult>>&& results)
    : m_inputSource(WTF::move(inputSource))
    , m_results(WTF::move(results))
{
}

WebXRTransientInputHitTestResult::~WebXRTransientInputHitTestResult() = default;

const WebXRInputSource& WebXRTransientInputHitTestResult::inputSource() const
{
    return m_inputSource;
}

const Vector<Ref<WebXRHitTestResult>>& WebXRTransientInputHitTestResult::results() const
{
    return m_results;
}

} // namespace WebCore

#endif // ENABLE(WEBXR_HIT_TEST)
