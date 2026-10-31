/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
#import "MediaPermissionUtilities.h"

#import "CapturePreviewDeviceSelection.h"
#import "CapturePreviewPromptModel.h"
#import "GPUProcessProxy.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "WKCapturePreviewPrompt.h"
#import "WKCapturePreviewPrompting.h"
#import "WKWebViewInternal.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <WebCore/HostingContext.h>
#import <WebCore/IntSize.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/MediaProducer.h>
#import <WebCore/PageIdentifier.h>
#import <WebCore/ProcessIdentifier.h>
#import <WebCore/SecurityOriginData.h>
#import <mutex>
#import <wtf/BlockPtr.h>
#import <wtf/Function.h>
#import <wtf/Markable.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/ProcessID.h>
#import <wtf/RunLoop.h>
#import <wtf/URLHelpers.h>
#import <wtf/Vector.h>
#import <wtf/WeakHashSet.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/WeakPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cf/CFBundleSPI.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitUtilities.h"
#if ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)
#import "WKCapturePreviewViewController.h"
#endif
#endif

#import "TCCSoftLink.h"
#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cocoa/SpeechSoftLink.h>

#if ENABLE(MEDIA_STREAM)

@interface WKCapturePreviewPromptForTesting : NSObject <WKCapturePreviewPrompting> {
    WebKit::CapturePreviewPromptModel _model;
    uint32_t _previewHostingContextID;
    unsigned _previewLayerCount;
    unsigned _audioLevelUpdateCount;
}

- (instancetype)initWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices;
- (const WebKit::CapturePreviewDeviceSelection&)selection;
- (unsigned)previewLayerCount;
- (unsigned)audioLevelUpdateCount;
- (BOOL)hasPreviewLayer;
- (void)selectDeviceOfType:(WebKit::CapturePreviewDeviceType)type atIndex:(size_t)index;
- (void)beginPreview;

@end

@implementation WKCapturePreviewPromptForTesting

- (instancetype)initWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (!(self = [super init]))
        return nil;

    _model = { WTF::move(videoDevices), WTF::move(audioDevices) };
    return self;
}

- (const WebKit::CapturePreviewDeviceSelection&)selection
{
    return _model;
}

- (NSString *)selectedVideoDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Camera)).autorelease();
}

- (NSString *)selectedAudioDeviceID
{
    return nsStringNilIfEmpty(_model.selectedDeviceID(WebKit::CapturePreviewDeviceType::Microphone)).autorelease();
}

- (unsigned)previewLayerCount
{
    return _previewLayerCount;
}

- (unsigned)audioLevelUpdateCount
{
    return _audioLevelUpdateCount;
}

- (BOOL)hasPreviewLayer
{
    return !!_previewHostingContextID;
}

- (void)setPreviewHostingContext:(const WebCore::HostingContext&)hostingContext gpuProcessIdentifier:(ProcessID)gpuProcessIdentifier
{
    UNUSED_PARAM(gpuProcessIdentifier);
    if (_model.shouldIgnoreHostingContext(hostingContext))
        return;
    if (hostingContext.contextID && hostingContext.contextID != _previewHostingContextID)
        ++_previewLayerCount;
    _previewHostingContextID = hostingContext.contextID;
}

- (void)setSelectionChangedHandler:(Function<void(std::optional<WebCore::CaptureDevice>&&, std::optional<WebCore::CaptureDevice>&&)>&&)handler
{
    _model.setSelectionChangedHandler(WTF::move(handler));
}

- (void)setAudioLevel:(float)level
{
    UNUSED_PARAM(level);
    if (!_model.isStopped())
        ++_audioLevelUpdateCount;
}

- (void)updateWithVideoDevices:(Vector<WebCore::CaptureDevice>&&)videoDevices audioDevices:(Vector<WebCore::CaptureDevice>&&)audioDevices
{
    if (_model.update(WTF::move(videoDevices), WTF::move(audioDevices)))
        _model.notifySelectionChanged();
}

- (void)selectDeviceOfType:(WebKit::CapturePreviewDeviceType)type atIndex:(size_t)index
{
    if (index >= _model.devices(type).size() || index == _model.selectedIndex(type))
        return;
    _model.select(type, index);
    _model.notifySelectionChanged();
}

- (void)beginPreview
{
    _model.beginPreview();
}

- (void)stop
{
    _model.stop();
    _previewHostingContextID = 0;
}

- (BOOL)isStopped
{
    return _model.isStopped();
}

- (CGSize)previewSize
{
    return CGSizeMake(320, 180);
}

@end

#endif // ENABLE(MEDIA_STREAM)

