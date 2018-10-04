/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "config.h"
#include "RealtimeMediaSourceFactory.h"

#include "CaptureDevice.h"
#include "RealtimeMediaSource.h"

#if ENABLE(MEDIA_STREAM)

#include "CaptureDeviceManager.h"
#include "Logging.h"
#include "MediaStreamPrivate.h"
#include <wtf/SHA1.h>

namespace WebCore {

void SingleSourceFactory::setActiveSource(RealtimeMediaSource& source)
{
    if (m_activeSource == &source)
        return;
    if (m_activeSource && m_activeSource->isProducingData())
        m_activeSource->setMuted(true);
    m_activeSource = &source;
}

void SingleSourceFactory::unsetActiveSource(RealtimeMediaSource& source)
{
    if (m_activeSource == &source)
        m_activeSource = nullptr;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
