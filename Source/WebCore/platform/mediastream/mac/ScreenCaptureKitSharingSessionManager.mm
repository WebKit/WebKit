/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT)

#import "CaptureDevice.h"
#import "Logging.h"
#import "PlatformMediaSessionManager.h"
#import "ScreenCaptureKitCaptureSource.h"
#import <pal/spi/mac/ScreenCaptureKitSPI.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/StringToIntegerConversion.h>

#if HAVE(SC_CONTENT_SHARING_PICKER)
#import <ScreenCaptureKit/SCContentSharingPicker.h>
#endif

#import <pal/mac/ScreenCaptureKitSoftLink.h>

// FIXME: Remove this once it is in a public header.

#if HAVE(SC_CONTENT_SHARING_PICKER)
@interface SCContentSharingPicker (SCContentSharingPicker_Pending_Public_API)
- (void)presentPickerUsingContentStyle:(SCShareableContentStyle)contentStyle;
@end
#endif

@interface WebDisplayMediaPromptHelper : NSObject <SCContentSharingSessionProtocol
#if HAVE(SC_CONTENT_SHARING_PICKER)
    , SCContentSharingPickerObserver
#endif
    > {
    WeakPtr<WebCore::ScreenCaptureKitSharingSessionManager> _callback;
    Vector<RetainPtr<SCContentSharingSession>> _sessions;
    BOOL _observingPicker;
}

- (instancetype)initWithCallback:(WebCore::ScreenCaptureKitSharingSessionManager*)callback;
- (void)disconnect;
- (BOOL)hasObservingSession;
- (void)startObservingSession:(SCContentSharingSession *)session;
- (void)stopObservingSession:(SCContentSharingSession *)session;
- (void)sessionDidEnd:(SCContentSharingSession *)session;
- (void)sessionDidChangeContent:(SCContentSharingSession *)session;
- (void)pickerCanceledForSession:(SCContentSharingSession *)session;
#if HAVE(SC_CONTENT_SHARING_PICKER)
- (void)startObservingPicker:(SCContentSharingPicker *)session;
- (void)stopObservingPicker:(SCContentSharingPicker *)session;
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didUpdateWithFilter:(SCContentFilter *)filter forStream:(SCStream *)stream;
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(SCStream *)stream;
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error;
#endif
@end

@implementation WebDisplayMediaPromptHelper
- (instancetype)initWithCallback:(WebCore::ScreenCaptureKitSharingSessionManager*)callback
{
    self = [super init];
    if (self) {
        _callback = WeakPtr { callback };
        _observingPicker = NO;
    }

    return self;
}

- (void)disconnect
{
    _callback = nullptr;
    for (auto& session : _sessions)
        [session setDelegate:nil];
    _sessions.clear();
}

- (BOOL)hasObservingSession
{
    return _sessions.isEmpty();
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

    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->sharingSessionDidEnd(session.get());
    });
}

- (void)sessionDidChangeContent:(SCContentSharingSession *)session
{
    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->sharingSessionDidChangeContent(session.get());
    });
}

- (void)pickerCanceledForSession:(SCContentSharingSession *)session
{
    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }, session = RetainPtr { session }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->cancelPicking();
    });
}

#if HAVE(SC_CONTENT_SHARING_PICKER)
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(SCStream *)stream
{
    UNUSED_PARAM(picker);
    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->cancelPicking();
    });
}

- (void)contentSharingPickerStartDidFailWithError:(NSError *)error
{
    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }, error = RetainPtr { error }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->contentSharingPickerFailedWithError(error.get());
    });
}

- (void)contentSharingPicker:(SCContentSharingPicker *)picker didUpdateWithFilter:(SCContentFilter *)filter forStream:(SCStream *)stream {
    UNUSED_PARAM(picker);
    RunLoop::mainSingleton().dispatch([self, protectedSelf = RetainPtr { self }, filter = RetainPtr { filter }, stream = RetainPtr { stream }]() mutable {
        if (RefPtr callback = _callback.get())
            callback->contentSharingPickerUpdatedFilterForStream(filter.get(), stream.get());
    });
}

