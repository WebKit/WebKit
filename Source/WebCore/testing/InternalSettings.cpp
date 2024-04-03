/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InternalSettings.h"

#include "CaptionUserPreferences.h"
#include "DeprecatedGlobalSettings.h"
#include "Document.h"
#include "FontCache.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocaleToScriptMapping.h"
#include "Page.h"
#include "PageGroup.h"
#include "PlatformMediaSessionManager.h"
#include "RenderTheme.h"
#include "Supplementable.h"
#include <wtf/Language.h>

#if ENABLE(WEB_AUDIO)
#include "AudioContext.h"
#endif

namespace WebCore {

InternalSettings::Backup::Backup(Settings& settings)
    : m_minimumDOMTimerInterval(settings.minimumDOMTimerInterval())
    , m_originalTimeWithoutMouseMovementBeforeHidingControls(settings.timeWithoutMouseMovementBeforeHidingControls())
    , m_originalEditingBehavior(settings.editingBehaviorType())
    , m_storageBlockingPolicy(settings.storageBlockingPolicy())
    , m_userInterfaceDirectionPolicy(settings.userInterfaceDirectionPolicy())
    , m_systemLayoutDirection(settings.systemLayoutDirection())
    , m_forcedColorsAreInvertedAccessibilityValue(settings.forcedColorsAreInvertedAccessibilityValue())
    , m_forcedDisplayIsMonochromeAccessibilityValue(settings.forcedDisplayIsMonochromeAccessibilityValue())
    , m_forcedPrefersContrastAccessibilityValue(settings.forcedPrefersContrastAccessibilityValue())
    , m_forcedPrefersReducedMotionAccessibilityValue(settings.forcedPrefersReducedMotionAccessibilityValue())
    , m_fontLoadTimingOverride(settings.fontLoadTimingOverride())
    , m_customPasteboardDataEnabled(DeprecatedGlobalSettings::customPasteboardDataEnabled())
    , m_originalMockScrollbarsEnabled(DeprecatedGlobalSettings::mockScrollbarsEnabled())
#if USE(AUDIO_SESSION)
    , m_shouldManageAudioSessionCategory(DeprecatedGlobalSettings::shouldManageAudioSessionCategory())
#endif
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    , m_shouldDeactivateAudioSession(PlatformMediaSessionManager::shouldDeactivateAudioSession())
#endif
{
}

void InternalSettings::Backup::restoreTo(Settings& settings)
{
    for (const auto& standardFont : m_standardFontFamilies)
        settings.setStandardFontFamily(standardFont.value, static_cast<UScriptCode>(standardFont.key));
    m_standardFontFamilies.clear();

    for (const auto& fixedFont : m_fixedFontFamilies)
        settings.setFixedFontFamily(fixedFont.value, static_cast<UScriptCode>(fixedFont.key));
    m_fixedFontFamilies.clear();

    for (const auto& serifFont : m_serifFontFamilies)
        settings.setSerifFontFamily(serifFont.value, static_cast<UScriptCode>(serifFont.key));
    m_serifFontFamilies.clear();

    for (const auto& sansSerifFont : m_sansSerifFontFamilies)
        settings.setSansSerifFontFamily(sansSerifFont.value, static_cast<UScriptCode>(sansSerifFont.key));
    m_sansSerifFontFamilies.clear();

    for (const auto& cursiveFont : m_cursiveFontFamilies)
        settings.setCursiveFontFamily(cursiveFont.value, static_cast<UScriptCode>(cursiveFont.key));
    m_cursiveFontFamilies.clear();

    for (const auto& fantasyFont : m_fantasyFontFamilies)
        settings.setFantasyFontFamily(fantasyFont.value, static_cast<UScriptCode>(fantasyFont.key));
    m_fantasyFontFamilies.clear();

    for (const auto& pictographFont : m_pictographFontFamilies)
        settings.setPictographFontFamily(pictographFont.value, static_cast<UScriptCode>(pictographFont.key));
    m_pictographFontFamilies.clear();

    settings.setMinimumDOMTimerInterval(m_minimumDOMTimerInterval);
    settings.setTimeWithoutMouseMovementBeforeHidingControls(m_originalTimeWithoutMouseMovementBeforeHidingControls);
    settings.setEditingBehaviorType(m_originalEditingBehavior);
    settings.setStorageBlockingPolicy(m_storageBlockingPolicy);
    settings.setUserInterfaceDirectionPolicy(m_userInterfaceDirectionPolicy);
    settings.setSystemLayoutDirection(m_systemLayoutDirection);
    settings.setForcedColorsAreInvertedAccessibilityValue(m_forcedColorsAreInvertedAccessibilityValue);
    settings.setForcedDisplayIsMonochromeAccessibilityValue(m_forcedDisplayIsMonochromeAccessibilityValue);
    settings.setForcedPrefersContrastAccessibilityValue(m_forcedPrefersContrastAccessibilityValue);
    settings.setForcedPrefersReducedMotionAccessibilityValue(m_forcedPrefersReducedMotionAccessibilityValue);
    settings.setFontLoadTimingOverride(m_fontLoadTimingOverride);

    DeprecatedGlobalSettings::setCustomPasteboardDataEnabled(m_customPasteboardDataEnabled);

#if USE(AUDIO_SESSION)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(m_shouldManageAudioSessionCategory);
#endif

#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::setShouldDeactivateAudioSession(m_shouldDeactivateAudioSession);
#endif

#if ENABLE(WEB_AUDIO)
    AudioContext::setDefaultSampleRateForTesting(std::nullopt);
#endif
}

class InternalSettingsWrapper : public Supplement<Page> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit InternalSettingsWrapper(Page* page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if ASSERT_ENABLED
    bool isRefCountedWrapper() const override { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

private:
    RefPtr<InternalSettings> m_internalSettings;
};

ASCIILiteral InternalSettings::supplementName()
{
    return "InternalSettings"_s;
}

InternalSettings* InternalSettings::from(Page* page)
{
    if (!Supplement<Page>::from(page, supplementName()))
        Supplement<Page>::provideTo(page, supplementName(), makeUnique<InternalSettingsWrapper>(page));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, supplementName()))->internalSettings();
}

