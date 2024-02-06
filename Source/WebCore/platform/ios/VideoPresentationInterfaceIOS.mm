/*
 * Copyright (C) 2014-2023 Apple Inc. All rights reserved.
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

#import "config.h"
#import "VideoPresentationInterfaceIOS.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "PictureInPictureSupport.h"
#import "RuntimeApplicationChecks.h"
#import "TimeRanges.h"
#import "UIViewControllerUtilities.h"
#import <UIKit/UIKit.h>
#import <UIKit/UIWindow.h>
#import <objc/message.h>
#import <objc/runtime.h>
#import <pal/spi/ios/UIKitSPI.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;

#if PLATFORM(WATCHOS) || PLATFORM(APPLETV)
static UIViewController *fallbackViewController(UIView *view)
{
    // FIXME: This logic to find a fallback view controller should move out of WebCore,
    // and into the client layer.
    for (UIView *currentView = view; currentView; currentView = currentView.superview) {
        if (auto controller = viewController(currentView)) {
            if (!controller.parentViewController)
                return controller;
        }
    }

    LOG_ERROR("Failed to find a view controller suitable to present fullscreen video");
    return nil;
}

UIViewController *VideoPresentationInterfaceIOS::presentingViewController()
{
    auto model = videoPresentationModel();
    auto *controller = model ? model->presentingViewController() : nil;
    if (!controller)
        controller = fallbackViewController(m_parentView.get());

    return controller;
}
#endif

VideoPresentationInterfaceIOS::VideoPresentationInterfaceIOS(PlaybackSessionInterfaceIOS& playbackSessionInterface)
    : m_watchdogTimer(RunLoop::main(), this, &VideoPresentationInterfaceIOS::watchdogTimerFired)
    , m_playbackSessionInterface(playbackSessionInterface)
{

}

VideoPresentationInterfaceIOS::~VideoPresentationInterfaceIOS()
{
    if (auto model = videoPresentationModel())
        model->removeClient(*this);
}

WebAVPlayerController *VideoPresentationInterfaceIOS::playerController() const
{
    return m_playbackSessionInterface->playerController();
}

void VideoPresentationInterfaceIOS::applicationDidBecomeActive()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::applicationDidBecomeActive(%p)", this);
}

void VideoPresentationInterfaceIOS::enterFullscreen()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::enterFullscreen(%p) %d", this, mode());

    doEnterFullscreen();
}

void VideoPresentationInterfaceIOS::exitFullscreenWithoutAnimationToMode(HTMLMediaElementEnums::VideoFullscreenMode mode)
{
    ASSERT_UNUSED(mode, mode == HTMLMediaElementEnums::VideoFullscreenModeNone);
    m_watchdogTimer.stop();
    m_targetMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    m_currentMode = HTMLMediaElementEnums::VideoFullscreenModeNone;
    cleanupFullscreen();
}

void VideoPresentationInterfaceIOS::invalidate()
{
    m_videoPresentationModel = nullptr;

    m_watchdogTimer.stop();
    m_enteringPictureInPicture = false;
    cleanupFullscreen();
}

void VideoPresentationInterfaceIOS::preparedToExitFullscreen()
{
#if PLATFORM(WATCHOS)
    if (!m_waitingForPreparedToExit)
        return;

    m_waitingForPreparedToExit = false;
    auto model = videoPresentationModel();
    if (model)
        model->requestFullscreenMode(HTMLMediaElementEnums::VideoFullscreenModeNone, true);
#endif
}

void VideoPresentationInterfaceIOS::prepareForPictureInPictureStop(WTF::Function<void(bool)>&& callback)
{
    m_prepareToInlineCallback = WTFMove(callback);
    if (auto model = videoPresentationModel())
        model->fullscreenMayReturnToInline();
}

void VideoPresentationInterfaceIOS::willStopPictureInPicture()
{
    LOG(Fullscreen, "VideoPresentationInterfaceIOS::willStopPictureInPicture(%p)", this);

    m_exitingPictureInPicture = true;
    m_shouldReturnToFullscreenWhenStoppingPictureInPicture = false;

    if (m_currentMode.hasFullscreen())
        return;

    if (auto model = videoPresentationModel())
        model->willExitPictureInPicture();
}

void VideoPresentationInterfaceIOS::setHasVideoContentLayer(bool value)
{
    m_hasVideoContentLayer = value;

    if (m_hasVideoContentLayer && m_finalizeSetupNeedsVideoContentLayer)
        finalizeSetup();
    if (!m_hasVideoContentLayer && m_cleanupNeedsReturnVideoContentLayer)
        cleanupFullscreen();
    if (!m_hasVideoContentLayer && m_finalizeSetupNeedsReturnVideoContentLayer && !m_returningToStandby)
        finalizeSetup();
    if (!m_hasVideoContentLayer && m_returningToStandby)
        returnToStandby();
    if (!m_hasVideoContentLayer && m_exitFullscreenNeedsReturnContentLayer)
        doExitFullscreen();
}

void VideoPresentationInterfaceIOS::preparedToReturnToStandby()
{
    if (!m_returningToStandby)
        return;

    returnToStandby();
}

void VideoPresentationInterfaceIOS::finalizeSetup()
{
    RunLoop::main().dispatch([protectedThis = Ref { *this }, this] {
        if (auto model = videoPresentationModel()) {
            if (!m_hasVideoContentLayer && m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsVideoContentLayer = true;
                model->requestVideoContentLayer();
                return;
            }
            m_finalizeSetupNeedsVideoContentLayer = false;
            if (m_hasVideoContentLayer && !m_targetMode.hasVideo()) {
                m_finalizeSetupNeedsReturnVideoContentLayer = true;
                model->returnVideoContentLayer();
                return;
            }
            m_finalizeSetupNeedsReturnVideoContentLayer = false;
            model->didSetupFullscreen();
        }
    });
}

void VideoPresentationInterfaceIOS::returnToStandby()
{
    ALWAYS_LOG_IF_POSSIBLE(LOGIDENTIFIER);
    m_returningToStandby = false;

    auto model = videoPresentationModel();
    if (model)
        model->returnVideoView();

    // Continue processing exit picture-in-picture now that
    // it is safe to do so:
    didStopPictureInPicture();
}

void VideoPresentationInterfaceIOS::setMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.setMode(mode);
    // Mode::mode() can be 3 (VideoFullscreenModeStandard | VideoFullscreenModePictureInPicture).
    // HTMLVideoElement does not expect such a value in the fullscreenModeChanged() callback.
    auto model = videoPresentationModel();
    if (model && shouldNotifyModel)
        model->fullscreenModeChanged(mode);
}

void VideoPresentationInterfaceIOS::clearMode(HTMLMediaElementEnums::VideoFullscreenMode mode, bool shouldNotifyModel)
{
    if ((~m_currentMode.mode() & mode) == mode)
        return;

    m_currentMode.clearMode(mode);
    auto model = videoPresentationModel();
    if (model && shouldNotifyModel)
        model->fullscreenModeChanged(m_currentMode.mode());
}

#if !RELEASE_LOG_DISABLED
const void* VideoPresentationInterfaceIOS::logIdentifier() const
{
    return m_playbackSessionInterface->logIdentifier();
}

const Logger* VideoPresentationInterfaceIOS::loggerPtr() const
{
    return m_playbackSessionInterface->loggerPtr();
}

WTFLogChannel& VideoPresentationInterfaceIOS::logChannel() const
{
    return LogFullscreen;
}
#endif
#endif // PLATFORM(IOS_FAMILY)