- (void)startObservingPicker:(SCContentSharingPicker *)picker
{
    if (_observingPicker)
        return;

    _observingPicker = YES;
    [picker addObserver:self];
}

- (void)stopObservingPicker:(SCContentSharingPicker *)picker
{
    if (!_observingPicker)
        return;

    _observingPicker = NO;
    [picker removeObserver:self];
}
#endif

@end

namespace WebCore {

bool ScreenCaptureKitSharingSessionManager::isAvailable()
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    if (PAL::getSCContentSharingPickerClass() && PAL::getSCContentSharingPickerConfigurationClass())
        return true;
#endif

    return PAL::getSCContentSharingSessionClass();
}

bool ScreenCaptureKitSharingSessionManager::useSCContentSharingPicker()
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    return isAvailable();
#else
    return false;
#endif
}

ScreenCaptureKitSharingSessionManager& ScreenCaptureKitSharingSessionManager::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<Ref<ScreenCaptureKitSharingSessionManager>> manager = adoptRef(*new ScreenCaptureKitSharingSessionManager);
    return manager.get().get();
}

ScreenCaptureKitSharingSessionManager::ScreenCaptureKitSharingSessionManager()
{
}

ScreenCaptureKitSharingSessionManager::~ScreenCaptureKitSharingSessionManager()
{
    m_activeSources.clear();
    cancelPicking();

    if (m_promptHelper) {
        [m_promptHelper disconnect];
        m_promptHelper = nullptr;
    }
}

void ScreenCaptureKitSharingSessionManager::cancelGetDisplayMediaPrompt()
{
    if (promptingInProgress())
        cancelPicking();
}

void ScreenCaptureKitSharingSessionManager::cancelPicking()
{
    ASSERT(isMainThread());

    m_promptWatchdogTimer = nullptr;

    if (m_pendingSession) {
        [m_promptHelper stopObservingSession:m_pendingSession.get()];
        [m_pendingSession end];
        m_pendingSession = nullptr;
    };
#if HAVE(SC_CONTENT_SHARING_PICKER)
    if (useSCContentSharingPicker()) {
        RetainPtr picker = [PAL::getSCContentSharingPickerClass() sharedPicker];
        if (![m_promptHelper hasObservingSession])
            [picker setActive:NO];
        if (m_activeSources.isEmpty())
            [m_promptHelper stopObservingPicker:picker.get()];
    }
#endif

    if (!m_completionHandler)
        return;

    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    completionHandler(std::nullopt);
}

void ScreenCaptureKitSharingSessionManager::completeDeviceSelection(SCContentFilter* contentFilter, SCContentSharingSession* sharingSession)
{
    m_promptWatchdogTimer = nullptr;

    if (!contentFilter || !m_completionHandler) {
        if (m_completionHandler)
            m_completionHandler(std::nullopt);
        return;
    }

    m_pendingSession = sharingSession;
    m_pendingContentFilter = contentFilter;

    std::optional<CaptureDevice> device;
    switch (contentFilter.type) {
    case SCContentFilterTypeDesktopIndependentWindow: {
        RetainPtr window = retainPtr(contentFilter.desktopIndependentWindowInfo.window);
        device = CaptureDevice(String::number([window windowID]), CaptureDevice::DeviceType::Window, [window title], emptyString(), true);
        break;
    }
    case SCContentFilterTypeDisplay:
        device = CaptureDevice(String::number(contentFilter.displayInfo.display.displayID), CaptureDevice::DeviceType::Screen, "Screen "_str, emptyString(), true);
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

WeakPtr<ScreenCaptureSessionSource> ScreenCaptureKitSharingSessionManager::findActiveSource(SCContentSharingSession* sharingSession)
{
    auto index = m_activeSources.findIf([sharingSession](auto activeSource) {
        return activeSource && [sharingSession isEqual:activeSource->sharingSession()];
    });
    if (index == notFound) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::sharingSessionDidEnd - unknown session!");
        return nullptr;
    }

    return m_activeSources[index];
}

void ScreenCaptureKitSharingSessionManager::sharingSessionDidEnd(SCContentSharingSession* sharingSession)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::sharingSessionDidEnd");
    auto activeSource = findActiveSource(sharingSession);
    if (!activeSource) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::sharingSessionDidEnd - unknown session!");
        return;
    }

    if (!activeSource->observer()) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::sharingSessionDidEnd - null session observer!");
        cleanupSessionSource(*activeSource);
        return;
    }

    activeSource->streamDidEnd();
}

