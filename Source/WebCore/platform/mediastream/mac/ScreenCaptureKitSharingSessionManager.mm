/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ScreenCaptureKitSharingSessionManager.h"

#if HAVE(SC_CONTENT_SHARING_SESSION)

#import "Logging.h"
#import "ScreenCaptureKitCaptureSource.h"
#import <pal/spi/cocoa/FeatureFlagsSPI.h>
#import <pal/spi/mac/ScreenCaptureKitSPI.h>
#import <wtf/cocoa/Entitlements.h>

#import <pal/mac/ScreenCaptureKitSoftLink.h>

using namespace WebCore;

@interface WebDisplayMediaPromptHelper : NSObject <SCContentSharingSessionProtocol> {
    WeakPtr<ScreenCaptureKitSharingSessionManager> _callback;
    Vector<RetainPtr<SCContentSharingSession>> _sessions;
}

- (instancetype)initWithCallback:(ScreenCaptureKitSharingSessionManager*)callback;
- (void)disconnect;
- (void)startObservingSession:(SCContentSharingSession *)session;
- (void)stopObservingSession:(SCContentSharingSession *)session;
- (void)sessionDidEnd:(SCContentSharingSession *)session;
- (void)sessionDidChangeContent:(SCContentSharingSession *)session;
- (void)pickerCanceledForSession:(SCContentSharingSession *)session;
@end

@implementation WebDisplayMediaPromptHelper
- (instancetype)initWithCallback:(ScreenCaptureKitSharingSessionManager*)callback
{
    self = [super init];
    if (!self)
        return self;

    _callback = WeakPtr { callback };
    return self;
}

- (void)disconnect
{
    _callback = nullptr;
    for (auto& session : _sessions)
        [session setDelegate:nil];
    _sessions.clear();
}

- (void)startObservingSession:(SCContentSharingSession *)session
{
    ASSERT(!_sessions.contains(session));
    _sessions.append(RetainPtr { session });
    [session setDelegate:self];
}

- (void)stopObservingSession:(SCContentSharingSession *)session
{
    auto index = _sessions.find(RetainPtr { session });
    if (index == notFound)
        return;

    [session setDelegate:nil];
    _sessions.removeAll(session);
}

- (void)sessionDidEnd:(SCContentSharingSession *)session
{
    auto index = _sessions.find(RetainPtr { session });
    if (index == notFound)
        return;

    RunLoop::main().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (_callback)
            _callback->sessionDidEnd(session);
    });
}

- (void)sessionDidChangeContent:(SCContentSharingSession *)session
{
    RunLoop::main().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (_callback)
            _callback->sessionDidChangeContent(session);
    });
}

- (void)pickerCanceledForSession:(SCContentSharingSession *)session
{
    RunLoop::main().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (_callback)
            _callback->pickerCanceledForSession(session);
    });
}
@end

namespace WebCore {

static bool screenCaptureKitPickerFeatureEnabled()
{
    static bool enabled;
    static std::once_flag flag;
    std::call_once(flag, [] {
        enabled = os_feature_enabled(ScreenCaptureKit, expanseAdoption);
    });
    return enabled;
}

bool ScreenCaptureKitSharingSessionManager::isAvailable()
{
    return screenCaptureKitPickerFeatureEnabled()
        && PAL::getSCContentSharingSessionClass();
}

ScreenCaptureKitSharingSessionManager& ScreenCaptureKitSharingSessionManager::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<ScreenCaptureKitSharingSessionManager> manager;
    return manager;
}

ScreenCaptureKitSharingSessionManager::ScreenCaptureKitSharingSessionManager()
{
}

ScreenCaptureKitSharingSessionManager::~ScreenCaptureKitSharingSessionManager()
{
    if (m_promptHelper) {
        [m_promptHelper disconnect];
        m_promptHelper = nullptr;
    }

    for (auto session : m_pendingCaptureSessions) {
        [m_promptHelper stopObservingSession:session.get()];
        [session end];
    }
    m_pendingCaptureSessions.clear();
}

void ScreenCaptureKitSharingSessionManager::pickerCanceledForSession(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());
    ASSERT(m_completionHandler);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::pickerCanceledForSession");

    m_promptWatchdogTimer = nullptr;
    if (!m_completionHandler)
        return;

    auto index = m_pendingCaptureSessions.findIf([session](auto pendingSession) {
        return [pendingSession isEqual:session.get()];
    });
    if (index != notFound)
        m_pendingCaptureSessions.remove(index);

    [m_promptHelper stopObservingSession:session.get()];
    [session end];

    auto completionHandler = std::exchange(m_completionHandler, { });
    completionHandler(std::nullopt);
}

