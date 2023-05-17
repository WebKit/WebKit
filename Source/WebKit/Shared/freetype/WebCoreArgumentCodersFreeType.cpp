/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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
#include "WebCoreArgumentCoders.h"

#if USE(FREETYPE)

#include <WebCore/Font.h>

namespace IPC {

void ArgumentCoder<WebCore::Font>::encodePlatformData(Encoder&, const WebCore::Font&)
{
    ASSERT_NOT_REACHED();
}

std::optional<WebCore::FontPlatformData> ArgumentCoder<WebCore::Font>::decodePlatformData(Decoder&)
{
    ASSERT_NOT_REACHED();
    return std::nullopt;
}

void ArgumentCoder<WebCore::FontPlatformData::Attributes>::encodePlatformData(Encoder&, const WebCore::FontPlatformData::Attributes&)
{
    ASSERT_NOT_REACHED();
}

bool ArgumentCoder<WebCore::FontPlatformData::Attributes>::decodePlatformData(Decoder&, WebCore::FontPlatformData::Attributes&)
{
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace IPC

#endif // USE(FREETYPE)
