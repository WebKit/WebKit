/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "Language.h"
#include "LocaleToScriptMapping.h"
#include "Page.h"
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

InternalSettings::Backup::Backup(Settings* settings)
    : m_originalFixedElementsLayoutRelativeToFrame(settings->fixedElementsLayoutRelativeToFrame())
    , m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
    , m_originalCSSVariablesEnabled(settings->cssVariablesEnabled())
#if ENABLE(SHADOW_DOM)
    , m_originalShadowDOMEnabled(RuntimeEnabledFeatures::shadowDOMEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
#endif
#if ENABLE(STYLE_SCOPED)
    , m_originalStyleScoped(RuntimeEnabledFeatures::styleScopedEnabled())
#endif
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalUnifiedSpellCheckerEnabled(settings->unifiedTextCheckerEnabled())
    , m_originalFixedPositionCreatesStackingContext(settings->fixedPositionCreatesStackingContext())
    , m_originalSyncXHRInDocumentsEnabled(settings->syncXHRInDocumentsEnabled())
    , m_originalWindowFocusRestricted(settings->windowFocusRestricted())
    , m_originalDeviceSupportsTouch(settings->deviceSupportsTouch())
    , m_originalDeviceSupportsMouse(settings->deviceSupportsMouse())
#if ENABLE(TEXT_AUTOSIZING)
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalTextAutosizingFontScaleFactor(settings->textAutosizingFontScaleFactor())
#endif
    , m_originalResolutionOverride(settings->resolutionOverride())
    , m_originalMediaTypeOverride(settings->mediaTypeOverride())
#if ENABLE(DIALOG_ELEMENT)
    , m_originalDialogElementEnabled(RuntimeEnabledFeatures::dialogElementEnabled())
#endif
    , m_originalForceCompositingMode(settings->forceCompositingMode())
    , m_originalCompositingForFixedPositionEnabled(settings->acceleratedCompositingForFixedPositionEnabled())
    , m_originalCompositingForScrollableFramesEnabled(settings->acceleratedCompositingForScrollableFramesEnabled())
    , m_originalAcceleratedDrawingEnabled(settings->acceleratedDrawingEnabled())
    , m_originalMockScrollbarsEnabled(settings->mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings->areImagesEnabled())
#if ENABLE(VIDEO_TRACK)
    , m_shouldDisplaySubtitles(settings->shouldDisplaySubtitles())
    , m_shouldDisplayCaptions(settings->shouldDisplayCaptions())
    , m_shouldDisplayTextDescriptions(settings->shouldDisplayTextDescriptions())
#endif
{
}

void InternalSettings::Backup::restoreTo(Settings* settings)
{
    settings->setFixedElementsLayoutRelativeToFrame(m_originalFixedElementsLayoutRelativeToFrame);
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
    settings->setCSSVariablesEnabled(m_originalCSSVariablesEnabled);
#if ENABLE(SHADOW_DOM)
    RuntimeEnabledFeatures::setShadowDOMEnabled(m_originalShadowDOMEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
#endif
#if ENABLE(STYLE_SCOPED)
    RuntimeEnabledFeatures::setStyleScopedEnabled(m_originalStyleScoped);
#endif
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setUnifiedTextCheckerEnabled(m_originalUnifiedSpellCheckerEnabled);
    settings->setFixedPositionCreatesStackingContext(m_originalFixedPositionCreatesStackingContext);
    settings->setSyncXHRInDocumentsEnabled(m_originalSyncXHRInDocumentsEnabled);
    settings->setWindowFocusRestricted(m_originalWindowFocusRestricted);
    settings->setDeviceSupportsTouch(m_originalDeviceSupportsTouch);
    settings->setDeviceSupportsMouse(m_originalDeviceSupportsMouse);
#if ENABLE(TEXT_AUTOSIZING)
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setTextAutosizingFontScaleFactor(m_originalTextAutosizingFontScaleFactor);
#endif
    settings->setResolutionOverride(m_originalResolutionOverride);
    settings->setMediaTypeOverride(m_originalMediaTypeOverride);
#if ENABLE(DIALOG_ELEMENT)
    RuntimeEnabledFeatures::setDialogElementEnabled(m_originalDialogElementEnabled);
#endif
    settings->setForceCompositingMode(m_originalForceCompositingMode);
    settings->setAcceleratedCompositingForFixedPositionEnabled(m_originalCompositingForFixedPositionEnabled);
    settings->setAcceleratedCompositingForScrollableFramesEnabled(m_originalCompositingForScrollableFramesEnabled);
    settings->setAcceleratedDrawingEnabled(m_originalAcceleratedDrawingEnabled);
    settings->setMockScrollbarsEnabled(m_originalMockScrollbarsEnabled);
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings->setImagesEnabled(m_imagesEnabled);
#if ENABLE(VIDEO_TRACK)
    settings->setShouldDisplaySubtitles(m_shouldDisplaySubtitles);
    settings->setShouldDisplayCaptions(m_shouldDisplayCaptions);
    settings->setShouldDisplayTextDescriptions(m_shouldDisplayTextDescriptions);
#endif
}

// We can't use RefCountedSupplement because that would try to make InternalSettings RefCounted
// and InternalSettings is already RefCounted via its base class, InternalSettingsGenerated.
// Instead, we manually make InternalSettings supplement Page.
class InternalSettingsWrapper : public Supplement<Page> {
public:
    explicit InternalSettingsWrapper(Page* page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if !ASSERT_DISABLED
    virtual bool isRefCountedWrapper() const OVERRIDE { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

private:
    RefPtr<InternalSettings> m_internalSettings;
};

InternalSettings* InternalSettings::from(Page* page)
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("InternalSettings", AtomicString::ConstructFromLiteral));
    if (!Supplement<Page>::from(page, name))
        Supplement<Page>::provideTo(page, name, adoptPtr(new InternalSettingsWrapper(page)));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, name))->internalSettings();
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page* page)
    : InternalSettingsGenerated(page)
    , m_page(page)
    , m_backup(page->settings())
{
}

