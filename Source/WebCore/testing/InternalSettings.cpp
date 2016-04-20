/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "ExceptionCode.h"
#include "FrameView.h"
#include "Language.h"
#include "LocaleToScriptMapping.h"
#include "MainFrame.h"
#include "Page.h"
#include "PageGroup.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
#include "Supplementable.h"
#include "TextRun.h"

#if ENABLE(INPUT_TYPE_COLOR)
#include "ColorChooser.h"
#endif

#define InternalSettingsGuardForSettingsReturn(returnValue) \
    if (!settings()) { \
        ec = INVALID_ACCESS_ERR; \
        return returnValue; \
    }

#define InternalSettingsGuardForSettings()  \
    if (!settings()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Settings& settings)
    : m_originalCSSShapesEnabled(RuntimeEnabledFeatures::sharedFeatures().cssShapesEnabled())
    , m_originalEditingBehavior(settings.editingBehaviorType())
#if ENABLE(TEXT_AUTOSIZING)
    , m_originalTextAutosizingEnabled(settings.textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings.textAutosizingWindowSizeOverride())
    , m_originalTextAutosizingFontScaleFactor(settings.textAutosizingFontScaleFactor())
#endif
    , m_originalMediaTypeOverride(settings.mediaTypeOverride())
    , m_originalCanvasUsesAcceleratedDrawing(settings.canvasUsesAcceleratedDrawing())
    , m_originalMockScrollbarsEnabled(settings.mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::sharedFeatures().langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings.areImagesEnabled())
    , m_minimumTimerInterval(settings.minimumDOMTimerInterval())
#if ENABLE(VIDEO_TRACK)
    , m_shouldDisplaySubtitles(settings.shouldDisplaySubtitles())
    , m_shouldDisplayCaptions(settings.shouldDisplayCaptions())
    , m_shouldDisplayTextDescriptions(settings.shouldDisplayTextDescriptions())
#endif
    , m_defaultVideoPosterURL(settings.defaultVideoPosterURL())
    , m_forcePendingWebGLPolicy(settings.isForcePendingWebGLPolicy())
    , m_originalTimeWithoutMouseMovementBeforeHidingControls(settings.timeWithoutMouseMovementBeforeHidingControls())
    , m_useLegacyBackgroundSizeShorthandBehavior(settings.useLegacyBackgroundSizeShorthandBehavior())
    , m_autoscrollForDragAndDropEnabled(settings.autoscrollForDragAndDropEnabled())
    , m_pluginReplacementEnabled(RuntimeEnabledFeatures::sharedFeatures().pluginReplacementEnabled())
    , m_shouldConvertPositionStyleOnCopy(settings.shouldConvertPositionStyleOnCopy())
    , m_fontFallbackPrefersPictographs(settings.fontFallbackPrefersPictographs())
    , m_backgroundShouldExtendBeyondPage(settings.backgroundShouldExtendBeyondPage())
    , m_storageBlockingPolicy(settings.storageBlockingPolicy())
    , m_scrollingTreeIncludesFrames(settings.scrollingTreeIncludesFrames())
#if ENABLE(TOUCH_EVENTS)
    , m_touchEventEmulationEnabled(settings.isTouchEventEmulationEnabled())
#endif
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    , m_allowsAirPlayForMediaPlayback(settings.allowsAirPlayForMediaPlayback())
#endif
    , m_allowsInlineMediaPlayback(settings.allowsInlineMediaPlayback())
    , m_inlineMediaPlaybackRequiresPlaysInlineAttribute(settings.inlineMediaPlaybackRequiresPlaysInlineAttribute())
#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    , m_indexedDBWorkersEnabled(RuntimeEnabledFeatures::sharedFeatures().indexedDBWorkersEnabled())
#endif
{
}