void InternalSettings::hostDestroyed()
{
    m_page = nullptr;
}

InternalSettings::InternalSettings(Page* page)
    : InternalSettingsGenerated(page)
    , m_page(page)
    , m_backup(page->settings())
{
}

Ref<InternalSettings> InternalSettings::create(Page* page)
{
    return adoptRef(*new InternalSettings(page));
}

void InternalSettings::resetToConsistentState()
{
    m_page->setPageScaleFactor(1, { 0, 0 });
    if (auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame()))
        localMainFrame->setPageAndTextZoomFactors(1, 1);
    m_page->setCanStartMedia(true);
    m_page->effectiveAppearanceDidChange(false, false);

    m_backup.restoreTo(settings());
    m_backup = Backup { settings() };

    m_page->settings().resetToConsistentState();

    InternalSettingsGenerated::resetToConsistentState();
}

Settings& InternalSettings::settings() const
{
    ASSERT(m_page);
    return m_page->settings();
}

ExceptionOr<void> InternalSettings::setStandardFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_standardFontFamilies.add(code, settings().standardFontFamily(code));
    settings().setStandardFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setSerifFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_serifFontFamilies.add(code, settings().serifFontFamily(code));
    settings().setSerifFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setSansSerifFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_sansSerifFontFamilies.add(code, settings().sansSerifFontFamily(code));
    settings().setSansSerifFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setFixedFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_fixedFontFamilies.add(code, settings().fixedFontFamily(code));
    settings().setFixedFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setCursiveFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_cursiveFontFamilies.add(code, settings().cursiveFontFamily(code));
    settings().setCursiveFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setFantasyFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_fantasyFontFamilies.add(code, settings().fantasyFontFamily(code));
    settings().setFantasyFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setPictographFontFamily(const String& family, const String& script)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return { };
    m_backup.m_pictographFontFamilies.add(code, settings().pictographFontFamily(code));
    settings().setPictographFontFamily(family, code);
    return { };
}

ExceptionOr<void> InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(TEXT_AUTOSIZING)
    settings().setTextAutosizingWindowSizeOverrideWidth(width);
    settings().setTextAutosizingWindowSizeOverrideHeight(height);
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
#endif
    return { };
}

ExceptionOr<void> InternalSettings::setEditingBehavior(EditingBehaviorType editingBehaviorType)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setEditingBehaviorType(editingBehaviorType);
    return { };
}

ExceptionOr<void> InternalSettings::setStorageBlockingPolicy(StorageBlockingPolicy policy)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setStorageBlockingPolicy(policy);
    return { };
}