void ScreenCaptureKitSharingSessionManager::sharingSessionDidChangeContent(SCContentSharingSession* sharingSession)
{
    ASSERT(isMainThread());

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::sharingSessionDidChangeContent");

    if (sharingSession.content.type == SCContentFilterTypeNothing) {
        sharingSessionDidEnd(sharingSession);
        return;
    }

    auto activeSource = findActiveSource(sharingSession);
    if (!activeSource) {
        completeDeviceSelection(sharingSession.content, sharingSession);
        return;
    }

    activeSource->updateContentFilter(sharingSession.content);
    activeSource->observer()->sessionFilterDidChange(sharingSession.content);
}

void ScreenCaptureKitSharingSessionManager::contentSharingPickerSelectedFilterForStream(SCContentFilter* contentFilter, SCStream*)
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::contentSharingPickerSelectedFilterForStream");

    completeDeviceSelection(contentFilter);
#else
    UNUSED_PARAM(contentFilter);
#endif
}

void ScreenCaptureKitSharingSessionManager::contentSharingPickerUpdatedFilterForStream(SCContentFilter* contentFilter, SCStream* stream)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::contentSharingPickerUpdatedFilterForStream");

    auto index = m_activeSources.findIf([stream](auto activeSource) {
        return activeSource && [stream isEqual:activeSource->stream()];
    });
    if (index == notFound) {
        if (stream) {
            RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::contentFilterWasUpdated - unexpected stream!");
            return;
        }
        completeDeviceSelection(contentFilter);
        return;
    }

    auto activeSource = m_activeSources[index];
    if (!activeSource->observer()) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::contentFilterWasUpdated - null session observer!");
        m_activeSources.removeAt(index);
        return;
    }

    activeSource->updateContentFilter(contentFilter);
}

void ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia(DisplayCapturePromptType promptType, CompletionHandler<void(std::optional<CaptureDevice>)>&& completionHandler)
{
    ASSERT(isAvailable());
    ASSERT(!m_completionHandler);

    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia - %s", promptType == DisplayCapturePromptType::Window ? "Window" : "Screen");

    if (!isAvailable()) {
        completionHandler(std::nullopt);
        return;
    }

    if (!m_promptHelper)
        m_promptHelper = adoptNS([[WebDisplayMediaPromptHelper alloc] initWithCallback:this]);

    m_completionHandler = WTFMove(completionHandler);

    bool showingPicker = useSCContentSharingPicker() ? promptWithSCContentSharingPicker(promptType) : promptWithSCContentSharingSession(promptType);
    if (!showingPicker) {
        m_completionHandler(std::nullopt);
        return;
    }

    constexpr Seconds userPromptWatchdogInterval = 60_s;
    m_promptWatchdogTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), [weakThis = WeakPtr { *this }, interval = userPromptWatchdogInterval]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia nothing picked after %f seconds, cancelling.", interval.value());

        protectedThis->cancelPicking();
    });
    m_promptWatchdogTimer->startOneShot(userPromptWatchdogInterval);
}

bool ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingSession(DisplayCapturePromptType promptType)
{
    m_pendingSession = adoptNS([PAL::allocSCContentSharingSessionInstance() initWithTitle:@"WebKit getDisplayMedia Prompt"]);
    if (!m_pendingSession) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingSession unable to create sharing session");
        return false;
    }

    [m_promptHelper startObservingSession:m_pendingSession.get()];
    [m_pendingSession showPickerForType:promptType == DisplayCapturePromptType::Window ? SCContentFilterTypeDesktopIndependentWindow : SCContentFilterTypeDisplay];

    return true;
}