void ScreenCaptureKitSharingSessionManager::sessionDidEnd(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::sessionDidEnd");

    [m_promptHelper stopObservingSession:session.get()];

    auto index = m_pendingCaptureSessions.findIf([session](auto pendingSession) {
        return [pendingSession isEqual:session.get()];
    });
    if (index == notFound)
        return;

    m_pendingCaptureSessions.remove(index);
}

void ScreenCaptureKitSharingSessionManager::sessionDidChangeContent(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());

    m_promptWatchdogTimer = nullptr;

    if ([session content].type == SCContentFilterTypeNothing) {
        sessionDidEnd(session);
        return;
    }

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::sessionDidChangeContent");

    auto index = m_pendingCaptureSessions.findIf([session](auto pendingSession) {
        return [pendingSession isEqual:session.get()];
    });
    if (index == notFound)
        return;

    ASSERT(m_completionHandler);
    if (!m_completionHandler)
        return;

    std::optional<CaptureDevice> device;
    SCContentFilter* content = [session content];
    switch (content.type) {
    case SCContentFilterTypeDesktopIndependentWindow: {
        auto *window = content.desktopIndependentWindowInfo.window;
        device = CaptureDevice(String::number(window.windowID), CaptureDevice::DeviceType::Window, window.title, emptyString(), true);
        break;
    }
    case SCContentFilterTypeDisplay:
        device = CaptureDevice(String::number(content.displayInfo.display.displayID), CaptureDevice::DeviceType::Screen, makeString("Screen "), emptyString(), true);
        break;
    case SCContentFilterTypeNothing:
    case SCContentFilterTypeAppsAndWindowsPinnedToDisplay:
    case SCContentFilterTypeClientShouldImplementDefault:
        ASSERT_NOT_REACHED();
        return;
    }

    auto completionHandler = std::exchange(m_completionHandler, { });
    completionHandler(device);
}

void ScreenCaptureKitSharingSessionManager::showWindowPicker(CompletionHandler<void(std::optional<CaptureDevice>)>&& completionHandler)
{
    promptForGetDisplayMedia(PromptType::Window, WTFMove(completionHandler));
}

void ScreenCaptureKitSharingSessionManager::showScreenPicker(CompletionHandler<void(std::optional<CaptureDevice>)>&& completionHandler)
{
    promptForGetDisplayMedia(PromptType::Screen, WTFMove(completionHandler));
}

void ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia(PromptType promptType, CompletionHandler<void(std::optional<CaptureDevice>)>&& completionHandler)
{
    ASSERT(isAvailable());
    ASSERT(!m_completionHandler);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia - %s", promptType == PromptType::Window ? "Window" : "Screen");

    if (!isAvailable()) {
        completionHandler(std::nullopt);
        return;
    }

    SCContentSharingSession* session = [[PAL::getSCContentSharingSessionClass() alloc] initWithTitle:@"WebKit getDisplayMedia Prompt"];
    if (!session) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia unable to create sharing session");
        completionHandler(std::nullopt);
        return;
    }

    if (!m_promptHelper)
        m_promptHelper = adoptNS([[WebDisplayMediaPromptHelper alloc] initWithCallback:this]);

    [m_promptHelper startObservingSession:session];
    m_pendingCaptureSessions.append(session);
    m_completionHandler = WTFMove(completionHandler);

    [session showPickerForType:promptType == PromptType::Window ? SCContentFilterTypeDesktopIndependentWindow : SCContentFilterTypeDisplay];

    constexpr Seconds userPromptWatchdogInterval = 60_s;
    m_promptWatchdogTimer = makeUnique<RunLoop::Timer>(RunLoop::main(), [this, weakThis = WeakPtr { *this }, session = RetainPtr { session }, interval = userPromptWatchdogInterval]() mutable {
        if (!weakThis)
            return;

        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia nothing picked after %f seconds, cancelling.", interval.value());
        pickerCanceledForSession(session);
    });
    m_promptWatchdogTimer->startOneShot(userPromptWatchdogInterval);
}

RetainPtr<SCContentSharingSession> ScreenCaptureKitSharingSessionManager::takeSharingSessionForFilter(SCContentFilter* filter)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::takeSharingSessionForFilter");

    auto index = m_pendingCaptureSessions.findIf([filter](auto pendingSession) {
        return [filter isEqual:[pendingSession content]];
    });
    ASSERT(index != notFound);
    if (index == notFound) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::takeSharingSessionForFilter sharing session not found");
        return nullptr;
    }

    RetainPtr<SCContentSharingSession> session = m_pendingCaptureSessions[index];
    m_pendingCaptureSessions.remove(index);
    [m_promptHelper stopObservingSession:session.get()];

    return session;
}


} // namespace WebCore

#endif // HAVE(SC_CONTENT_SHARING_SESSION)