ExceptionOr<void> InternalSettings::setMinimumTimerInterval(double intervalInSeconds)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setMinimumDOMTimerInterval(Seconds { intervalInSeconds });
    return { };
}

ExceptionOr<void> InternalSettings::setTimeWithoutMouseMovementBeforeHidingControls(double time)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setTimeWithoutMouseMovementBeforeHidingControls(Seconds { time });
    return { };
}

ExceptionOr<void> InternalSettings::setFontLoadTimingOverride(FontLoadTimingOverride fontLoadTimingOverride)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setFontLoadTimingOverride(fontLoadTimingOverride);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowAnimationControlsOverride(bool allowAnimationControls)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setAllowAnimationControlsOverride(allowAnimationControls);
    return { };
}

ExceptionOr<void> InternalSettings::setUserInterfaceDirectionPolicy(UserInterfaceDirectionPolicy policy)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setUserInterfaceDirectionPolicy(policy);
    return { };
}

ExceptionOr<void> InternalSettings::setSystemLayoutDirection(SystemLayoutDirection direction)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    settings().setSystemLayoutDirection(direction);
    return { };
}

InternalSettings::ForcedAccessibilityValue InternalSettings::forcedColorsAreInvertedAccessibilityValue() const
{
    return settings().forcedColorsAreInvertedAccessibilityValue();
}

void InternalSettings::setForcedColorsAreInvertedAccessibilityValue(InternalSettings::ForcedAccessibilityValue value)
{
    settings().setForcedColorsAreInvertedAccessibilityValue(value);
}

InternalSettings::ForcedAccessibilityValue InternalSettings::forcedDisplayIsMonochromeAccessibilityValue() const
{
    return settings().forcedDisplayIsMonochromeAccessibilityValue();
}

void InternalSettings::setForcedDisplayIsMonochromeAccessibilityValue(InternalSettings::ForcedAccessibilityValue value)
{
    settings().setForcedDisplayIsMonochromeAccessibilityValue(value);
}

InternalSettings::ForcedAccessibilityValue InternalSettings::forcedPrefersContrastAccessibilityValue() const
{
    return settings().forcedPrefersContrastAccessibilityValue();
}

void InternalSettings::setForcedPrefersContrastAccessibilityValue(InternalSettings::ForcedAccessibilityValue value)
{
    settings().setForcedPrefersContrastAccessibilityValue(value);
}

InternalSettings::ForcedAccessibilityValue InternalSettings::forcedPrefersReducedMotionAccessibilityValue() const
{
    return settings().forcedPrefersReducedMotionAccessibilityValue();
}

void InternalSettings::setForcedPrefersReducedMotionAccessibilityValue(InternalSettings::ForcedAccessibilityValue value)
{
    settings().setForcedPrefersReducedMotionAccessibilityValue(value);
}

InternalSettings::ForcedAccessibilityValue InternalSettings::forcedSupportsHighDynamicRangeValue() const
{
    return settings().forcedSupportsHighDynamicRangeValue();
}

void InternalSettings::setForcedSupportsHighDynamicRangeValue(InternalSettings::ForcedAccessibilityValue value)
{
    settings().setForcedSupportsHighDynamicRangeValue(value);
}

bool InternalSettings::vp9DecoderEnabled() const
{
#if ENABLE(VP9)
    return m_page ? m_page->settings().vp9DecoderEnabled() : false;
#else
    return false;
#endif
}

bool InternalSettings::mediaSourceInlinePaintingEnabled() const
{
#if ENABLE(MEDIA_SOURCE) && (HAVE(AVSAMPLEBUFFERVIDEOOUTPUT) || USE(GSTREAMER))
    return DeprecatedGlobalSettings::mediaSourceInlinePaintingEnabled();
#else
    return false;
#endif
}

ExceptionOr<void> InternalSettings::setCustomPasteboardDataEnabled(bool enabled)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    DeprecatedGlobalSettings::setCustomPasteboardDataEnabled(enabled);
    return { };
}

ExceptionOr<void> InternalSettings::setShouldManageAudioSessionCategory(bool should)
{
#if USE(AUDIO_SESSION)
    DeprecatedGlobalSettings::setShouldManageAudioSessionCategory(should);
    return { };
#else
    UNUSED_PARAM(should);
    return Exception { ExceptionCode::InvalidAccessError };
#endif
}