namespace WebKit {
bool checkSandboxRequirementForType(MediaPermissionType type)
{
#if PLATFORM(MAC)
    auto checkFunction = [](ASCIILiteral operation) {
        if (!currentProcessIsSandboxed())
            return true;

        int result = sandbox_check(getpid(), operation, static_cast<enum sandbox_filter_type>(SANDBOX_CHECK_NO_REPORT | SANDBOX_FILTER_NONE));
        if (result == -1)
            SAFE_WTFLOGALWAYS("Error checking '%s' sandbox access, errno=%ld", operation, (long)errno);
        return !result;
    };

    switch (type) {
    case MediaPermissionType::Audio:
        static bool isAudioEntitled = checkFunction("device-microphone"_s);
        return isAudioEntitled;
    case MediaPermissionType::Video:
        static bool isVideoEntitled = checkFunction("device-camera"_s);
        return isVideoEntitled;
    }
#endif
    return true;
}

bool checkUsageDescriptionStringForType(MediaPermissionType type)
{
    switch (type) {
    case MediaPermissionType::Audio:
        static TCCAccessPreflightResult audioAccess = TCCAccessPreflight(get_TCC_kTCCServiceMicrophoneSingleton(), NULL);
        if (audioAccess == kTCCAccessPreflightGranted)
            return true;
        static bool hasMicrophoneDescriptionString= dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSMicrophoneUsageDescription"]).length > 0;
        return hasMicrophoneDescriptionString;
    case MediaPermissionType::Video:
        static TCCAccessPreflightResult videoAccess = TCCAccessPreflight(get_TCC_kTCCServiceCameraSingleton(), NULL);
        if (videoAccess == kTCCAccessPreflightGranted)
            return true;
        static bool hasCameraDescriptionString = dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSCameraUsageDescription"]).length > 0;
        return hasCameraDescriptionString;
    }
}

bool checkUsageDescriptionStringForSpeechRecognition()
{
    return dynamic_objc_cast<NSString>(NSBundle.mainBundle.infoDictionary[@"NSSpeechRecognitionUsageDescription"]).length > 0;
}

static RetainPtr<NSString> visibleDomain(const String& host)
{
    auto domain = WTF::URLHelpers::userVisibleURL(host.utf8().span());
    return startsWithLettersIgnoringASCIICase(domain, "www."_s) ? StringView(domain).substring(4).createNSString() : domain.createNSString();
}

RetainPtr<NSString> applicationVisibleNameFromOrigin(const WebCore::SecurityOriginData& origin)
{
    if (origin.protocol() != "http"_s && origin.protocol() != "https"_s)
        return nil;

    return visibleDomain(origin.host());
}

RetainPtr<NSString> applicationVisibleName()
{
    RetainPtr appBundle = [NSBundle mainBundle];
    if (RetainPtr displayName = dynamic_objc_cast<NSString>(appBundle.get().infoDictionary[bridge_cast(_kCFBundleDisplayNameKey)]))
        return displayName;
    return dynamic_objc_cast<NSString>(appBundle.get().infoDictionary[bridge_cast(kCFBundleNameKey)]);
}

static RetainPtr<NSString> alertMessageText(MediaPermissionReason reason, const WebCore::SecurityOriginData& origin)
{
    RetainPtr visibleOrigin = applicationVisibleNameFromOrigin(origin);
    if (!visibleOrigin)
        visibleOrigin = applicationVisibleName();

    switch (reason) {
    case MediaPermissionReason::Camera:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera?", @"Message for user camera access prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::CameraAndMicrophone:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your camera and microphone?", @"Message for user media prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::Microphone:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your microphone?", @"Message for user microphone access prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::ScreenCapture:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to observe your screen?", @"Message for screen sharing prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::DeviceOrientation:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"“%@” Would Like to Access Motion and Orientation", @"Message for requesting access to the device motion and orientation"), visibleOrigin.get()]);
    case MediaPermissionReason::Geolocation:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to use your current location?", @"Message for geolocation prompt"), visibleOrigin.get()]);
    case MediaPermissionReason::SpeechRecognition:
        SUPPRESS_UNRETAINED_ARG return adoptNS([[NSString alloc] initWithFormat:WEB_UI_NSSTRING(@"Allow “%@” to capture your audio and use it for speech recognition?", @"Message for spechrecognition prompt"), visibleDomain(origin.host()).get()]);
    }
}

static RetainPtr<NSString> allowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (usermedia)", @"Allow button title in user media prompt").createNSString();
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (screensharing)", @"Allow button title in screen sharing prompt").createNSString();
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt").createNSString();
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (geolocation)", @"Allow button title in geolocation prompt").createNSString();
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Allow", "Allow (speechrecognition)", @"Allow button title in speech recognition prompt").createNSString();
    }
}

static RetainPtr<NSString> doNotAllowButtonText(MediaPermissionReason reason)
{
    switch (reason) {
    case MediaPermissionReason::Camera:
    case MediaPermissionReason::CameraAndMicrophone:
    case MediaPermissionReason::Microphone:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (usermedia)", @"Disallow button title in user media prompt").createNSString();
    case MediaPermissionReason::ScreenCapture:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (screensharing)", @"Disallow button title in screen sharing prompt").createNSString();
    case MediaPermissionReason::DeviceOrientation:
        return WEB_UI_STRING_KEY(@"Cancel", "Cancel (device motion and orientation access)", @"Button title in Device Orientation Permission API prompt").createNSString();
    case MediaPermissionReason::Geolocation:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (geolocation)", @"Disallow button title in geolocation prompt").createNSString();
    case MediaPermissionReason::SpeechRecognition:
        return WEB_UI_STRING_KEY(@"Don’t Allow", "Don’t Allow (speechrecognition)", @"Disallow button title in speech recognition prompt").createNSString();
    }
}

