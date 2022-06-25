/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NavigatorMediaDevices.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "Frame.h"
#include "MediaDevices.h"
#include "Navigator.h"

namespace WebCore {

NavigatorMediaDevices::NavigatorMediaDevices(DOMWindow* window)
    : DOMWindowProperty(window)
{
}

NavigatorMediaDevices::~NavigatorMediaDevices() = default;

NavigatorMediaDevices* NavigatorMediaDevices::from(Navigator* navigator)
{
    NavigatorMediaDevices* supplement = static_cast<NavigatorMediaDevices*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<NavigatorMediaDevices>(navigator->window());
        supplement = newSupplement.get();
        provideTo(navigator, supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

MediaDevices* NavigatorMediaDevices::mediaDevices(Navigator& navigator)
{
    return NavigatorMediaDevices::from(&navigator)->mediaDevices();
}

MediaDevices* NavigatorMediaDevices::mediaDevices() const
{
    if (!m_mediaDevices && frame())
        m_mediaDevices = MediaDevices::create(*frame()->document());
    return m_mediaDevices.get();
}

const char* NavigatorMediaDevices::supplementName()
{
    return "NavigatorMediaDevices";
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