bool ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingPicker(DisplayCapturePromptType promptType)
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    ASSERT(useSCContentSharingPicker());

    auto configuration = adoptNS([PAL::allocSCContentSharingPickerConfigurationInstance() init]);
    SCShareableContentStyle shareableContentStyle = SCShareableContentStyleWindow;
    switch (promptType) {
    case DisplayCapturePromptType::Window:
        [configuration setAllowedPickerModes:SCContentSharingPickerModeSingleWindow];
        shareableContentStyle = SCShareableContentStyleWindow;
        break;
    case DisplayCapturePromptType::Screen:
        [configuration setAllowedPickerModes:SCContentSharingPickerModeSingleDisplay];
        shareableContentStyle = SCShareableContentStyleDisplay;
        break;
    case DisplayCapturePromptType::UserChoose:
        [configuration setAllowedPickerModes:SCContentSharingPickerModeSingleWindow | SCContentSharingPickerModeSingleDisplay];
        shareableContentStyle = SCShareableContentStyleNone;
        break;
    }

    RetainPtr picker = [PAL::getSCContentSharingPickerClass() sharedPicker];
    [picker setDefaultConfiguration:configuration.get()];
    [picker setMaximumStreamCount:@(std::numeric_limits<unsigned>::max())];
    [picker setActive:YES];
    [m_promptHelper startObservingPicker:picker.get()];

    if (shareableContentStyle != SCShareableContentStyleNone && [picker respondsToSelector:@selector(presentPickerUsingContentStyle:)])
        [picker presentPickerUsingContentStyle:shareableContentStyle];
    else
        [picker present];

    return true;
#else
    UNUSED_PARAM(promptType);
    return false;
#endif
}

bool ScreenCaptureKitSharingSessionManager::promptingInProgress() const
{
    return !!m_completionHandler;
}

void ScreenCaptureKitSharingSessionManager::contentSharingPickerFailedWithError(NSError* error)
{
#if HAVE(SC_CONTENT_SHARING_PICKER)
    ASSERT(isMainThread());

    RELEASE_LOG_ERROR_IF(error, WebRTC, "ScreenCaptureKitSharingSessionManager::contentSharingPickerFailedWithError content sharing picker failed with error '%s'", [[error localizedDescription] UTF8String]);

    cancelPicking();
#else
    UNUSED_PARAM(error);
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

        return filter.displayInfo.display.displayID == deviceID;
    }

    if (device.type() == CaptureDevice::DeviceType::Window) {
        if (filter.type != SCContentFilterTypeDesktopIndependentWindow)
            return false;

        return filter.desktopIndependentWindowInfo.window.windowID == deviceID;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void ScreenCaptureKitSharingSessionManager::cancelPendingSessionForDevice(const CaptureDevice& device)
{
    ASSERT(isMainThread());

    if (m_pendingContentFilter.get() != device) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::cancelPendingSessionForDevice - unknown capture device.");
        return;
    }

    m_pendingContentFilter = nullptr;
    cancelPicking();
}

std::pair<RetainPtr<SCContentFilter>, RetainPtr<SCContentSharingSession>> ScreenCaptureKitSharingSessionManager::contentFilterAndSharingSessionFromCaptureDevice(const CaptureDevice& device)
{
    ASSERT(isMainThread());

#if HAVE(SC_CONTENT_SHARING_PICKER)
    if (m_pendingContentFilter.get() != device) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::contentFilterFromCaptureDevice - unknown capture device.");
        return { };
    }
#else
    UNUSED_PARAM(device);
#endif
    return std::make_pair(std::exchange(m_pendingContentFilter, { }), std::exchange(m_pendingSession, { }));
}