ExceptionOr<void> InternalSettings::setShouldDisplayTrackKind(TrackKind kind, bool enabled)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(VIDEO)
    auto& captionPreferences = m_page->group().ensureCaptionPreferences();
    switch (kind) {
    case TrackKind::Subtitles:
        captionPreferences.setUserPrefersSubtitles(enabled);
        break;
    case TrackKind::Captions:
        captionPreferences.setUserPrefersCaptions(enabled);
        break;
    case TrackKind::TextDescriptions:
        captionPreferences.setUserPrefersTextDescriptions(enabled);
        break;
    }
#else
    UNUSED_PARAM(kind);
    UNUSED_PARAM(enabled);
#endif
    return { };
}

ExceptionOr<bool> InternalSettings::shouldDisplayTrackKind(TrackKind kind)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(VIDEO)
    auto& captionPreferences = m_page->group().ensureCaptionPreferences();
    switch (kind) {
    case TrackKind::Subtitles:
        return captionPreferences.userPrefersSubtitles();
    case TrackKind::Captions:
        return captionPreferences.userPrefersCaptions();
    case TrackKind::TextDescriptions:
        return captionPreferences.userPrefersTextDescriptions();
    }
#else
    UNUSED_PARAM(kind);
#endif
    return false;
}

ExceptionOr<void> InternalSettings::setEditableRegionEnabled(bool enabled)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(EDITABLE_REGION)
    m_page->setEditableRegionEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
    return { };
}

ExceptionOr<void> InternalSettings::setCanStartMedia(bool enabled)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->setCanStartMedia(enabled);
    return { };
}

ExceptionOr<void> InternalSettings::setUseDarkAppearance(bool useDarkAppearance)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->effectiveAppearanceDidChange(useDarkAppearance, m_page->useElevatedUserInterfaceLevel());
    return { };
}

ExceptionOr<void> InternalSettings::setUseElevatedUserInterfaceLevel(bool useElevatedUserInterfaceLevel)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->effectiveAppearanceDidChange(m_page->useDarkAppearance(), useElevatedUserInterfaceLevel);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowUnclampedScrollPosition(bool allowUnclamped)
{
    auto* localMainFrame = dynamicDowncast<LocalFrame>(m_page->mainFrame());
    if (!m_page || !localMainFrame || !localMainFrame->view())
        return Exception { ExceptionCode::InvalidAccessError };

    localMainFrame->view()->setAllowsUnclampedScrollPositionForTesting(allowUnclamped);
    return { };
}

ExceptionOr<void>  InternalSettings::setShouldDeactivateAudioSession(bool should)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(VIDEO) || ENABLE(WEB_AUDIO)
    PlatformMediaSessionManager::setShouldDeactivateAudioSession(should);
#else
    UNUSED_PARAM(should);
#endif
    return { };
}

ExceptionOr<void> InternalSettings::setShouldMockBoldSystemFontForAccessibility(bool should)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    FontCache::invalidateAllFontCaches();
#if PLATFORM(COCOA)
    setOverrideEnhanceTextLegibility(should);
#else
    UNUSED_PARAM(should);
#endif
    return { };
}

ExceptionOr<void> InternalSettings::setDefaultAudioContextSampleRate(float sampleRate)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
#if ENABLE(WEB_AUDIO)
    AudioContext::setDefaultSampleRateForTesting(sampleRate);
#else
    UNUSED_PARAM(sampleRate);
#endif
    return { };
}

ExceptionOr<void> InternalSettings::setAllowedMediaContainerTypes(const String& types)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->settings().setAllowedMediaContainerTypes(types);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowedMediaCodecTypes(const String& types)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->settings().setAllowedMediaCodecTypes(types);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowedMediaVideoCodecIDs(const String& types)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->settings().setAllowedMediaVideoCodecIDs(types);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowedMediaAudioCodecIDs(const String& types)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->settings().setAllowedMediaAudioCodecIDs(types);
    return { };
}

ExceptionOr<void> InternalSettings::setAllowedMediaCaptionFormatTypes(const String& types)
{
    if (!m_page)
        return Exception { ExceptionCode::InvalidAccessError };
    m_page->settings().setAllowedMediaCaptionFormatTypes(types);
    return { };
}



// If you add to this class, make sure you are not duplicating functionality in the generated
// base class InternalSettingsGenerated and that you update the Backup class for test reproducability.

}
