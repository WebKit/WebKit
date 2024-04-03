/*
 * Copyright (C) 2024 Sony Interactive Entertainment Inc.
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
#include "BackingStore.h"

#if USE(SKIA)

#include "UpdateInfo.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

static const Seconds s_scrollHysteresisDuration { 300_ms };

BackingStore::BackingStore(const WebCore::IntSize& size, float deviceScaleFactor)
    : m_size(size)
    , m_deviceScaleFactor(deviceScaleFactor)
    , m_scrolledHysteresis([this](PAL::HysteresisState state) { if (state == PAL::HysteresisState::Stopped) m_scrollSurface = nullptr; }, s_scrollHysteresisDuration)
{
}

BackingStore::~BackingStore() = default;

void BackingStore::paint(PlatformPaintContextPtr cr, const WebCore::IntRect& rect)
{
    notImplemented();
}

void BackingStore::incorporateUpdate(UpdateInfo&& updateInfo)
{
    notImplemented();
}

void BackingStore::scroll(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset)
{
    notImplemented();
}

} // namespace WebKit

#endif // USE(SKIA)