void InternalSettings::Backup::restoreTo(Settings& settings)
{
    RuntimeEnabledFeatures::sharedFeatures().setCSSShapesEnabled(m_originalCSSShapesEnabled);
    settings.setEditingBehaviorType(m_originalEditingBehavior);

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

#if ENABLE(TEXT_AUTOSIZING)
    settings.setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings.setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings.setTextAutosizingFontScaleFactor(m_originalTextAutosizingFontScaleFactor);
#endif
    settings.setMediaTypeOverride(m_originalMediaTypeOverride);
    settings.setCanvasUsesAcceleratedDrawing(m_originalCanvasUsesAcceleratedDrawing);
    RuntimeEnabledFeatures::sharedFeatures().setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings.setImagesEnabled(m_imagesEnabled);
    settings.setMinimumDOMTimerInterval(m_minimumTimerInterval);
#if ENABLE(VIDEO_TRACK)
    settings.setShouldDisplaySubtitles(m_shouldDisplaySubtitles);
    settings.setShouldDisplayCaptions(m_shouldDisplayCaptions);
    settings.setShouldDisplayTextDescriptions(m_shouldDisplayTextDescriptions);
#endif
    settings.setDefaultVideoPosterURL(m_defaultVideoPosterURL);
    settings.setForcePendingWebGLPolicy(m_forcePendingWebGLPolicy);
    settings.setTimeWithoutMouseMovementBeforeHidingControls(m_originalTimeWithoutMouseMovementBeforeHidingControls);
    settings.setUseLegacyBackgroundSizeShorthandBehavior(m_useLegacyBackgroundSizeShorthandBehavior);
    settings.setAutoscrollForDragAndDropEnabled(m_autoscrollForDragAndDropEnabled);
    settings.setShouldConvertPositionStyleOnCopy(m_shouldConvertPositionStyleOnCopy);
    settings.setFontFallbackPrefersPictographs(m_fontFallbackPrefersPictographs);
    settings.setBackgroundShouldExtendBeyondPage(m_backgroundShouldExtendBeyondPage);
    settings.setStorageBlockingPolicy(m_storageBlockingPolicy);
    settings.setScrollingTreeIncludesFrames(m_scrollingTreeIncludesFrames);
#if ENABLE(TOUCH_EVENTS)
    settings.setTouchEventEmulationEnabled(m_touchEventEmulationEnabled);
#endif
    settings.setAllowsInlineMediaPlayback(m_allowsInlineMediaPlayback);
    settings.setInlineMediaPlaybackRequiresPlaysInlineAttribute(m_inlineMediaPlaybackRequiresPlaysInlineAttribute);
    RuntimeEnabledFeatures::sharedFeatures().setPluginReplacementEnabled(m_pluginReplacementEnabled);
#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    RuntimeEnabledFeatures::sharedFeatures().setIndexedDBWorkersEnabled(m_indexedDBWorkersEnabled);
#endif
}

class InternalSettingsWrapper : public Supplement<Page> {
public:
    explicit InternalSettingsWrapper(Page* page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if !ASSERT_DISABLED
    bool isRefCountedWrapper() const override { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

private:
    RefPtr<InternalSettings> m_internalSettings;
};

const char* InternalSettings::supplementName()
{
    return "InternalSettings";
}

InternalSettings* InternalSettings::from(Page* page)
{
    if (!Supplement<Page>::from(page, supplementName()))
        Supplement<Page>::provideTo(page, supplementName(), std::make_unique<InternalSettingsWrapper>(page));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, supplementName()))->internalSettings();
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page* page)
    : InternalSettingsGenerated(page)
    , m_page(page)
    , m_backup(page->settings())
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    page->settings().setAllowsAirPlayForMediaPlayback(false);
#endif
}

void InternalSettings::resetToConsistentState()
{
    page()->setPageScaleFactor(1, IntPoint(0, 0));
    page()->setCanStartMedia(true);
    page()->settings().setForcePendingWebGLPolicy(false);
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_page->settings().setAllowsAirPlayForMediaPlayback(false);
#endif

    m_backup.restoreTo(*settings());
    m_backup = Backup(*settings());

    InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return &page()->settings();
}