#if PLATFORM(MAC)
static MediaPermissionPromptResult promptResultForAlertResponse(NSModalResponse returnCode, const Vector<MediaPermissionPromptAdditionalAction>& additionalActions)
{
    if (returnCode == NSAlertFirstButtonReturn)
        return { .granted = true, .wasAnswered = true };
    if (returnCode == NSAlertSecondButtonReturn)
        return { .granted = false, .wasAnswered = true };

    if (returnCode == NSModalResponseAbort)
        return { };

    // Return NSAlertFirstButtonReturn and NSAlertSecondButtonReturn for the two built-in buttons,
    // NSAlertThirdButtonReturn... for custom buttons
    size_t index = returnCode - NSAlertThirdButtonReturn;
    if (index >= additionalActions.size()) {
        ASSERT_NOT_REACHED();
        return { };
    }

    return {
        .granted = additionalActions[index].decision == MediaPermissionPromptAdditionalAction::Decision::Allow,
        .wasAnswered = true,
        .chosenAction = additionalActions[index].source,
    };
}
#endif

static RetainPtr<NSString> promptMessageText(const PermissionPromptRequest& request)
{
    if (!request.customization.messageText.isNull())
        return request.customization.messageText.createNSString();
    return alertMessageText(request.reason, request.origin);
}

static RetainPtr<NSString> promptAllowButtonText(const PermissionPromptRequest& request)
{
    if (!request.customization.allowButtonTitle.isNull())
        return request.customization.allowButtonTitle.createNSString();
    return allowButtonText(request.reason);
}

static RetainPtr<NSString> promptDenyButtonText(const PermissionPromptRequest& request)
{
    if (!request.customization.denyButtonTitle.isNull())
        return request.customization.denyButtonTitle.createNSString();
    return doNotAllowButtonText(request.reason);
}

static RetainPtr<NSString> promptInformativeText(const PermissionPromptRequest& request)
{
    if (request.customization.informativeText.isNull())
        return nil;
    return request.customization.informativeText.createNSString();
}

void alertForPermission(WebPageProxy& page, PermissionPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

#if PLATFORM(IOS_FAMILY)
    if (request.reason == MediaPermissionReason::DeviceOrientation) {
        if (auto& userPermissionHandler = page.deviceOrientationUserPermissionHandlerForTesting())
            return completionHandler({ .granted = userPermissionHandler() });
    }
#endif

    RetainPtr webView = page.cocoaView();
    if (!webView)
        return completionHandler({ });

    RetainPtr alertTitle = promptMessageText(request);
    if (!alertTitle)
        return completionHandler({ });

    RetainPtr allowButtonString = promptAllowButtonText(request);
    RetainPtr doNotAllowButtonString = promptDenyButtonText(request);
    auto completionBlock = makeBlockPtr(WTF::move(completionHandler));

#if PLATFORM(MAC)
    RetainPtr alert = adoptNS([NSAlert new]);
    [alert setMessageText:alertTitle.get()];
    if (RetainPtr informativeText = promptInformativeText(request))
        [alert setInformativeText:informativeText.get()];
    RetainPtr button = [alert addButtonWithTitle:allowButtonString.get()];
    [button setKeyEquivalent:@""];
    // Tags keep the responses promptResultForAlertResponse expects, which would otherwise follow position.
    for (size_t index = 0; index < request.customization.additionalActions.size(); ++index) {
        button = [alert addButtonWithTitle:request.customization.additionalActions[index].title.createNSString().get()];
        // Cleared so AppKit does not hand Return to a button the application supplied.
        [button setKeyEquivalent:@""];
        [button setTag:NSAlertThirdButtonReturn + index];
    }
    button = [alert addButtonWithTitle:doNotAllowButtonString.get()];
    [button setKeyEquivalent:@"\E"];
    [button setTag:NSAlertSecondButtonReturn];

    [alert beginSheetModalForWindow:retainPtr([webView window]).get() completionHandler:[completionBlock, additionalActions = WTF::move(request.customization.additionalActions)](NSModalResponse returnCode) {
        completionBlock(promptResultForAlertResponse(returnCode, additionalActions));
    }];
#else
    RetainPtr alert = WebKit::createUIAlertController(alertTitle.get(), promptInformativeText(request).get());
    RetainPtr allowAction = [UIAlertAction actionWithTitle:allowButtonString.get() style:UIAlertActionStyleDefault handler:[completionBlock](UIAlertAction *) {
        completionBlock({ .granted = true, .wasAnswered = true });
    }];

    RetainPtr doNotAllowAction = [UIAlertAction actionWithTitle:doNotAllowButtonString.get() style:UIAlertActionStyleCancel handler:[completionBlock](UIAlertAction *) {
        completionBlock({ .granted = false, .wasAnswered = true });
    }];

    [alert addAction:allowAction.get()];

    for (auto& action : request.customization.additionalActions) {
        bool granted = action.decision == MediaPermissionPromptAdditionalAction::Decision::Allow;
        [alert addAction:[UIAlertAction actionWithTitle:action.title.createNSString().get() style:UIAlertActionStyleDefault handler:[completionBlock, source = action.source, granted](UIAlertAction *) {
            completionBlock({ .granted = granted, .wasAnswered = true, .chosenAction = source });
        }]];
    }

    [alert addAction:doNotAllowAction.get()];

#if PLATFORM(VISION)
    page.dispatchWillPresentModalUI();
#endif
    [[webView _wk_viewControllerForFullScreenPresentation] presentViewController:alert.get() animated:YES completion:nil];
#endif
}