RefPtr<ScreenCaptureSessionSource> ScreenCaptureKitSharingSessionManager::createSessionSourceForDevice(WeakPtr<ScreenCaptureSessionSourceObserver> observer, SCContentFilter* contentFilter, SCContentSharingSession* sharingSession, SCStreamConfiguration* configuration, SCStreamDelegate* delegate)
{
    ASSERT(isMainThread());

    RetainPtr<SCStream> stream;
#if HAVE(SC_CONTENT_SHARING_PICKER)
    if (useSCContentSharingPicker())
        stream = adoptNS([PAL::allocSCStreamInstance() initWithFilter:contentFilter configuration:configuration delegate:(id<SCStreamDelegate> _Nullable)delegate]);
    else
#endif
        stream = adoptNS([PAL::allocSCStreamInstance() initWithSharingSession:sharingSession captureOutputProperties:configuration delegate:(id<SCStreamDelegate> _Nullable)delegate]);

    if (!stream) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::createSessionSourceForDevice - unable to create SCStream.");
        return nullptr;
    }

    auto cleanupFunction = [weakThis = WeakPtr { *this }](ScreenCaptureSessionSource& source) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->cleanupSessionSource(source);
    };

    auto newSession = ScreenCaptureSessionSource::create(WTFMove(observer), WTFMove(stream), contentFilter, sharingSession, WTFMove(cleanupFunction));
    m_activeSources.append(newSession);

    return newSession;
}

void ScreenCaptureKitSharingSessionManager::cleanupSessionSource(ScreenCaptureSessionSource& source)
{
    auto index = m_activeSources.findIf([&source](auto activeSource) {
        return &source == activeSource.get();
    });
    if (index == notFound) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::cleanupSessionSource - unknown source.");
        return;
    }

    m_activeSources.removeAt(index);

    if (!promptingInProgress())
        cancelPicking();
}

void ScreenCaptureKitSharingSessionManager::cleanupSharingSession(SCContentSharingSession* sharingSession)
{
    if (m_promptHelper)
        [m_promptHelper stopObservingSession:sharingSession];
    [sharingSession end];
}

Ref<ScreenCaptureSessionSource> ScreenCaptureSessionSource::create(WeakPtr<ScreenCaptureSessionSourceObserver> observer, RetainPtr<SCStream> stream, RetainPtr<SCContentFilter> filter, RetainPtr<SCContentSharingSession> sharingSession, CleanupFunction&& cleanupFunction)
{
    return adoptRef(*new ScreenCaptureSessionSource(WTFMove(observer), WTFMove(stream), WTFMove(filter), WTFMove(sharingSession), WTFMove(cleanupFunction)));
}

ScreenCaptureSessionSource::ScreenCaptureSessionSource(WeakPtr<ScreenCaptureSessionSourceObserver>&& observer, RetainPtr<SCStream>&& stream, RetainPtr<SCContentFilter>&& filter, RetainPtr<SCContentSharingSession>&& sharingSession, CleanupFunction&& cleanupFunction)
    : m_stream(WTFMove(stream))
    , m_contentFilter(WTFMove(filter))
    , m_sharingSession(WTFMove(sharingSession))
    , m_observer(WTFMove(observer))
    , m_cleanupFunction(WTFMove(cleanupFunction))
{
}

ScreenCaptureSessionSource::~ScreenCaptureSessionSource()
{
    m_cleanupFunction(*this);
}

bool ScreenCaptureSessionSource::operator==(const ScreenCaptureSessionSource& other) const
{
    if (![m_stream isEqual:other.stream()])
        return false;

    if (![m_contentFilter isEqual:other.contentFilter()])
        return false;

    if (![m_sharingSession isEqual:other.sharingSession()])
        return false;

    return m_observer == other.observer();
}

void ScreenCaptureSessionSource::updateContentFilter(SCContentFilter* contentFilter)
{
    ASSERT(m_observer);
    m_contentFilter = contentFilter;
    m_observer->sessionFilterDidChange(contentFilter);
}

void ScreenCaptureSessionSource::streamDidEnd()
{
    ASSERT(m_observer);
    m_observer->sessionStreamDidEnd(m_stream.get());
}

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