void InternalSettings::setTouchEventEmulationEnabled(bool enabled, ExceptionCode& ec)
{
#if ENABLE(TOUCH_EVENTS)
    InternalSettingsGuardForSettings();
    settings()->setTouchEventEmulationEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
#endif
}

void InternalSettings::setStandardFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_standardFontFamilies.add(code, settings()->standardFontFamily(code));
    settings()->setStandardFontFamily(family, code);
}

void InternalSettings::setSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_serifFontFamilies.add(code, settings()->serifFontFamily(code));
    settings()->setSerifFontFamily(family, code);
}

void InternalSettings::setSansSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_sansSerifFontFamilies.add(code, settings()->sansSerifFontFamily(code));
    settings()->setSansSerifFontFamily(family, code);
}

void InternalSettings::setFixedFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_fixedFontFamilies.add(code, settings()->fixedFontFamily(code));
    settings()->setFixedFontFamily(family, code);
}

void InternalSettings::setCursiveFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_cursiveFontFamilies.add(code, settings()->cursiveFontFamily(code));
    settings()->setCursiveFontFamily(family, code);
}

void InternalSettings::setFantasyFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_fantasyFontFamilies.add(code, settings()->fantasyFontFamily(code));
    settings()->setFantasyFontFamily(family, code);
}

void InternalSettings::setPictographFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    m_backup.m_pictographFontFamilies.add(code, settings()->pictographFontFamily(code));
    settings()->setPictographFontFamily(family, code);
}

void InternalSettings::setTextAutosizingEnabled(bool enabled, ExceptionCode& ec)
{
#if ENABLE(TEXT_AUTOSIZING)
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
#endif
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height, ExceptionCode& ec)
{
#if ENABLE(TEXT_AUTOSIZING)
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingWindowSizeOverride(IntSize(width, height));
#else
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
    UNUSED_PARAM(ec);
#endif
}

void InternalSettings::setMediaTypeOverride(const String& mediaType, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaTypeOverride(mediaType);
}

void InternalSettings::setTextAutosizingFontScaleFactor(float fontScaleFactor, ExceptionCode& ec)
{
#if ENABLE(TEXT_AUTOSIZING)
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingFontScaleFactor(fontScaleFactor);
#else
    UNUSED_PARAM(fontScaleFactor);
    UNUSED_PARAM(ec);
#endif
}

void InternalSettings::setCSSShapesEnabled(bool enabled, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    RuntimeEnabledFeatures::sharedFeatures().setCSSShapesEnabled(enabled);
}

void InternalSettings::setCanStartMedia(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    m_page->setCanStartMedia(enabled);
}

void InternalSettings::setWirelessPlaybackDisabled(bool available)
{
#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    m_page->settings().setAllowsAirPlayForMediaPlayback(available);
#else
    UNUSED_PARAM(available);
#endif
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    if (equalLettersIgnoringASCIICase(editingBehavior, "win"))
        settings()->setEditingBehaviorType(EditingWindowsBehavior);
    else if (equalLettersIgnoringASCIICase(editingBehavior, "mac"))
        settings()->setEditingBehaviorType(EditingMacBehavior);
    else if (equalLettersIgnoringASCIICase(editingBehavior, "unix"))
        settings()->setEditingBehaviorType(EditingUnixBehavior);
    else if (equalLettersIgnoringASCIICase(editingBehavior, "ios"))
        settings()->setEditingBehaviorType(EditingIOSBehavior);
    else
        ec = SYNTAX_ERR;
}

void InternalSettings::setShouldDisplayTrackKind(const String& kind, bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();

#if ENABLE(VIDEO_TRACK)
    if (!page())
        return;

    auto& captionPreferences = page()->group().captionPreferences();
    if (equalLettersIgnoringASCIICase(kind, "subtitles"))
        captionPreferences.setUserPrefersSubtitles(enabled);
    else if (equalLettersIgnoringASCIICase(kind, "captions"))
        captionPreferences.setUserPrefersCaptions(enabled);
    else if (equalLettersIgnoringASCIICase(kind, "textdescriptions"))
        captionPreferences.setUserPrefersTextDescriptions(enabled);
    else
        ec = SYNTAX_ERR;
#else
    UNUSED_PARAM(kind);
    UNUSED_PARAM(enabled);
#endif
}

