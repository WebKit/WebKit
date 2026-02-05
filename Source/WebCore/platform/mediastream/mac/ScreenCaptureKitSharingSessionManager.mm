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
#import <ScreenCaptureKit/SCContentSharingPicker.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/text/StringToIntegerConversion.h>

#import <pal/mac/ScreenCaptureKitSoftLink.h>

@interface WebDisplayMediaPromptHelper : NSObject <SCContentSharingPickerObserver> {
    WeakPtr<WebCore::ScreenCaptureKitSharingSessionManager> _callback;
    BOOL _observingPicker;
}

- (instancetype)initWithCallback:(WebCore::ScreenCaptureKitSharingSessionManager*)callback;
- (void)disconnect;
- (void)startObservingPicker:(SCContentSharingPicker *)session;
- (void)stopObservingPicker:(SCContentSharingPicker *)session;
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didUpdateWithFilter:(SCContentFilter *)filter forStream:(SCStream *)stream;
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(SCStream *)stream;
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error;
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
}

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

    [picker setActive:YES];

    _observingPicker = YES;
    [picker addObserver:self];
}

- (void)stopObservingPicker:(SCContentSharingPicker *)picker
{
    if (!_observingPicker)
        return;

    [picker setActive:NO];

    _observingPicker = NO;
    [picker removeObserver:self];
}

@end

namespace WebCore {

bool ScreenCaptureKitSharingSessionManager::isAvailable()
{
    return PAL::getSCContentSharingPickerClassSingleton() && PAL::getSCContentSharingPickerConfigurationClassSingleton();
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

    if (m_activeSources.isEmpty()) {
        RetainPtr picker = [PAL::getSCContentSharingPickerClassSingleton() sharedPicker];
        [m_promptHelper stopObservingPicker:picker.get()];
    }

    if (!m_completionHandler)
        return;

    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    completionHandler(std::nullopt);
}

void ScreenCaptureKitSharingSessionManager::completeDeviceSelection(SCContentFilter* contentFilter)
{
    m_promptWatchdogTimer = nullptr;

    if (!contentFilter || !m_completionHandler) {
        if (m_completionHandler)
            m_completionHandler(std::nullopt);
        return;
    }

    m_pendingContentFilter = contentFilter;

    std::optional<CaptureDevice> device;
    switch (contentFilter.style) {
    case SCShareableContentStyleWindow: {
        RetainPtr windows = retainPtr(contentFilter.includedWindows);
        ASSERT([windows count] == 1);
        if (![windows count])
            return;

        RetainPtr window = retainPtr(windows.get()[0]);
        device = CaptureDevice(String::number([window windowID]), CaptureDevice::DeviceType::Window, [window title], emptyString(), true);
        break;
    }
    case SCShareableContentStyleDisplay: {
        RetainPtr displays = retainPtr(contentFilter.includedDisplays);
        ASSERT([displays count] == 1);
        if (![displays count])
            return;

        device = CaptureDevice(String::number(displays.get()[0].displayID), CaptureDevice::DeviceType::Screen, "Screen "_str, emptyString(), true);
        break;
    }
    case SCShareableContentStyleNone:
    case SCShareableContentStyleApplication:
        ASSERT_NOT_REACHED();
        return;
    }

    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    completionHandler(device);
}

void ScreenCaptureKitSharingSessionManager::contentSharingPickerSelectedFilterForStream(SCContentFilter* contentFilter, SCStream*)
{
    ASSERT(isMainThread());
    RELEASE_LOG(WebRTC, "ScreenCaptureKitSharingSessionManager::contentSharingPickerSelectedFilterForStream");

    completeDeviceSelection(contentFilter);
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

    m_completionHandler = WTF::move(completionHandler);

    bool showingPicker = promptWithSCContentSharingPicker(promptType);
    if (!showingPicker) {
        m_completionHandler(std::nullopt);
        return;
    }

    constexpr Seconds userPromptWatchdogInterval = 60_s;
    m_promptWatchdogTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), "ScreenCaptureKitSharingSessionManager::PromptWatchdogTimer"_s, [weakThis = WeakPtr { *this }, interval = userPromptWatchdogInterval]() mutable {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::promptForGetDisplayMedia nothing picked after %f seconds, cancelling.", interval.value());

        protectedThis->cancelPicking();
    });
    m_promptWatchdogTimer->startOneShot(userPromptWatchdogInterval);
}