void InternalSettings::resetToConsistentState()
{
    page()->setPageScaleFactor(1, IntPoint(0, 0));
    page()->setCanStartMedia(true);

    m_backup.restoreTo(settings());
    m_backup = Backup(settings());

    InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return page()->settings();
}

void InternalSettings::setForceCompositingMode(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setForceCompositingMode(enabled);
}

void InternalSettings::setAcceleratedFiltersEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAcceleratedFiltersEnabled(enabled);
}

void InternalSettings::setEnableCompositingForFixedPosition(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAcceleratedCompositingForFixedPositionEnabled(enabled);
}

void InternalSettings::setEnableCompositingForScrollableFrames(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAcceleratedCompositingForScrollableFramesEnabled(enabled);
}

void InternalSettings::setEnableCompositingForOverflowScroll(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAcceleratedCompositingForOverflowScrollEnabled(enabled);
}

void InternalSettings::setAcceleratedDrawingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setAcceleratedDrawingEnabled(enabled);
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setFixedElementsLayoutRelativeToFrame(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setFixedElementsLayoutRelativeToFrame(enabled);
}

void InternalSettings::setUnifiedTextCheckingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setUnifiedTextCheckerEnabled(enabled);
}

bool InternalSettings::unifiedTextCheckingEnabled(ExceptionCode& ec)
{
    InternalSettingsGuardForSettingsReturn(false);
    return settings()->unifiedTextCheckerEnabled();
}

void InternalSettings::setShadowDOMEnabled(bool enabled, ExceptionCode& ec)
{
#if ENABLE(SHADOW_DOM)
    UNUSED_PARAM(ec);
    RuntimeEnabledFeatures::setShadowDOMEnabled(enabled);
#else
    // Even SHADOW_DOM is off, InternalSettings allows setShadowDOMEnabled(false) to
    // have broader test coverage. But it cannot be setShadowDOMEnabled(true).
    if (enabled)
        ec = INVALID_ACCESS_ERR;
#endif
}

void InternalSettings::setAuthorShadowDOMForAnyElementEnabled(bool isEnabled)
{
#if ENABLE(SHADOW_DOM)
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(isEnabled);
#else
    UNUSED_PARAM(isEnabled);
#endif
}

void InternalSettings::setStyleScopedEnabled(bool enabled)
{
#if ENABLE(STYLE_SCOPED)
    RuntimeEnabledFeatures::setStyleScopedEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
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

void InternalSettings::setDeviceSupportsTouch(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setDeviceSupportsTouch(enabled);
}

void InternalSettings::setDeviceSupportsMouse(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setDeviceSupportsMouse(enabled);
}

typedef void (Settings::*SetFontFamilyFunction)(const AtomicString&, UScriptCode);
static void setFontFamily(Settings* settings, const String& family, const String& script, SetFontFamilyFunction setter)
{
    UScriptCode code = scriptNameToCode(script);
    if (code != USCRIPT_INVALID_CODE)
        (settings->*setter)(family, code);
}

void InternalSettings::setStandardFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setStandardFontFamily);
}