bool InternalSettings::shouldDisplayTrackKind(const String& kind, ExceptionCode& ec)
{
    InternalSettingsGuardForSettingsReturn(false);

#if ENABLE(VIDEO_TRACK)
    if (!page())
        return false;

    auto& captionPreferences = page()->group().captionPreferences();
    if (equalLettersIgnoringASCIICase(kind, "subtitles"))
        return captionPreferences.userPrefersSubtitles();
    if (equalLettersIgnoringASCIICase(kind, "captions"))
        return captionPreferences.userPrefersCaptions();
    if (equalLettersIgnoringASCIICase(kind, "textdescriptions"))
        return captionPreferences.userPrefersTextDescriptions();

    ec = SYNTAX_ERR;
    return false;
#else
    UNUSED_PARAM(kind);
    return false;
#endif
}

void InternalSettings::setStorageBlockingPolicy(const String& mode, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();

    if (mode == "AllowAll")
        settings()->setStorageBlockingPolicy(SecurityOrigin::AllowAllStorage);
    else if (mode == "BlockThirdParty")
        settings()->setStorageBlockingPolicy(SecurityOrigin::BlockThirdPartyStorage);
    else if (mode == "BlockAll")
        settings()->setStorageBlockingPolicy(SecurityOrigin::BlockAllStorage);
    else
        ec = SYNTAX_ERR;
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled)
{
    RuntimeEnabledFeatures::sharedFeatures().setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

void InternalSettings::setMinimumTimerInterval(double intervalInSeconds, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMinimumDOMTimerInterval(std::chrono::milliseconds((std::chrono::milliseconds::rep)(intervalInSeconds * 1000)));
}

void InternalSettings::setDefaultVideoPosterURL(const String& url, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setDefaultVideoPosterURL(url);
}

void InternalSettings::setForcePendingWebGLPolicy(bool forced, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setForcePendingWebGLPolicy(forced);
}

void InternalSettings::setTimeWithoutMouseMovementBeforeHidingControls(double time, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setTimeWithoutMouseMovementBeforeHidingControls(time);
}

void InternalSettings::setUseLegacyBackgroundSizeShorthandBehavior(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setUseLegacyBackgroundSizeShorthandBehavior(enabled);
}

void InternalSettings::setAutoscrollForDragAndDropEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAutoscrollForDragAndDropEnabled(enabled);
}

void InternalSettings::setFontFallbackPrefersPictographs(bool preferPictographs, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setFontFallbackPrefersPictographs(preferPictographs);
}

void InternalSettings::setPluginReplacementEnabled(bool enabled)
{
    RuntimeEnabledFeatures::sharedFeatures().setPluginReplacementEnabled(enabled);
}

void InternalSettings::setBackgroundShouldExtendBeyondPage(bool hasExtendedBackground, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setBackgroundShouldExtendBeyondPage(hasExtendedBackground);
}

void InternalSettings::setShouldConvertPositionStyleOnCopy(bool convert, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setShouldConvertPositionStyleOnCopy(convert);
}

void InternalSettings::setScrollingTreeIncludesFrames(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setScrollingTreeIncludesFrames(enabled);
}

void InternalSettings::setAllowsInlineMediaPlayback(bool allows, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAllowsInlineMediaPlayback(allows);
}

void InternalSettings::setInlineMediaPlaybackRequiresPlaysInlineAttribute(bool requires, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setInlineMediaPlaybackRequiresPlaysInlineAttribute(requires);
}

void InternalSettings::setIndexedDBWorkersEnabled(bool enabled, ExceptionCode&)
{
#if ENABLE(INDEXED_DATABASE_IN_WORKERS)
    RuntimeEnabledFeatures::sharedFeatures().setIndexedDBWorkersEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}


// If you add to this list, make sure that you update the Backup class for test reproducability!

}