void alertForPermission(WebPageProxy& page, MediaPermissionReason reason, const WebCore::SecurityOriginData& origin, CompletionHandler<void(bool)>&& completionHandler)
{
    alertForPermission(page, { .reason = reason, .origin = origin }, [completionHandler = WTF::move(completionHandler)](MediaPermissionPromptResult&& result) mutable {
        completionHandler(result.granted);
    });
}

#if ENABLE(MEDIA_STREAM)

class CapturePreviewSession : public RefCounted<CapturePreviewSession> {
public:
    static Ref<CapturePreviewSession> create(WebCore::PageIdentifier pageIdentifier) { return adoptRef(*new CapturePreviewSession(pageIdentifier)); }
    ~CapturePreviewSession() { resume(); }

    WebCore::PageIdentifier pageIdentifier() const { return m_pageIdentifier; }

    Ref<GenericPromise::AllPromiseType> pause(WebCore::MediaProducerMutedStateFlags captureKinds)
    {
        Vector<Ref<GenericPromise>> promises;
        for (Ref process : WebProcessProxy::allProcesses()) {
            for (Ref page : process->pages()) {
                if (m_pausedPages.add(page.get()).isNewEntry)
                    page->beginCapturePreviewPause();
                promises.append(page->pauseCaptureForCapturePreview(captureKinds));
            }
        }
        return GenericPromise::all(promises);
    }

    void resume()
    {
        if (previewOwner() == m_pageIdentifier)
            previewOwner().reset();
        for (Ref page : std::exchange(m_pausedPages, { }))
            page->endCapturePreviewPause();
    }

    bool claimPreview()
    {
        if (previewOwner() && *previewOwner() != m_pageIdentifier)
            return false;
        previewOwner() = m_pageIdentifier;
        return true;
    }

    unsigned nextSelection() { return ++m_selectionCount; }
    bool isLatestSelection(unsigned selection) const { return selection == m_selectionCount; }

    bool connectTo(const GPUProcessProxy& gpuProcess)
    {
        if (m_gpuProcessIdentifier == gpuProcess.coreProcessIdentifier())
            return false;
        m_gpuProcessIdentifier = gpuProcess.coreProcessIdentifier();
        return true;
    }

private:
    explicit CapturePreviewSession(WebCore::PageIdentifier pageIdentifier)
        : m_pageIdentifier(pageIdentifier)
    {
    }

    static Markable<WebCore::PageIdentifier>& previewOwner()
    {
        static NeverDestroyed<Markable<WebCore::PageIdentifier>> owner;
        return owner.get();
    }

    const WebCore::PageIdentifier m_pageIdentifier;
    WeakHashSet<WebPageProxy> m_pausedPages;
    unsigned m_selectionCount { 0 };
    Markable<WebCore::ProcessIdentifier> m_gpuProcessIdentifier;
};

static void connectPreviewToGPUProcess(WebPageProxy& page, id<WKCapturePreviewPrompting> prompt, const WebCore::SecurityOriginData& origin, CapturePreviewSession& session)
{
    Ref process = page.legacyMainFrameProcess();
    Ref gpuProcess = protect(process->processPool())->ensureGPUProcess();
    if (!session.connectTo(gpuProcess))
        return;

    gpuProcess->setCapturePreviewAudioLevelHandler(session.pageIdentifier(), [weakPrompt = WeakObjCPtr<id<WKCapturePreviewPrompting>>(prompt)](float level) {
        [weakPrompt.get() setAudioLevel:level];
    });

    [prompt setSelectionChangedHandler:[pageForPreview = WeakPtr { page }, weakPrompt = WeakObjCPtr<id<WKCapturePreviewPrompting>>(prompt), origin, session = Ref { session }](std::optional<WebCore::CaptureDevice>&& videoDevice, std::optional<WebCore::CaptureDevice>&& audioDevice) mutable {
        if (!pageForPreview || !weakPrompt.get() || !session->claimPreview())
            return;

        WebCore::MediaProducerMutedStateFlags captureKinds;
        if (videoDevice)
            captureKinds.add(WebCore::MediaProducerMutedState::VideoCaptureIsMuted);
        if (audioDevice)
            captureKinds.add(WebCore::MediaProducerMutedState::AudioCaptureIsMuted);

        unsigned selection = session->nextSelection();
        session->pause(captureKinds)->whenSettled(RunLoop::mainSingleton(), [pageForPreview, weakPrompt, origin, session, selection, videoDevice = WTF::move(videoDevice), audioDevice = WTF::move(audioDevice)](auto&&) mutable {
            RefPtr page = pageForPreview;
            RetainPtr prompt = weakPrompt.get();
            if (!page || !prompt || [prompt isStopped] || !session->isLatestSelection(selection))
                return;

            Ref process = page->legacyMainFrameProcess();
            Ref gpuProcess = protect(process->processPool())->ensureGPUProcess();
            CGSize previewSize = [prompt previewSize];
            bool expectsLayer = !!videoDevice;
            gpuProcess->startCapturePreview(WTF::move(videoDevice), WTF::move(audioDevice), session->pageIdentifier(), WebCore::IntSize(static_cast<int>(previewSize.width), static_cast<int>(previewSize.height)), page->orientationForMediaCapture(), origin,
                [weakPrompt, gpuProcessIdentifier = gpuProcess->processID(), expectsLayer](WebCore::HostingContext hostingContext) mutable {
                    if (expectsLayer && !hostingContext.contextID)
                        RELEASE_LOG_ERROR(WebRTC, "The capture preview did not start");
                    [weakPrompt.get() setPreviewHostingContext:hostingContext gpuProcessIdentifier:gpuProcessIdentifier];
                });
        });
    }];
}

