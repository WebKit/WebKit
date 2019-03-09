/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DeviceOrientationAndMotionAccessController.h"

#if ENABLE(DEVICE_ORIENTATION)

#include "Chrome.h"
#include "ChromeClient.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"

namespace WebCore {

DeviceOrientationAndMotionAccessController::DeviceOrientationAndMotionAccessController(Document& document)
    : m_document(document)
{
    ASSERT(&m_document.topDocument() == &m_document);
}

void DeviceOrientationAndMotionAccessController::shouldAllowAccess(Function<void(bool granted)>&& callback)
{
    if (m_accessState)
        return callback(*m_accessState);

    auto* page = m_document.page();
    if (!page)
        return callback(false);

    m_pendingRequests.append(WTFMove(callback));
    if (m_pendingRequests.size() > 1)
        return;

    page->chrome().client().shouldAllowDeviceOrientationAndMotionAccess(m_document.securityOrigin(), [this, weakThis = makeWeakPtr(*this)](bool granted) mutable {
        if (weakThis)
            setAccessState(granted);
    });
}

void DeviceOrientationAndMotionAccessController::setAccessState(bool granted)
{
    ASSERT(!m_accessState);

    m_accessState = granted;

    auto pendingRequests = WTFMove(m_pendingRequests);
    for (auto& request : pendingRequests)
        request(granted);

    if (!granted)
        return;

    for (auto* frame = m_document.frame(); frame && frame->window(); frame = frame->tree().traverseNext(m_document.frame())) {
        frame->window()->startListeningForDeviceOrientationIfNecessary();
        frame->window()->startListeningForDeviceMotionIfNecessary();
    }
}

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
