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

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorController.h"
#include "Language.h"
#include "LocaleToScriptMapping.h"
#include "MockPagePopupDriver.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"
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

#define InternalSettingsGuardForPageReturn(returnValue) \
    if (!page()) { \
        ec = INVALID_ACCESS_ERR; \
        return returnValue; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Page* page, Settings* settings)
    : m_originalPasswordEchoDurationInSeconds(settings->passwordEchoDurationInSeconds())
    , m_originalPasswordEchoEnabled(settings->passwordEchoEnabled())
    , m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
#if ENABLE(SHADOW_DOM)
    , m_originalShadowDOMEnabled(RuntimeEnabledFeatures::shadowDOMEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
#endif
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalFixedPositionCreatesStackingContext(settings->fixedPositionCreatesStackingContext())
    , m_originalSyncXHRInDocumentsEnabled(settings->syncXHRInDocumentsEnabled())
#if ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)
    , m_originalJavaScriptProfilingEnabled(page->inspectorController() && page->inspectorController()->profilerEnabled())
#endif
    , m_originalWindowFocusRestricted(settings->windowFocusRestricted())
    , m_originalDeviceSupportsTouch(settings->deviceSupportsTouch())
    , m_originalDeviceSupportsMouse(settings->deviceSupportsMouse())
#if ENABLE(TEXT_AUTOSIZING)
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalTextAutosizingFontScaleFactor(settings->textAutosizingFontScaleFactor())
#endif
#if ENABLE(DIALOG_ELEMENT)
    , m_originalDialogElementEnabled(RuntimeEnabledFeatures::dialogElementEnabled())
#endif
    , m_canStartMedia(page->canStartMedia())
{
}


void InternalSettings::Backup::restoreTo(Page* page, Settings* settings)
{
    settings->setPasswordEchoDurationInSeconds(m_originalPasswordEchoDurationInSeconds);
    settings->setPasswordEchoEnabled(m_originalPasswordEchoEnabled);
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
#if ENABLE(SHADOW_DOM)
    RuntimeEnabledFeatures::setShadowDOMEnabled(m_originalShadowDOMEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
#endif
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setFixedPositionCreatesStackingContext(m_originalFixedPositionCreatesStackingContext);
    settings->setSyncXHRInDocumentsEnabled(m_originalSyncXHRInDocumentsEnabled);
#if ENABLE(INSPECTOR) && ENABLE(JAVASCRIPT_DEBUGGER)
    if (page->inspectorController())
        page->inspectorController()->setProfilerEnabled(m_originalJavaScriptProfilingEnabled);
#endif
    settings->setWindowFocusRestricted(m_originalWindowFocusRestricted);
    settings->setDeviceSupportsTouch(m_originalDeviceSupportsTouch);
    settings->setDeviceSupportsMouse(m_originalDeviceSupportsMouse);
#if ENABLE(TEXT_AUTOSIZING)
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setTextAutosizingFontScaleFactor(m_originalTextAutosizingFontScaleFactor);
#endif
#if ENABLE(DIALOG_ELEMENT)
    RuntimeEnabledFeatures::setDialogElementEnabled(m_originalDialogElementEnabled);
#endif
    page->setCanStartMedia(m_canStartMedia);
}

InternalSettings* InternalSettings::from(Page* page)
{
    DEFINE_STATIC_LOCAL(AtomicString, name, ("InternalSettings"));
    if (!SuperType::from(page, name))
        SuperType::provideTo(page, name, adoptRef(new InternalSettings(page)));
    return static_cast<InternalSettings*>(SuperType::from(page, name));
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page* page)
    : m_page(page)
    , m_backup(page, page->settings())
{
}

#if ENABLE(PAGE_POPUP)
PagePopupController* InternalSettings::pagePopupController()
{
    return m_pagePopupDriver ? m_pagePopupDriver->pagePopupController() : 0;
}
#endif

void InternalSettings::reset()
{
    TextRun::setAllowsRoundingHacks(false);
    setUserPreferredLanguages(Vector<String>());
    page()->setPagination(Page::Pagination());
    page()->setPageScaleFactor(1, IntPoint(0, 0));
#if ENABLE(PAGE_POPUP)
    m_pagePopupDriver.clear();
    if (page()->chrome())
        page()->chrome()->client()->resetPagePopupDriver();
#endif

    m_backup.restoreTo(page(), settings());
    m_backup = Backup(page(), settings());
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return page()->settings();
}

void InternalSettings::setInspectorResourcesDataSizeLimits(int maximumResourcesContentSize, int maximumSingleResourceContentSize, ExceptionCode& ec)
{
#if ENABLE(INSPECTOR)
    if (!page() || !page()->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    page()->inspectorController()->setResourcesDataSizeLimitsFromInternals(maximumResourcesContentSize, maximumSingleResourceContentSize);
#else
    UNUSED_PARAM(maximumResourcesContentSize);
    UNUSED_PARAM(maximumSingleResourceContentSize);
    UNUSED_PARAM(ec);
#endif
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

void InternalSettings::setPasswordEchoEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setPasswordEchoEnabled(enabled);
}

void InternalSettings::setPasswordEchoDurationInSeconds(double durationInSeconds, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setPasswordEchoDurationInSeconds(durationInSeconds);
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

void InternalSettings::setPageScaleFactor(float scaleFactor, int x, int y, ExceptionCode& ec)
{
    InternalSettingsGuardForPage();
    page()->setPageScaleFactor(scaleFactor, IntPoint(x, y));
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

void InternalSettings::setJavaScriptProfilingEnabled(bool enabled, ExceptionCode& ec)
{
#if ENABLE(INSPECTOR)
    if (!page() || !page()->inspectorController()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    page()->inspectorController()->setProfilerEnabled(enabled);
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
    return;
#endif
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

void InternalSettings::allowRoundingHacks() const
{
    TextRun::setAllowsRoundingHacks(true);
}

Vector<String> InternalSettings::userPreferredLanguages() const
{
    return WebCore::userPreferredLanguages();
}

void InternalSettings::setUserPreferredLanguages(const Vector<String>& languages)
{
    WebCore::overrideUserPreferredLanguages(languages);
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

void InternalSettings::setPagination(const String& mode, int gap, int pageLength, ExceptionCode& ec)
{
    if (!page()) {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    Page::Pagination pagination;
    if (mode == "Unpaginated")
        pagination.mode = Page::Pagination::Unpaginated;
    else if (mode == "LeftToRightPaginated")
        pagination.mode = Page::Pagination::LeftToRightPaginated;
    else if (mode == "RightToLeftPaginated")
        pagination.mode = Page::Pagination::RightToLeftPaginated;
    else if (mode == "TopToBottomPaginated")
        pagination.mode = Page::Pagination::TopToBottomPaginated;
    else if (mode == "BottomToTopPaginated")
        pagination.mode = Page::Pagination::BottomToTopPaginated;
    else {
        ec = SYNTAX_ERR;
        return;
    }

    pagination.gap = gap;
    pagination.pageLength = pageLength;
    page()->setPagination(pagination);
}

void InternalSettings::setEnableMockPagePopup(bool enabled, ExceptionCode& ec)
{
#if ENABLE(PAGE_POPUP)
    InternalSettingsGuardForPage();
    if (!page()->chrome())
        return;
    if (!enabled) {
        page()->chrome()->client()->resetPagePopupDriver();
        return;
    }
    if (!m_pagePopupDriver)
        m_pagePopupDriver = MockPagePopupDriver::create(page()->mainFrame());
    page()->chrome()->client()->setPagePopupDriver(m_pagePopupDriver.get());
#else
    UNUSED_PARAM(enabled);
    UNUSED_PARAM(ec);
#endif
}

String InternalSettings::configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode& ec)
{
    if (!page()) {
        ec = INVALID_ACCESS_ERR;
        return String();
    }

    const int defaultLayoutWidthForNonMobilePages = 980;

    ViewportArguments arguments = page()->viewportArguments();
    ViewportAttributes attributes = computeViewportAttributes(arguments, defaultLayoutWidthForNonMobilePages, deviceWidth, deviceHeight, devicePixelRatio, IntSize(availableWidth, availableHeight));
    restrictMinimumScaleFactorToViewportSize(attributes, IntSize(availableWidth, availableHeight));
    restrictScaleFactorToInitialScaleIfNotUserScalable(attributes);

    return "viewport size " + String::number(attributes.layoutSize.width()) + "x" + String::number(attributes.layoutSize.height()) + " scale " + String::number(attributes.initialScale) + " with limits [" + String::number(attributes.minimumScale) + ", " + String::number(attributes.maximumScale) + "] and userScalable " + (attributes.userScalable ? "true" : "false");
}

void InternalSettings::setMemoryInfoEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMemoryInfoEnabled(enabled);
}

void InternalSettings::setThirdPartyStorageBlockingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setThirdPartyStorageBlockingEnabled(enabled);
}

}