static void stopPreviewAndReport(id<WKCapturePreviewPrompting> prompt, CapturePreviewSession& session, MediaPermissionPromptResult&& result, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    if (result.granted) {
        result.selectedAudioDeviceUID = [prompt selectedAudioDeviceID];
        result.selectedVideoDeviceUID = [prompt selectedVideoDeviceID];
    }
    [prompt stop];

    auto stopRotationMonitoring = [pageIdentifier = session.pageIdentifier()] {
        if (RefPtr page = WebProcessProxy::webPage(pageIdentifier))
            page->stopMonitoringCaptureDeviceRotationForCapturePreview();
    };

    RefPtr gpuProcess = GPUProcessProxy::singletonIfCreated();
    if (!gpuProcess) {
        stopRotationMonitoring();
        session.resume();
        return completionHandler(WTF::move(result));
    }

    gpuProcess->stopCapturePreview(session.pageIdentifier(), [stopRotationMonitoring = WTF::move(stopRotationMonitoring), session = Ref { session }, result = WTF::move(result), completionHandler = WTF::move(completionHandler)] mutable {
        stopRotationMonitoring();
        session->resume();
        completionHandler(WTF::move(result));
    });
}

static CapturePreviewDeviceListUpdater deviceListUpdater(id<WKCapturePreviewPrompting> prompt)
{
    return [weakPrompt = WeakObjCPtr<id<WKCapturePreviewPrompting>>(prompt)](Vector<WebCore::CaptureDevice>&& videoDevices, Vector<WebCore::CaptureDevice>&& audioDevices) mutable {
        [weakPrompt.get() updateWithVideoDevices:WTF::move(videoDevices) audioDevices:WTF::move(audioDevices)];
    };
}

static CapturePreviewRestarter previewRestarter(WebPageProxy& page, id<WKCapturePreviewPrompting> prompt, const WebCore::SecurityOriginData& origin, CapturePreviewSession& session)
{
    return [weakPage = WeakPtr { page }, weakPrompt = WeakObjCPtr<id<WKCapturePreviewPrompting>>(prompt), origin, session = Ref { session }] {
        RefPtr page = weakPage;
        RetainPtr prompt = weakPrompt.get();

        if (!page || !prompt || [prompt isStopped])
            return;

        connectPreviewToGPUProcess(*page, prompt.get(), origin, session);
    };
}

