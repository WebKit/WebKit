/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/Color.h>
#include <WebCore/SharedBuffer.h>
#include <optional>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class Model final : public RefCounted<Model> {
public:
    WEBCORE_EXPORT static Ref<Model> create(Ref<SharedBuffer>&&, String&&, URL, std::optional<Color>&& surfaceColor = std::nullopt, bool isConverted = false);
    WEBCORE_EXPORT ~Model();

    // FIXME: data() should return SharedBuffer& and we should have a separate dataForSerialization() that returns a Ref.
    Ref<SharedBuffer> data() const { return m_data; }
    const String& mimeType() const LIFETIME_BOUND { return m_mimeType; }
    const URL& url() const LIFETIME_BOUND { return m_url; }
    const std::optional<Color>& surfaceColor() const LIFETIME_BOUND { return m_surfaceColor; }
    bool isConverted() const { return m_isConverted; }
    WEBCORE_EXPORT String filename() const;

private:
    Model(Ref<SharedBuffer>&&, String&&, URL, std::optional<Color>&& surfaceColor, bool isConverted);

    const Ref<SharedBuffer> m_data;
    String m_mimeType;
    URL m_url;
    std::optional<Color> m_surfaceColor;
    bool m_isConverted { false };
};

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const Model&);

} // namespace WebCore
