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

#include "CachedResourceLoader.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameView.h"
#include "InspectorController.h"
#include "Language.h"
#include "LocaleToScriptMapping.h"
#include "Page.h"
#include "RuntimeEnabledFeatures.h"
#include "Settings.h"

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

#define InternalSettingsGuardForFrame() \
    if (!frame()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

#define InternalSettingsGuardForFrameView() \
    if (!frame() || !frame()->view()) { \
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


PassRefPtr<InternalSettings> InternalSettings::create(Frame* frame)
{
    return adoptRef(new InternalSettings(frame));
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Frame* frame)
    : FrameDestructionObserver(frame)
    , m_originalPasswordEchoDurationInSeconds(settings()->passwordEchoDurationInSeconds())
    , m_originalPasswordEchoEnabled(settings()->passwordEchoEnabled())
    , m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
#if ENABLE(SHADOW_DOM)
    , m_originalShadowDOMEnabled(RuntimeEnabledFeatures::shadowDOMEnabled())
#endif
{
}

void InternalSettings::restoreTo(Settings* settings)
{
    settings->setPasswordEchoDurationInSeconds(m_originalPasswordEchoDurationInSeconds);
    settings->setPasswordEchoEnabled(m_originalPasswordEchoEnabled);
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
#if ENABLE(SHADOW_DOM)
    RuntimeEnabledFeatures::setShadowDOMEnabled(m_originalShadowDOMEnabled);
#endif
}

Settings* InternalSettings::settings() const
{
    if (!frame() || !frame()->page())
        return 0;
    return frame()->page()->settings();
}

Document* InternalSettings::document() const
{
    return frame() ? frame()->document() : 0;
}

Page* InternalSettings::page() const
{
    return document() ? document()->page() : 0;
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
    InternalSettingsGuardForFrameView();
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

void InternalSettings::setPerTileDrawingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setPerTileDrawingEnabled(enabled);
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

void InternalSettings::setMediaPlaybackRequiresUserGesture(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaPlaybackRequiresUserGesture(enabled);
}

void InternalSettings::setUnsafePluginPastingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setUnsafePluginPastingEnabled(enabled);
}

}