class CapturePreviewPromptForTesting final : public MediaPermissionPromptForTesting {
public:
    static Ref<CapturePreviewPromptForTesting> create(String&& title, String&& informativeText, Vector<String>&& buttonTitles, bool showsPreview, RetainPtr<WKCapturePreviewPromptForTesting>&& prompt, RefPtr<CapturePreviewSession>&& session, Vector<MediaPermissionPromptAdditionalAction>&& additionalActions, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
    {
        return adoptRef(*new CapturePreviewPromptForTesting(WTF::move(title), WTF::move(informativeText), WTF::move(buttonTitles), showsPreview, WTF::move(prompt), WTF::move(session), WTF::move(additionalActions), WTF::move(completionHandler)));
    }

    ~CapturePreviewPromptForTesting() final { dismiss(); }

    Vector<Device> cameras() const final { return devicesForTesting(CapturePreviewDeviceType::Camera); }
    Vector<Device> microphones() const final { return devicesForTesting(CapturePreviewDeviceType::Microphone); }
    String selectedCameraID() const final { return [m_prompt selection].selectedDeviceID(CapturePreviewDeviceType::Camera); }
    String selectedMicrophoneID() const final { return [m_prompt selection].selectedDeviceID(CapturePreviewDeviceType::Microphone); }
    unsigned audioLevelUpdateCount() const final { return [m_prompt audioLevelUpdateCount]; }
    unsigned previewLayerCount() const final { return [m_prompt previewLayerCount]; }
    bool hasPreviewLayer() const final { return [m_prompt hasPreviewLayer]; }

    void selectCamera(size_t index) final { [m_prompt selectDeviceOfType:CapturePreviewDeviceType::Camera atIndex:index]; }
    void selectMicrophone(size_t index) final { [m_prompt selectDeviceOfType:CapturePreviewDeviceType::Microphone atIndex:index]; }

    void respond(MediaPermissionPromptResponseForTesting&& response) final
    {
        MediaPermissionPromptResult result;
        if (auto index = response.buttonIndex) {
            result.wasAnswered = true;
            if (!*index)
                result.granted = true;
            else if (*index <= m_additionalActions.size()) {
                result.chosenAction = m_additionalActions[*index - 1].source;
                result.granted = m_additionalActions[*index - 1].decision == MediaPermissionPromptAdditionalAction::Decision::Allow;
            }
        }
        selectDevice(CapturePreviewDeviceType::Camera, response.cameraPersistentID);
        selectDevice(CapturePreviewDeviceType::Microphone, response.microphonePersistentID);
        finish(WTF::move(result));
    }

    void dismiss() { finish({ }); }

private:
    CapturePreviewPromptForTesting(String&& title, String&& informativeText, Vector<String>&& buttonTitles, bool showsPreview, RetainPtr<WKCapturePreviewPromptForTesting>&& prompt, RefPtr<CapturePreviewSession>&& session, Vector<MediaPermissionPromptAdditionalAction>&& additionalActions, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
        : MediaPermissionPromptForTesting(WTF::move(title), WTF::move(informativeText), WTF::move(buttonTitles), showsPreview)
        , m_prompt(WTF::move(prompt))
        , m_session(WTF::move(session))
        , m_additionalActions(WTF::move(additionalActions))
        , m_completionHandler(WTF::move(completionHandler))
    {
    }

    Vector<Device> devicesForTesting(CapturePreviewDeviceType type) const
    {
        return WTF::map([m_prompt selection].devices(type), [](auto& device) {
            return Device { device.persistentId(), device.label() };
        });
    }

    void selectDevice(CapturePreviewDeviceType type, const String& persistentID)
    {
        if (persistentID.isNull())
            return;
        size_t index = [m_prompt selection].devices(type).findIf([&](auto& device) {
            return device.persistentId() == persistentID;
        });
        ASSERT(index != notFound);
        [m_prompt selectDeviceOfType:type atIndex:index];
    }

    void finish(MediaPermissionPromptResult&& result)
    {
        auto completionHandler = std::exchange(m_completionHandler, nullptr);
        if (!completionHandler)
            return;

        if (RefPtr session = m_session) {
            stopPreviewAndReport(m_prompt.get(), *session, WTF::move(result), WTF::move(completionHandler));
            return;
        }

        if (result.granted && showsPreview()) {
            result.selectedAudioDeviceUID = [m_prompt selection].selectedDeviceID(CapturePreviewDeviceType::Microphone);
            result.selectedVideoDeviceUID = [m_prompt selection].selectedDeviceID(CapturePreviewDeviceType::Camera);
        }
        [m_prompt stop];
        completionHandler(WTF::move(result));
    }

    const RetainPtr<WKCapturePreviewPromptForTesting> m_prompt;
    const RefPtr<CapturePreviewSession> m_session;
    const Vector<MediaPermissionPromptAdditionalAction> m_additionalActions;
    CompletionHandler<void(MediaPermissionPromptResult&&)> m_completionHandler;
};

CapturePreviewPromptHandles alertForPermissionForTesting(WebPageProxy& page, CapturePreviewPromptRequest&& request, bool showsPreview, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    PermissionPromptRequest textRequest { .reason = request.reason, .origin = request.origin, .customization = request.customization };
    Vector<String> buttonTitles;
    buttonTitles.append(promptAllowButtonText(textRequest).get());
    for (auto& action : request.customization.additionalActions)
        buttonTitles.append(action.title);
    buttonTitles.append(promptDenyButtonText(textRequest).get());

    Vector<WebCore::CaptureDevice> videoDevices;
    Vector<WebCore::CaptureDevice> audioDevices;
    if (showsPreview) {
        videoDevices = WTF::move(request.eligibleVideoDevices);
        audioDevices = WTF::move(request.eligibleAudioDevices);
    }
    RetainPtr prompt = adoptNS([[WKCapturePreviewPromptForTesting alloc] initWithVideoDevices:WTF::move(videoDevices) audioDevices:WTF::move(audioDevices)]);

    ASSERT(showsPreview || !page.mediaPermissionPromptRunsPreviewForTesting());

    RefPtr<CapturePreviewSession> session;
    if (showsPreview && page.mediaPermissionPromptRunsPreviewForTesting()) {
        session = CapturePreviewSession::create(page.webPageIDInMainFrameProcess());
        connectPreviewToGPUProcess(page, prompt.get(), request.origin, *session);
        [prompt beginPreview];
    }

    Ref testingPrompt = CapturePreviewPromptForTesting::create(promptMessageText(textRequest).get(), promptInformativeText(textRequest).get(), WTF::move(buttonTitles), showsPreview, RetainPtr { prompt }, RefPtr { session }, WTF::move(request.customization.additionalActions), WTF::move(completionHandler));
    page.mediaPermissionPromptHandlerForTesting()(testingPrompt.copyRef());

    return {
        .deviceListUpdater = session ? deviceListUpdater(prompt.get()) : nullptr,
        .promptDismisser = [testingPrompt] {
            testingPrompt->dismiss();
        },
        .previewRestarter = session ? previewRestarter(page, prompt.get(), request.origin, *session) : nullptr,
    };
}

#endif

#if PLATFORM(MAC) && ENABLE(MEDIA_STREAM)

CapturePreviewPromptHandles alertForPermissionWithCapturePreview(WebPageProxy& page, CapturePreviewPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    RetainPtr webView = page.cocoaView();
    RetainPtr parentWindow = [webView window];
    if (!parentWindow) {
        completionHandler({ });
        return { };
    }

    PermissionPromptRequest textRequest { .reason = request.reason, .origin = request.origin, .customization = request.customization };
    RetainPtr alertTitle = promptMessageText(textRequest);
    if (!alertTitle) {
        completionHandler({ });
        return { };
    }

    RetainPtr chooser = adoptNS([[WKCaptureDeviceChooser alloc] initWithVideoDevices:WTF::move(request.eligibleVideoDevices) audioDevices:WTF::move(request.eligibleAudioDevices)]);

    RetainPtr chooserView = [chooser view];
    RetainPtr sheet = adoptNS([[WKCapturePreviewPromptSheet alloc] initWithMessageText:alertTitle.get() informativeText:promptInformativeText(textRequest).get() accessoryView:chooserView.get()]);
    RetainPtr allowButton = [sheet addButtonWithTitle:promptAllowButtonText(textRequest).get() response:NSAlertFirstButtonReturn];

    RetainPtr allowButtons = adoptNS([[NSMutableArray alloc] initWithObjects:allowButton.get(), nil]);
    for (size_t index = 0; index < request.customization.additionalActions.size(); ++index) {
        auto& action = request.customization.additionalActions[index];
        RetainPtr additionalButton = [sheet addButtonWithTitle:action.title.createNSString().get() response:NSAlertThirdButtonReturn + index];
        if (action.decision == MediaPermissionPromptAdditionalAction::Decision::Allow)
            [allowButtons addObject:additionalButton.get()];
    }

    RetainPtr denyButton = [sheet addButtonWithTitle:promptDenyButtonText(textRequest).get() response:NSAlertSecondButtonReturn];
    [denyButton setKeyEquivalent:@"\E"];

    [chooser setAllowButtons:allowButtons.get()];

    Ref session = CapturePreviewSession::create(page.webPageIDInMainFrameProcess());
    connectPreviewToGPUProcess(page, chooser.get(), request.origin, session);

    auto completionBlock = makeBlockPtr([completionHandler = WTF::move(completionHandler), chooser, sheet, session, additionalActions = WTF::move(request.customization.additionalActions)](NSModalResponse returnCode) mutable {
        stopPreviewAndReport(chooser.get(), session, promptResultForAlertResponse(returnCode, additionalActions), WTF::move(completionHandler));
    });

    [sheet beginSheetModalForWindow:parentWindow.get() didPresentHandler:makeBlockPtr([weakChooser = WeakObjCPtr<WKCaptureDeviceChooser>(chooser.get())] {
        [weakChooser.get() beginPreview];
    }).get() completionHandler:completionBlock.get()];

    return {
        .deviceListUpdater = deviceListUpdater(chooser.get()),
        .promptDismisser = [sheet = WTF::move(sheet)] {
            [sheet dismissWithResponse:NSModalResponseAbort];
        },
        .previewRestarter = previewRestarter(page, chooser.get(), request.origin, session),
    };
}

#elif PLATFORM(IOS_FAMILY) && ENABLE(MEDIA_STREAM) && !PLATFORM(APPLETV)

CapturePreviewPromptHandles alertForPermissionWithCapturePreview(WebPageProxy& page, CapturePreviewPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    RetainPtr webView = page.cocoaView();
    if (!webView) {
        completionHandler({ });
        return { };
    }

    PermissionPromptRequest textRequest { .reason = request.reason, .origin = request.origin, .customization = request.customization };
    RetainPtr alertTitle = promptMessageText(textRequest);
    if (!alertTitle) {
        completionHandler({ });
        return { };
    }

    RetainPtr presentingViewController = [webView _wk_viewControllerForFullScreenPresentation];
    if (!presentingViewController) {
        completionHandler({ });
        return { };
    }

    RetainPtr controller = adoptNS([[WKCapturePreviewViewController alloc] initWithAvailableSize:[presentingViewController view].safeAreaLayoutGuide.layoutFrame.size title:alertTitle.get() informativeText:promptInformativeText(textRequest).get() allowButtonTitle:promptAllowButtonText(textRequest).get() denyButtonTitle:promptDenyButtonText(textRequest).get() additionalActions:WTF::move(request.customization.additionalActions) videoDevices:WTF::move(request.eligibleVideoDevices) audioDevices:WTF::move(request.eligibleAudioDevices)]);

    Ref session = CapturePreviewSession::create(page.webPageIDInMainFrameProcess());
    connectPreviewToGPUProcess(page, controller.get(), request.origin, session);

    [controller setDecisionHandler:[completionHandler = WTF::move(completionHandler), weakController = WeakObjCPtr<WKCapturePreviewViewController>(controller.get()), session](MediaPermissionPromptResult&& result) mutable {
        RetainPtr controller = weakController.get();
        stopPreviewAndReport(controller.get(), session, WTF::move(result), WTF::move(completionHandler));
        [controller dismissWhenPossible];
    }];

#if PLATFORM(VISION)
    page.dispatchWillPresentModalUI();
#endif
    [presentingViewController presentViewController:controller.get() animated:YES completion:nil];

    return {
        .deviceListUpdater = deviceListUpdater(controller.get()),
        .promptDismisser = [weakController = WeakObjCPtr<WKCapturePreviewViewController>(controller.get())] {
            [weakController.get() dismissWithoutAnswer];
        },
        .previewRestarter = previewRestarter(page, controller.get(), request.origin, session),
    };
}

#elif PLATFORM(APPLETV) && ENABLE(MEDIA_STREAM)

// FIXME: tvOS lacks the layer hosting the preview prompt uses, so it gets the plain prompt.
CapturePreviewPromptHandles alertForPermissionWithCapturePreview(WebPageProxy& page, CapturePreviewPromptRequest&& request, CompletionHandler<void(MediaPermissionPromptResult&&)>&& completionHandler)
{
    alertForPermission(page, { .reason = request.reason, .origin = request.origin, .customization = WTF::move(request.customization) }, WTF::move(completionHandler));
    return { };
}

#endif // PLATFORM(MAC) && ENABLE(MEDIA_STREAM)

void requestAVCaptureAccessForType(MediaPermissionType type, CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

#if HAVE(AVCAPTUREDEVICE)
    RetainPtr mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto decisionHandler = makeBlockPtr([completionHandler = WTF::move(completionHandler)](BOOL authorized) mutable {
        callOnMainRunLoop([completionHandler = WTF::move(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getAVCaptureDeviceClassSingleton() requestAccessForMediaType:mediaType.get() completionHandler:decisionHandler.get()];
#else
    UNUSED_PARAM(type);
    completionHandler(false);
#endif
}

MediaPermissionResult checkAVCaptureAccessForType(MediaPermissionType type)
{
#if HAVE(AVCAPTUREDEVICE)
    RetainPtr mediaType = type == MediaPermissionType::Audio ? AVMediaTypeAudio : AVMediaTypeVideo;
    auto authorizationStatus = [PAL::getAVCaptureDeviceClassSingleton() authorizationStatusForMediaType:mediaType.get()];
    if (authorizationStatus == AVAuthorizationStatusDenied || authorizationStatus == AVAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == AVAuthorizationStatusNotDetermined)
        return MediaPermissionResult::Unknown;
    return MediaPermissionResult::Granted;
#else
    UNUSED_PARAM(type);
    return MediaPermissionResult::Denied;
#endif
}

#if HAVE(SPEECHRECOGNIZER)

void requestSpeechRecognitionAccess(CompletionHandler<void(bool authorized)>&& completionHandler)
{
    ASSERT(isMainRunLoop());

    auto decisionHandler = makeBlockPtr([completionHandler = WTF::move(completionHandler)](SFSpeechRecognizerAuthorizationStatus status) mutable {
        bool authorized = status == SFSpeechRecognizerAuthorizationStatusAuthorized;
        callOnMainRunLoop([completionHandler = WTF::move(completionHandler), authorized]() mutable {
            completionHandler(authorized);
        });
    });
    [PAL::getSFSpeechRecognizerClassSingleton() requestAuthorization:decisionHandler.get()];
}

MediaPermissionResult checkSpeechRecognitionServiceAccess()
{
    auto authorizationStatus = [PAL::getSFSpeechRecognizerClassSingleton() authorizationStatus];
IGNORE_WARNINGS_BEGIN("deprecated-enum-compare")
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusDenied || authorizationStatus == SFSpeechRecognizerAuthorizationStatusRestricted)
        return MediaPermissionResult::Denied;
    if (authorizationStatus == SFSpeechRecognizerAuthorizationStatusAuthorized)
        return MediaPermissionResult::Granted;
IGNORE_WARNINGS_END
    return MediaPermissionResult::Unknown;
}

bool checkSpeechRecognitionServiceAvailability(const String& localeIdentifier)
{
    auto recognizer = localeIdentifier.isEmpty() ? adoptNS([PAL::allocSFSpeechRecognizerInstance() init]) : adoptNS([PAL::allocSFSpeechRecognizerInstance() initWithLocale:[NSLocale localeWithLocaleIdentifier:localeIdentifier.createNSString().get()]]);
    return recognizer && [recognizer isAvailable];
}

#endif // HAVE(SPEECHRECOGNIZER)

} // namespace WebKit
