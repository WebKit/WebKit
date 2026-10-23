/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/FontGenericFamilies.h>
#include <WebCore/TrustedFonts.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class Settings;

class ImageEnvironmentSettings final : public RefCounted<ImageEnvironmentSettings> {
    WTF_MAKE_TZONE_ALLOCATED(ImageEnvironmentSettings);
public:
    static Ref<ImageEnvironmentSettings> create(const Settings&);

    bool layerBasedSVGEngineEnabled() const { return m_layerBasedSVGEngineEnabled; }
    bool cssDPropertyEnabled() const { return m_cssDPropertyEnabled; }
    DownloadableBinaryFontTrustedTypes downloadableBinaryFontTrustedTypes() const { return m_downloadableBinaryFontTrustedTypes; }
    const FontGenericFamilies& fontGenericFamilies() const LIFETIME_BOUND { return m_fontGenericFamilies; }

    bool operator==(const ImageEnvironmentSettings&) const;

private:
    explicit ImageEnvironmentSettings(const Settings&);

    bool m_layerBasedSVGEngineEnabled { false };
    bool m_cssDPropertyEnabled { false };
    DownloadableBinaryFontTrustedTypes m_downloadableBinaryFontTrustedTypes { DownloadableBinaryFontTrustedTypes::Any };
    FontGenericFamilies m_fontGenericFamilies;
};

} // namespace WebCore
