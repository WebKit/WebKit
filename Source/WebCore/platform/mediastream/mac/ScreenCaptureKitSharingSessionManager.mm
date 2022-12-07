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
#import <pal/spi/mac/ScreenCaptureKitSPI.h>
#import <wtf/cocoa/Entitlements.h>

#import <pal/mac/ScreenCaptureKitSoftLink.h>

using namespace WebCore;

@interface WebDisplayMediaPromptHelper : NSObject <SCContentSharingSessionProtocol
#if HAVE(SC_CONTENT_PICKER)
    , SCContentPickerDelegate
#endif
    > {
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
#if HAVE(SC_CONTENT_PICKER)
- (void)contentPicker:(SCContentPicker *)contentPicker didFinishWithContentFilter:(SCContentFilter *)filter;
#endif
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

#if HAVE(SC_CONTENT_PICKER)
- (void)contentPicker:(SCContentPicker *)contentPicker didFinishWithContentFilter:(SCContentFilter *)filter
{
    RunLoop::main().dispatch([self, protectedSelf = RetainPtr { self }, contentPicker = RetainPtr { contentPicker }, filter = RetainPtr { filter }]() mutable {
        if (_callback)
            _callback->contentPickerDidFinish(contentPicker, filter);
    });
}
#endif

@end

namespace WebCore {

bool ScreenCaptureKitSharingSessionManager::isAvailable()
{
    return PAL::getSCContentSharingSessionClass();
}

bool ScreenCaptureKitSharingSessionManager::useSCContentPicker()
{
#if HAVE(SC_CONTENT_PICKER)
    return PAL::getSCContentPickerClass();
#else
    return false;
#endif
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
    cancelSharingSession(m_pendingCaptureSession);

#if HAVE(SC_CONTENT_PICKER)
    m_contentPicker = nullptr;
#endif

    if (m_promptHelper) {
        [m_promptHelper disconnect];
        m_promptHelper = nullptr;
    }
}

void ScreenCaptureKitSharingSessionManager::cancelSharingSession(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());

    m_promptWatchdogTimer = nullptr;

    if (session) {
        [m_promptHelper stopObservingSession:session.get()];
        [session end];
        if (m_pendingCaptureSession == session)
            m_pendingCaptureSession = nullptr;
    }

    if (!m_completionHandler)
        return;

    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    completionHandler(std::nullopt);
}


void ScreenCaptureKitSharingSessionManager::pickerCanceledForSession(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());
    ASSERT(m_completionHandler);
    ASSERT(m_pendingCaptureSession == session);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::pickerCanceledForSession");

    cancelSharingSession(session);
}

void ScreenCaptureKitSharingSessionManager::sessionDidEnd(RetainPtr<SCContentSharingSession> session)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::sessionDidEnd");

    [m_promptHelper stopObservingSession:session.get()];

    if (m_pendingCaptureSession == session)
        m_pendingCaptureSession = nullptr;
}

void ScreenCaptureKitSharingSessionManager::completeDeviceSelection(SCContentFilter* contentFilter)
{
    ASSERT(m_completionHandler);
    ASSERT(contentFilter);
    if (!m_completionHandler || !contentFilter)
        return;

    std::optional<CaptureDevice> device;
    switch (contentFilter.type) {
    case SCContentFilterTypeDesktopIndependentWindow: {
        auto *window = contentFilter.desktopIndependentWindowInfo.window;
        device = CaptureDevice(String::number(window.windowID), CaptureDevice::DeviceType::Window, window.title, emptyString(), true);
        break;
    }
    case SCContentFilterTypeDisplay:
        device = CaptureDevice(String::number(contentFilter.displayInfo.display.displayID), CaptureDevice::DeviceType::Screen, makeString("Screen "), emptyString(), true);
        break;
    case SCContentFilterTypeNothing:
    case SCContentFilterTypeAppsAndWindowsPinnedToDisplay:
    case SCContentFilterTypeClientShouldImplementDefault:
        ASSERT_NOT_REACHED();
        return;
    }

    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    completionHandler(device);
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

    if (m_pendingCaptureSession == session)
        completeDeviceSelection([session content]);
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

    if (!m_promptHelper)
        m_promptHelper = adoptNS([[WebDisplayMediaPromptHelper alloc] initWithCallback:this]);

    m_completionHandler = WTFMove(completionHandler);

    std::optional<ContentPicker> picker;
    if (useSCContentPicker())
        picker = promptWithSCContentPicker(promptType);
    else
        picker = promptWithSCContentSharingSession(promptType);

    if (!picker) {
        m_completionHandler(std::nullopt);
        m_completionHandler = { };
        return;
    }

    constexpr Seconds userPromptWatchdogInterval = 60_s;
    m_promptWatchdogTimer = makeUnique<RunLoop::Timer<ScreenCaptureKitSharingSessionManager>>(RunLoop::main(), [this, weakThis = WeakPtr { *this }, picker = WTFMove(picker), interval = userPromptWatchdogInterval]() mutable {
        if (!weakThis)
            return;

        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia nothing picked after %f seconds, cancelling.", interval.value());

        switchOn(picker.value(),
            [this] (const RetainPtr<SCContentPicker> contentPicker) {
                contentPickerDidFinish(contentPicker, nullptr);
            },
            [this] (const RetainPtr<SCContentSharingSession> session) {
                pickerCanceledForSession(session);
            }
        );
    });
    m_promptWatchdogTimer->startOneShot(userPromptWatchdogInterval);
}

