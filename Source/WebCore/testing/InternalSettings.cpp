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
#include "Page.h"
#include "Settings.h"

#if ENABLE(GESTURE_EVENTS)
#include "PlatformGestureEvent.h"
#endif

#if ENABLE(SMOOTH_SCROLLING)
#include "ScrollAnimator.h"
#endif

#if ENABLE(INPUT_COLOR)
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


PassRefPtr<InternalSettings> InternalSettings::create(Frame* frame, InternalSettings* old)
{
    return adoptRef(new InternalSettings(frame, old));
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Frame* frame, InternalSettings* old)
    : FrameDestructionObserver(frame)
    , m_passwordEchoDurationInSecondsBackup(0)
    , m_passwordEchoDurationInSecondsBackedUp(false)
    , m_passwordEchoEnabledBackedUp(false)
{
    if (old && settings()) {
        if (old->m_passwordEchoDurationInSecondsBackedUp)
            settings()->setPasswordEchoDurationInSeconds(old->m_passwordEchoDurationInSecondsBackup);
        if (old->m_passwordEchoEnabledBackedUp)
            settings()->setPasswordEchoEnabled(old->m_passwordEchoEnabledBackup);
    }
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

void InternalSettings::setEnableScrollAnimator(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
#if ENABLE(SMOOTH_SCROLLING)
    settings()->setEnableScrollAnimator(enabled);
#else
    UNUSED_PARAM(enabled);
#endif
}

void InternalSettings::setZoomAnimatorTransform(float scale, float tx, float ty, ExceptionCode& ec)
{
    InternalSettingsGuardForFrame();

#if ENABLE(GESTURE_EVENTS)
    PlatformGestureEvent pge(PlatformEvent::GestureDoubleTap, IntPoint(tx, ty), IntPoint(tx, ty), 0, scale, 0.f, 0, 0, 0, 0);
    frame()->eventHandler()->handleGestureEvent(pge);
#else
    UNUSED_PARAM(scale);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
#endif
}

void InternalSettings::setZoomParameters(float scale, float x, float y, ExceptionCode& ec)
{
    InternalSettingsGuardForFrameView();

#if ENABLE(SMOOTH_SCROLLING)
    frame()->view()->scrollAnimator()->setZoomParametersForTest(scale, x, y);
#else
    UNUSED_PARAM(scale);
    UNUSED_PARAM(x);
    UNUSED_PARAM(y);
#endif
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setPasswordEchoEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    if (!m_passwordEchoEnabledBackedUp) {
        m_passwordEchoEnabledBackup = settings()->passwordEchoEnabled();
        m_passwordEchoEnabledBackedUp = true;
    }
    settings()->setPasswordEchoEnabled(enabled);
}

void InternalSettings::setPasswordEchoDurationInSeconds(double durationInSeconds, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    if (!m_passwordEchoDurationInSecondsBackedUp) {
        m_passwordEchoDurationInSecondsBackup = settings()->passwordEchoDurationInSeconds();
        m_passwordEchoDurationInSecondsBackedUp = true;
    }
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

float InternalSettings::pageScaleFactor(ExceptionCode& ec)
{
    InternalSettingsGuardForPageReturn(0);
    return page()->pageScaleFactor();
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

}
