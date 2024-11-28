/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(SKIA)

#include "ViewSnapshotStore.h"
#include <WebCore/NotImplemented.h>
#include <skia/core/SkData.h>
IGNORE_CLANG_WARNINGS_BEGIN("cast-align")
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Skia port
#include <skia/encode/SkPngEncoder.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
IGNORE_CLANG_WARNINGS_END
#include <span>
#include <wtf/text/Base64.h>

namespace WebKit {
using namespace WebCore;

static std::optional<String> base64EncodedPNGData(SkImage& image)
{
    auto data = SkPngEncoder::Encode(nullptr, &image, { });
    if (!data)
        return std::nullopt;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Skia port
    return base64EncodeToString(std::span<const uint8_t>(data->bytes(), data->size()));
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(ShareableBitmap::Handle&& handle)
{
    auto bitmap = ShareableBitmap::create(WTFMove(handle), SharedMemory::Protection::ReadOnly);
    if (!bitmap)
        return std::nullopt;

    auto image = bitmap->createPlatformImage();
    return base64EncodedPNGData(*image.get());
}

#if !PLATFORM(GTK)
std::optional<String> WebAutomationSession::platformGetBase64EncodedPNGData(const ViewSnapshot&)
{
    notImplemented();
    return std::nullopt;
}
#endif

} // namespace WebKit

#endif // USE(SKIA)