std::optional<ScreenCaptureKitSharingSessionManager::ContentPicker> ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingSession(PromptType promptType)
{
    SCContentSharingSession* session = [[PAL::getSCContentSharingSessionClass() alloc] initWithTitle:@"WebKit getDisplayMedia Prompt"];
    if (!session) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingSession unable to create sharing session");
        return std::nullopt;
    }

    m_pendingCaptureSession = session;
    [m_promptHelper startObservingSession:session];

    [session showPickerForType:promptType == PromptType::Window ? SCContentFilterTypeDesktopIndependentWindow : SCContentFilterTypeDisplay];

    return session;
}

std::optional<ScreenCaptureKitSharingSessionManager::ContentPicker> ScreenCaptureKitSharingSessionManager::promptWithSCContentPicker(PromptType promptType)
{
#if HAVE(SC_CONTENT_PICKER)
    ASSERT(useSCContentPicker());

    auto contentPickerType = promptType == PromptType::Window ? SCContentPickerTypeDesktopIndependentWindow : SCContentPickerTypeDisplay;
    SCContentPicker* picker = [[PAL::getSCContentPickerClass() alloc] initWithType:contentPickerType delegate:m_promptHelper.get()];
    if (!picker) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptWithSCContentPicker unable to create content picker");
        return { };
    }

    m_contentPicker = picker;

    [picker showContentPicker];

    return picker;
#else
    UNUSED_PARAM(promptType);
    return { };
#endif
}

void ScreenCaptureKitSharingSessionManager::contentPickerDidFinish(RetainPtr<SCContentPicker> picker, RetainPtr<SCContentFilter> contentFilter)
{
#if HAVE(SC_CONTENT_PICKER)
    ASSERT(isMainThread());
    ASSERT([m_contentPicker isEqual:picker.get()]);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::contentPickerDidFinish");

    m_promptWatchdogTimer = nullptr;
    if (![m_contentPicker isEqual:picker.get()])
        return;

    m_contentFilter = contentFilter;

    if (!contentFilter) {
        if (m_completionHandler)
            m_completionHandler(std::nullopt);
        m_completionHandler = { };

        return;
    }

    completeDeviceSelection(contentFilter.get());
#else
    UNUSED_PARAM(picker);
    UNUSED_PARAM(contentFilter);
#endif
}

static bool operator==(const SCContentFilter* filter, const CaptureDevice& device)
{
    auto deviceID = parseInteger<uint32_t>(device.persistentId());
    if (!deviceID)
        return false;

    if (device.type() == CaptureDevice::DeviceType::Screen) {
        if (filter.type != SCContentFilterTypeDisplay)
            return false;

        if (filter.displayInfo.display.displayID != deviceID)
            return false;

        return true;
    }

    if (device.type() == CaptureDevice::DeviceType::Window) {
        if (filter.type != SCContentFilterTypeDesktopIndependentWindow)
            return false;

        if (filter.desktopIndependentWindowInfo.window.windowID != deviceID)
            return false;

        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

RetainPtr<SCContentFilter> ScreenCaptureKitSharingSessionManager::takeContentFilterForCaptureDevice(const CaptureDevice& device)
{
#if HAVE(SC_CONTENT_PICKER)
    if (!useSCContentPicker())
        return nullptr;

    ASSERT(isMainThread());
    ASSERT(m_contentFilter);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::takeContentFilterForCaptureDevice");

    if (!m_contentFilter)
        return nullptr;

    if (m_contentFilter.get() != device)
        return nullptr;

    auto contentFilter = std::exchange(m_contentFilter, nullptr);
    return contentFilter;

#else
    UNUSED_PARAM(device);
    return nullptr;
#endif
}

RetainPtr<SCContentSharingSession> ScreenCaptureKitSharingSessionManager::takeSharingSessionForCaptureDevice(const CaptureDevice& device)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::takeSharingSessionForFilter");

    ASSERT(device == [m_pendingCaptureSession content]);
    if (device != [m_pendingCaptureSession content]) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::takeSharingSessionForFilter sharing session not found");
        return nullptr;
    }

    auto session = std::exchange(m_pendingCaptureSession, nullptr);
    [m_promptHelper stopObservingSession:session.get()];

    return session;
}

} // namespace WebCore

#endif // HAVE(SC_CONTENT_SHARING_SESSION)
