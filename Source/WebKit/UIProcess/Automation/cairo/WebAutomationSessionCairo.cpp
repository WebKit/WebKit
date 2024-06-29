/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "WebAutomationSession.h"

#if USE(CAIRO)

#include "ViewSnapshotStore.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/RefPtrCairo.h>
#include <cairo.h>
#include <wtf/text/Base64.h>

namespace WebKit {
using namespace WebCore;

static std::optional<String> base64EncodedPNGData(cairo_surface_t* surface)
{
    if (!surface)
        return std::nullopt;

    Vector<uint8_t> pngData;
    cairo_surface_write_to_png_stream(surface, [](void* userData, const unsigned char* data, unsigned length) -> cairo_status_t {
        auto* pngData = static_cast<Vector<uint8_t>*>(userData);
        pngData->append(std::span { reinterpret_cast<const uint8_t*>(data), length });
        return CAIRO_STATUS_SUCCESS;
    }, &pngData);

    if (pngData.isEmpty())
        return std::nullopt;

    return base64EncodeToString(pngData);
}

std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(ShareableBitmap::Handle&& handle)
{
    auto bitmap = ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly);
    if (!bitmap)
        return std::nullopt;

    auto surface = bitmap->createCairoSurface();
    return base64EncodedPNGData(surface.get());
}

#if !PLATFORM(GTK)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ViewSnapshot&)
{
    notImplemented();
    return std::nullopt;
}
#endif

} // namespace WebKit

#endif // USE(CAIRO)
