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

#include "config.h"
#include "ImageEnvironmentSettings.h"

#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageEnvironmentSettings);

Ref<ImageEnvironmentSettings> ImageEnvironmentSettings::create(const Settings& settings)
{
    return adoptRef(*new ImageEnvironmentSettings(settings));
}

ImageEnvironmentSettings::ImageEnvironmentSettings(const Settings& settings)
    : m_layerBasedSVGEngineEnabled(settings.layerBasedSVGEngineEnabled())
    , m_cssDPropertyEnabled(settings.cssDPropertyEnabled())
    , m_downloadableBinaryFontTrustedTypes(settings.downloadableBinaryFontTrustedTypes())
    , m_fontGenericFamilies(settings.fontGenericFamilies())
{
}

bool ImageEnvironmentSettings::operator==(const ImageEnvironmentSettings& other) const
{
    return m_layerBasedSVGEngineEnabled == other.m_layerBasedSVGEngineEnabled
        && m_cssDPropertyEnabled == other.m_cssDPropertyEnabled
        && m_downloadableBinaryFontTrustedTypes == other.m_downloadableBinaryFontTrustedTypes
        && m_fontGenericFamilies == other.m_fontGenericFamilies;
}

} // namespace WebCore