bool ScreenCaptureKitSharingSessionManager::promptWithSCContentSharingPicker(DisplayCapturePromptType promptType)
{
    auto configuration = adoptNS([PAL::allocSCContentSharingPickerConfigurationInstance() init]);
    SCShareableContentStyle shareableContentStyle = SCShareableContentStyleNone;
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

    RetainPtr picker = [PAL::getSCContentSharingPickerClassSingleton() sharedPicker];
    [picker setDefaultConfiguration:configuration.get()];
    [picker setMaximumStreamCount:@(std::numeric_limits<unsigned>::max())];
    [m_promptHelper startObservingPicker:picker.get()];

    if (shareableContentStyle != SCShareableContentStyleNone)
        [picker presentPickerUsingContentStyle:shareableContentStyle];
    else
        [picker present];

    return true;
}

bool ScreenCaptureKitSharingSessionManager::promptingInProgress() const
{
    return !!m_completionHandler;
}

void ScreenCaptureKitSharingSessionManager::contentSharingPickerFailedWithError(NSError* error)
{
    ASSERT(isMainThread());

    RELEASE_LOG_ERROR_IF(error, WebRTC, "ScreenCaptureKitSharingSessionManager::contentSharingPickerFailedWithError content sharing picker failed with error '%s'", [[error localizedDescription] UTF8String]);

    cancelPicking();
}

static bool operator==(const SCContentFilter* filter, const CaptureDevice& device)
{
    auto deviceID = parseInteger<uint32_t>(device.persistentId());
    if (!deviceID)
        return false;

    if (device.type() == CaptureDevice::DeviceType::Screen) {
        if (filter.style != SCShareableContentStyleDisplay)
            return false;

        RetainPtr displays = retainPtr(filter.includedDisplays);
        if (![displays count])
            return false;

        return displays.get()[0].displayID == deviceID;
    }

    if (device.type() == CaptureDevice::DeviceType::Window) {
        if (filter.style != SCShareableContentStyleWindow)
            return false;

        RetainPtr windows = retainPtr(filter.includedWindows);
        ASSERT([windows count] == 1);
        if (![windows count])
            return false;

        return windows.get()[0].windowID == deviceID;
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

RetainPtr<SCContentFilter> ScreenCaptureKitSharingSessionManager::contentFilter(const CaptureDevice& device)
{
    ASSERT(isMainThread());

    if (m_pendingContentFilter.get() != device) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::contentFilterFromCaptureDevice - unknown capture device.");
        return { };
    }

    return std::exchange(m_pendingContentFilter, { });
}

RefPtr<ScreenCaptureSessionSource> ScreenCaptureKitSharingSessionManager::createSessionSourceForDevice(WeakPtr<ScreenCaptureSessionSourceObserver> observer, SCContentFilter* contentFilter, SCStreamConfiguration* configuration, SCStreamDelegate* delegate)
{
    ASSERT(isMainThread());

    RetainPtr<SCStream> stream = adoptNS([PAL::allocSCStreamInstance() initWithFilter:contentFilter configuration:configuration delegate:(id<SCStreamDelegate> _Nullable)delegate]);

    if (!stream) {
        RELEASE_LOG_ERROR(WebRTC, "ScreenCaptureKitSharingSessionManager::createSessionSourceForDevice - unable to create SCStream.");
        return nullptr;
    }

    auto cleanupFunction = [weakThis = WeakPtr { *this }](ScreenCaptureSessionSource& source) mutable {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->cleanupSessionSource(source);
    };

    auto newSession = ScreenCaptureSessionSource::create(WTF::move(observer), WTF::move(stream), contentFilter, WTF::move(cleanupFunction));
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

Ref<ScreenCaptureSessionSource> ScreenCaptureSessionSource::create(WeakPtr<ScreenCaptureSessionSourceObserver> observer, RetainPtr<SCStream> stream, RetainPtr<SCContentFilter> filter, CleanupFunction&& cleanupFunction)
{
    return adoptRef(*new ScreenCaptureSessionSource(WTF::move(observer), WTF::move(stream), WTF::move(filter), WTF::move(cleanupFunction)));
}

ScreenCaptureSessionSource::ScreenCaptureSessionSource(WeakPtr<ScreenCaptureSessionSourceObserver>&& observer, RetainPtr<SCStream>&& stream, RetainPtr<SCContentFilter>&& filter, CleanupFunction&& cleanupFunction)
    : m_stream(WTF::move(stream))
    , m_contentFilter(WTF::move(filter))
    , m_observer(WTF::move(observer))
    , m_cleanupFunction(WTF::move(cleanupFunction))
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

    return m_observer == other.observer();
}

void ScreenCaptureSessionSource::updateContentFilter(SCContentFilter* contentFilter)
{
    ASSERT(m_observer);
    m_contentFilter = contentFilter;
    CheckedRef { *m_observer }->sessionFilterDidChange(contentFilter);
}

void ScreenCaptureSessionSource::streamDidEnd()
{
    ASSERT(m_observer);
    CheckedRef { *m_observer }->sessionStreamDidEnd(m_stream.get());
}

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