void InternalSettings::setSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSerifFontFamily);
}

void InternalSettings::setSansSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSansSerifFontFamily);
}

void InternalSettings::setFixedFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFixedFontFamily);
}

void InternalSettings::setCursiveFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setCursiveFontFamily);
}

void InternalSettings::setFantasyFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFantasyFontFamily);
}

void InternalSettings::setPictographFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setPictographFontFamily);
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

void InternalSettings::setResolutionOverride(int dotsPerCSSInchHorizontally, int dotsPerCSSInchVertically, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    // An empty size resets the override.
    settings()->setResolutionOverride(IntSize(dotsPerCSSInchHorizontally, dotsPerCSSInchVertically));
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

void InternalSettings::setEnableScrollAnimator(bool enabled, ExceptionCode& ec)
{
#if ENABLE(SMOOTH_SCROLLING)
    InternalSettingsGuardForSettings();
    settings()->setEnableScrollAnimator(enabled);
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
#endif
}

bool InternalSettings::scrollAnimatorEnabled(ExceptionCode& ec)
{
#if ENABLE(SMOOTH_SCROLLING)
    InternalSettingsGuardForSettingsReturn(false);
    return settings()->scrollAnimatorEnabled();
#else
    UNUSED_PARAM(ec);
    return false;
#endif
}

void InternalSettings::setCSSExclusionsEnabled(bool enabled, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enabled);
}

void InternalSettings::setCSSVariablesEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setCSSVariablesEnabled(enabled);
}

bool InternalSettings::cssVariablesEnabled(ExceptionCode& ec)
{
    InternalSettingsGuardForSettingsReturn(false);
    return settings()->cssVariablesEnabled();
}

void InternalSettings::setCanStartMedia(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    m_page->setCanStartMedia(enabled);
}

void InternalSettings::setMediaPlaybackRequiresUserGesture(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaPlaybackRequiresUserGesture(enabled);
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    if (equalIgnoringCase(editingBehavior, "win"))
        settings()->setEditingBehaviorType(EditingWindowsBehavior);
    else if (equalIgnoringCase(editingBehavior, "mac"))
        settings()->setEditingBehaviorType(EditingMacBehavior);
    else if (equalIgnoringCase(editingBehavior, "unix"))
        settings()->setEditingBehaviorType(EditingUnixBehavior);
    else
        ec = SYNTAX_ERR;
}

void InternalSettings::setFixedPositionCreatesStackingContext(bool creates, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setFixedPositionCreatesStackingContext(creates);
}

void InternalSettings::setSyncXHRInDocumentsEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setSyncXHRInDocumentsEnabled(enabled);
}

void InternalSettings::setWindowFocusRestricted(bool restricted, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setWindowFocusRestricted(restricted);
}

void InternalSettings::setDialogElementEnabled(bool enabled, ExceptionCode& ec)
{
    UNUSED_PARAM(ec);
#if ENABLE(DIALOG_ELEMENT)
    RuntimeEnabledFeatures::setDialogElementEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void InternalSettings::setShouldDisplayTrackKind(const String& kind, bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();

#if ENABLE(VIDEO_TRACK)
    if (equalIgnoringCase(kind, "Subtitles"))
        settings()->setShouldDisplaySubtitles(enabled);
    else if (equalIgnoringCase(kind, "Captions"))
        settings()->setShouldDisplayCaptions(enabled);
    else if (equalIgnoringCase(kind, "TextDescriptions"))
        settings()->setShouldDisplayTextDescriptions(enabled);
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
    if (equalIgnoringCase(kind, "Subtitles"))
        return settings()->shouldDisplaySubtitles();
    if (equalIgnoringCase(kind, "Captions"))
        return settings()->shouldDisplayCaptions();
    if (equalIgnoringCase(kind, "TextDescriptions"))
        return settings()->shouldDisplayTextDescriptions();

    ec = SYNTAX_ERR;
    return false;
#else
    UNUSED_PARAM(kind);
    return false;
#endif
}

void InternalSettings::setMemoryInfoEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMemoryInfoEnabled(enabled);
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
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

}
