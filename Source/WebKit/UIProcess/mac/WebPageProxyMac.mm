/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#import "WebPageProxy.h"

#if PLATFORM(MAC)

#import "APIUIClient.h"
#import "APIWebsitePolicies.h"
#import "CocoaImage.h"
#import "Connection.h"
#import "FrameInfoData.h"
#import "ImageAnalysisUtilities.h"
#import "InsertTextOptions.h"
#import "MenuUtilities.h"
#import "MessageSenderInlines.h"
#import "NativeWebKeyboardEvent.h"
#import "NetworkProcessMessages.h"
#import "PDFContextMenu.h"
#import "PageClient.h"
#import "PageClientImplMac.h"
#import "PlatformFontInfo.h"
#import "PlatformWritingToolsUtilities.h"
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "TextChecker.h"
#import "WKQuickLookPreviewController.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuProxyMac.h"
#import "WebPageMessages.h"
#import "WebPageProxyInternals.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebProcessProxy.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/AttributedString.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragItem.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/Pasteboard.h>
#import <WebCore/Quirks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/UniversalAccessZoom.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <mach-o/dyld.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <pal/spi/mac/NSPasteboardSPI.h>
#import <wtf/FileHandle.h>
#import <wtf/FileSystem.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/text/cf/StringConcatenateCF.h>

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), connection)

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

#if ENABLE(PDF_PLUGIN)
@interface WKPDFMenuTarget : NSObject {
    WeakObjCPtr<NSMenuItem> _selectedMenuItem;
}
- (RetainPtr<NSMenuItem>)selectedMenuItem;
- (void)contextMenuAction:(NSMenuItem *)sender;
@end

@implementation WKPDFMenuTarget
- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;
    
    _selectedMenuItem = nil;
    return self;
}

- (RetainPtr<NSMenuItem>)selectedMenuItem
{
    return _selectedMenuItem.get();
}

- (void)contextMenuAction:(NSMenuItem *)sender
{
    _selectedMenuItem = sender;
}
@end // implementation WKPDFMenuTarget
#endif

#import <pal/mac/QuickLookUISoftLink.h>

namespace WebKit {
using namespace WebCore;

static inline bool expectsLegacyImplicitRubberBandControl()
{
    if (WTF::MacApplication::isSafari()) {
        const int32_t firstVersionOfSafariNotExpectingImplicitRubberBandControl = 0x021A0F00; // 538.15.0
        bool linkedAgainstSafariExpectingImplicitRubberBandControl = NSVersionOfLinkTimeLibrary("Safari") < firstVersionOfSafariNotExpectingImplicitRubberBandControl;
        return linkedAgainstSafariExpectingImplicitRubberBandControl;
    }

    const int32_t firstVersionOfWebKit2WithNoImplicitRubberBandControl = 0x021A0200; // 538.2.0
    int32_t linkedWebKit2Version = NSVersionOfLinkTimeLibrary("WebKit2");
    return linkedWebKit2Version != -1 && linkedWebKit2Version < firstVersionOfWebKit2WithNoImplicitRubberBandControl;
}

void WebPageProxy::platformInitialize()
{
    static bool clientExpectsLegacyImplicitRubberBandControl = expectsLegacyImplicitRubberBandControl();
    setShouldUseImplicitRubberBandControl(clientExpectsLegacyImplicitRubberBandControl);
}

String WebPageProxy::userAgentForURL(const URL&)
{
    return userAgent();
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return standardUserAgentWithApplicationName(applicationNameForUserAgent);
}

void WebPageProxy::getIsSpeaking(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    completionHandler([NSApp isSpeaking]);
}

void WebPageProxy::speak(const String& string)
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp speakString:string.createNSString().get()];
}

void WebPageProxy::stopSpeaking()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp stopSpeaking:nil];
}

void WebPageProxy::searchTheWeb(const String& string)
{
    RetainPtr pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard clearContents];
    if (sessionID().isEphemeral())
        [pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    [pasteboard addTypes:@[legacyStringPasteboardTypeSingleton()] owner:nil];
    [pasteboard setString:string.createNSString().get() forType:legacyStringPasteboardTypeSingleton()];

    NSPerformService(@"Search With %WebSearchProvider@", pasteboard.get());
}

void WebPageProxy::windowAndViewFramesChanged(const FloatRect& viewFrameInWindowCoordinates, const FloatPoint& accessibilityViewCoordinates)
{
    // In case the UI client overrides getWindowFrame(), we call it here to make sure we send the appropriate window frame.
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, viewFrameInWindowCoordinates, accessibilityViewCoordinates] (FloatRect windowFrameInScreenCoordinates) {
        RefPtr pageClient = this->pageClient();
        if (!pageClient)
            return;

        FloatRect windowFrameInUnflippedScreenCoordinates = protectedPageClient()->convertToUserSpace(windowFrameInScreenCoordinates);

        m_viewWindowCoordinates = makeUnique<ViewWindowCoordinates>();
        auto& coordinates = *m_viewWindowCoordinates;
        coordinates.windowFrameInScreenCoordinates = windowFrameInScreenCoordinates;
        coordinates.windowFrameInUnflippedScreenCoordinates = windowFrameInUnflippedScreenCoordinates;
        coordinates.viewFrameInWindowCoordinates = viewFrameInWindowCoordinates;
        coordinates.accessibilityViewCoordinates = accessibilityViewCoordinates;

        if (!hasRunningProcess())
            return;

        protectedLegacyMainFrameProcess()->sendWithAsyncReply(
            Messages::WebPage::WindowAndViewFramesChanged(*m_viewWindowCoordinates),
            [this, protectedThis, viewFrameInWindowCoordinates]() {
                updateMouseEventTargetAfterWindowAndViewFramesChanged(viewFrameInWindowCoordinates);
            },
            webPageIDInMainFrameProcess()
        );
    });
}

void WebPageProxy::updateMouseEventTargetAfterWindowAndViewFramesChanged(const FloatRect& viewFrameInWindowCoordinates)
{
    RetainPtr window = platformWindow();

    // The origin of mac coordinate system is the bottom left corner. We need to convert
    // the point to the web coordinate system which has an origin of the top left corner.
    auto macMouseLocationInWindow = [window mouseLocationOutsideOfEventStream];

    auto webMouseLocationInWindow = DoublePoint(macMouseLocationInWindow.x, [window frame].size.height - macMouseLocationInWindow.y);

    // do same conversion as above
    auto macMouseLocationInScreen = [NSEvent mouseLocation];
    auto webMouseLocationInScreen = DoublePoint(macMouseLocationInScreen.x, [[NSScreen mainScreen] frame].size.height - macMouseLocationInScreen.y);
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::UpdateMouseEventTargetAfterWindowAndViewFramesChanged(webMouseLocationInWindow, webMouseLocationInScreen), webPageIDInMainFrameProcess());
}

void WebPageProxy::setMainFrameIsScrollable(bool isScrollable)
{
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetMainFrameIsScrollable(isScrollable), webPageIDInMainFrameProcess());
}

void WebPageProxy::attributedSubstringForCharacterRangeAsync(const EditingRange& range, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction({ }, { });
        return;
    }

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::AttributedSubstringForCharacterRangeAsync(range), WTF::move(callbackFunction), webPageIDInMainFrameProcess());
}

static constexpr auto timeoutForPasteboardSyncIPC = 5_s;

String WebPageProxy::stringSelectionForPasteboard()
{
    if (!hasRunningProcess())
        return { };

    if (!editorState().selectionIsRange)
        return { };

    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::GetStringSelectionForPasteboard(), webPageIDInMainFrameProcess(), timeoutForPasteboardSyncIPC);
    auto [value] = sendResult.takeReplyOr(String { });
    return value;
}

RefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String& pasteboardType)
{
    if (!hasRunningProcess())
        return nullptr;

    if (!editorState().selectionIsRange)
        return nullptr;

    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::GetDataSelectionForPasteboard(pasteboardType), webPageIDInMainFrameProcess(), timeoutForPasteboardSyncIPC);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

bool WebPageProxy::readSelectionFromPasteboard(const String& pasteboardName)
{
    if (!hasRunningProcess())
        return false;

    if (auto replyID = grantAccessToCurrentPasteboardData(pasteboardName, [] () { }))
        protectedWebsiteDataStore()->protectedNetworkProcess()->protectedConnection()->waitForAsyncReplyAndDispatchImmediately<Messages::NetworkProcess::AllowFilesAccessFromWebProcess>(*replyID, 100_ms);

    const Seconds messageTimeout(20);
    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::ReadSelectionFromPasteboard(pasteboardName), webPageIDInMainFrameProcess(), messageTimeout);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::setPromisedDataForImage(IPC::Connection& connection, const String& pasteboardName, SharedMemory::Handle&& imageHandle, const String& filename, const String& extension,
    const String& title, const String& url, const String& visibleURL, SharedMemory::Handle&& archiveHandle, const String& originIdentifier)
{
    Ref process = WebProcessProxy::fromConnection(connection);
    MESSAGE_CHECK_URL(url);
    MESSAGE_CHECK_URL(visibleURL);
    MESSAGE_CHECK(extension == FileSystem::lastComponentOfPathIgnoringTrailingSlash(extension), connection);

    auto sharedMemoryImage = SharedMemory::map(WTF::move(imageHandle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryImage)
        return;
    auto imageBuffer = sharedMemoryImage->createSharedBuffer(sharedMemoryImage->size());

    RefPtr<FragmentedSharedBuffer> archiveBuffer;
    auto sharedMemoryArchive = SharedMemory::map(WTF::move(archiveHandle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryArchive)
        return;
    archiveBuffer = sharedMemoryArchive->createSharedBuffer(sharedMemoryArchive->size());
    if (RefPtr pageClient = this->pageClient())
        pageClient->setPromisedDataForImage(pasteboardName, WTF::move(imageBuffer), ResourceResponseBase::sanitizeSuggestedFilename(filename), extension, title, url, visibleURL, WTF::move(archiveBuffer), originIdentifier);
}

#endif

void WebPageProxy::setSmartInsertDeleteEnabled(bool isSmartInsertDeleteEnabled)
{
    if (m_isSmartInsertDeleteEnabled == isSmartInsertDeleteEnabled)
        return;

    TextChecker::setSmartInsertDeleteEnabled(isSmartInsertDeleteEnabled);
    m_isSmartInsertDeleteEnabled = isSmartInsertDeleteEnabled;
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetSmartInsertDeleteEnabled(isSmartInsertDeleteEnabled), webPageIDInMainFrameProcess());
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    if (RefPtr pageClient = this->pageClient()) {
        pageClient->didPerformDictionaryLookup(dictionaryPopupInfo);

        DictionaryLookup::showPopup(dictionaryPopupInfo, pageClient->protectedViewForPresentingRevealPopover().get(), [this](TextIndicator& textIndicator) {
            setTextIndicator(textIndicator, WebCore::TextIndicatorLifetime::Permanent);
        }, nullptr, [weakThis = WeakPtr { *this }] {
            if (!weakThis)
                return;
            weakThis->clearTextIndicatorWithAnimation(WebCore::TextIndicatorDismissalAnimation::None);
        });
    }
}

void WebPageProxy::registerWebProcessAccessibilityToken(std::span<const uint8_t> data)
{
    if (!hasRunningProcess())
        return;

    // Note: The WebFrameProxy with this FrameIdentifier might not exist in the UI process. See rdar://130998804.
    if (RefPtr pageClient = this->pageClient())
        pageClient->accessibilityWebProcessTokenReceived(data, protectedLegacyMainFrameProcess()->protectedConnection()->remoteProcessID());
}

void WebPageProxy::makeFirstResponder()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->makeFirstResponder();
}

void WebPageProxy::assistiveTechnologyMakeFirstResponder()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->assistiveTechnologyMakeFirstResponder();
}

bool WebPageProxy::useFormSemanticContext() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient && pageClient->useFormSemanticContext();
}

void WebPageProxy::semanticContextDidChange()
{
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SemanticContextDidChange(useFormSemanticContext()), webPageIDInMainFrameProcess());
}

WebCore::DestinationColorSpace WebPageProxy::colorSpace()
{
    return protectedPageClient()->colorSpace();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(WebCore::AccessibilityRemoteToken elementToken, WebCore::AccessibilityRemoteToken windowToken)
{
    if (!hasRunningProcess())
        return;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken), webPageIDInMainFrameProcess());
}

void WebPageProxy::executeSavedCommandBySelector(IPC::Connection& connection, const String& selector, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK_COMPLETION(isValidKeypressCommandName(selector), connection, completionHandler(false));

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler(false);
    completionHandler(pageClient->executeSavedCommandBySelector(selector));
}

bool WebPageProxy::shouldDelayWindowOrderingForEvent(const WebKit::WebMouseEvent& event)
{
    if (protectedLegacyMainFrameProcess()->state() != WebProcessProxy::State::Running)
        return false;

    const Seconds messageTimeout(3);
    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::ShouldDelayWindowOrderingEvent(event), webPageIDInMainFrameProcess(), messageTimeout);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebPageProxy::acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent& event)
{
    if (!hasRunningProcess())
        return false;

    Ref legacyMainFrameProcess = m_legacyMainFrameProcess;
    if (!legacyMainFrameProcess->hasConnection())
        return false;

    if (shouldAvoidSynchronouslyWaitingToPreventDeadlock())
        return false;

    legacyMainFrameProcess->send(Messages::WebPage::RequestAcceptsFirstMouse(eventNumber, event), webPageIDInMainFrameProcess(), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    bool receivedReply = legacyMainFrameProcess->protectedConnection()->waitForAndDispatchImmediately<Messages::WebPageProxy::HandleAcceptsFirstMouse>(webPageIDInMainFrameProcess(), 3_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives) == IPC::Error::NoError;

    if (!receivedReply)
        return false;

    return m_acceptsFirstMouse;
}

void WebPageProxy::handleAcceptsFirstMouse(bool acceptsFirstMouse)
{
    m_acceptsFirstMouse = acceptsFirstMouse;
}

void WebPageProxy::setAutomaticallyAdjustsContentInsets(bool automaticallyAdjustsContentInsets)
{
    if (m_automaticallyAdjustsContentInsets == automaticallyAdjustsContentInsets)
        return;

    m_automaticallyAdjustsContentInsets = automaticallyAdjustsContentInsets;
    updateContentInsetsIfAutomatic();
}

void WebPageProxy::updateContentInsetsIfAutomatic()
{
    if (!m_automaticallyAdjustsContentInsets)
        return;

    m_internals->pendingObscuredContentInsets = std::nullopt;

    scheduleSetObscuredContentInsetsDispatch();
}

void WebPageProxy::setOverflowHeightForTopScrollEdgeEffect(double value)
{
    if (m_overflowHeightForTopScrollEdgeEffect == value)
        return;

    m_overflowHeightForTopScrollEdgeEffect = value;

    protectedLegacyMainFrameProcess()->send(Messages::WebPage::SetOverflowHeightForTopScrollEdgeEffect(value), webPageIDInMainFrameProcess());
}

void WebPageProxy::setObscuredContentInsetsAsync(const FloatBoxExtent& obscuredContentInsets)
{
    m_internals->pendingObscuredContentInsets = obscuredContentInsets;
    scheduleSetObscuredContentInsetsDispatch();
}

FloatBoxExtent WebPageProxy::pendingOrActualObscuredContentInsets() const
{
    return m_internals->pendingObscuredContentInsets.value_or(m_internals->obscuredContentInsets);
}

void WebPageProxy::scheduleSetObscuredContentInsetsDispatch()
{
    if (m_didScheduleSetObscuredContentInsetsDispatch)
        return;

    m_didScheduleSetObscuredContentInsetsDispatch = true;

    callOnMainRunLoop([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->dispatchSetObscuredContentInsets();
    });
}

void WebPageProxy::dispatchSetObscuredContentInsets()
{
    bool wasScheduled = std::exchange(m_didScheduleSetObscuredContentInsetsDispatch, false);
    if (!wasScheduled)
        return;

    if (!m_internals->pendingObscuredContentInsets) {
        if (!m_automaticallyAdjustsContentInsets)
            return;

        if (RefPtr pageClient = this->pageClient()) {
            if (auto automaticTopInset = pageClient->computeAutomaticTopObscuredInset()) {
                m_internals->pendingObscuredContentInsets = m_internals->obscuredContentInsets;
                m_internals->pendingObscuredContentInsets->setTop(*automaticTopInset);
            }
        }

        if (!m_internals->pendingObscuredContentInsets)
            m_internals->pendingObscuredContentInsets = FloatBoxExtent { };
    }

    setObscuredContentInsets(*m_internals->pendingObscuredContentInsets);
    m_internals->pendingObscuredContentInsets = std::nullopt;
}

void WebPageProxy::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->setRemoteLayerTreeRootNode(rootNode);
    m_frozenRemoteLayerTreeHost = nullptr;
}

CALayer *WebPageProxy::acceleratedCompositingRootLayer() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->acceleratedCompositingRootLayer() : nullptr;
}

RetainPtr<CALayer> WebPageProxy::protectedAcceleratedCompositingRootLayer() const
{
    return acceleratedCompositingRootLayer();
}

CALayer *WebPageProxy::headerBannerLayer() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->headerBannerLayer() : nullptr;
}

RetainPtr<CALayer> WebPageProxy::protectedHeaderBannerLayer() const
{
    return headerBannerLayer();
}

CALayer *WebPageProxy::footerBannerLayer() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->footerBannerLayer() : nullptr;
}

RetainPtr<CALayer> WebPageProxy::protectedFooterBannerLayer() const
{
    return footerBannerLayer();
}

int WebPageProxy::headerBannerHeight() const
{
    if (RetainPtr headerBannerLayer = this->headerBannerLayer())
        return headerBannerLayer.get().frame.size.height;
    return 0;
}

int WebPageProxy::footerBannerHeight() const
{
    if (RetainPtr footerBannerLayer = this->footerBannerLayer())
        return footerBannerLayer.get().frame.size.height;
    return 0;
}

static NSString *temporaryPDFDirectoryPath()
{
    static NeverDestroyed path = [] {
        RetainPtr temporaryDirectory = NSTemporaryDirectory();
        RetainPtr temporaryDirectoryTemplate = [temporaryDirectory stringByAppendingPathComponent:@"WebKitPDFs-XXXXXX"];
        CString templateRepresentation = [temporaryDirectoryTemplate fileSystemRepresentation];
        if (mkdtemp(templateRepresentation.mutableSpanIncludingNullTerminator().data()))
            return adoptNS((NSString *)[[[NSFileManager defaultManager] stringWithFileSystemRepresentation:templateRepresentation.data() length:templateRepresentation.length()] copy]);
        return RetainPtr<NSString> { };
    }();
    return path.get().get();
}

static RetainPtr<NSString> pathToPDFOnDisk(const String& suggestedFilename)
{
    RetainPtr pdfDirectoryPath = temporaryPDFDirectoryPath();
    if (!pdfDirectoryPath) {
        WTFLogAlways("Cannot create temporary PDF download directory.");
        return nil;
    }

    // The NSFileManager expects a path string, while NSWorkspace uses file URLs, and will decode any percent encoding
    // in its passed URLs before loading from disk. Create the files using decoded file paths so they match up.
    RetainPtr path = [[pdfDirectoryPath stringByAppendingPathComponent:suggestedFilename.createNSString().get()] stringByRemovingPercentEncoding];

    RetainPtr fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path.get()]) {
        auto [fileHandle, pathTemplateRepresentation] = FileSystem::createTemporaryFileInDirectory(pdfDirectoryPath.get(), makeString('-', suggestedFilename));
        if (!fileHandle) {
            WTFLogAlways("Cannot create PDF file in the temporary directory (%s).", suggestedFilename.utf8().data());
            return nil;
        }

        fileHandle = { };
        path = [fileManager stringWithFileSystemRepresentation:pathTemplateRepresentation.data() length:pathTemplateRepresentation.length()];
    }

    return path;
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, FrameInfoData&& frameInfo, std::span<const uint8_t> data)
{
    if (data.empty()) {
        WTFLogAlways("Cannot save empty PDF file to the temporary directory.");
        return;
    }

    auto sanitizedFilename = ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename);
    if (!sanitizedFilename.endsWithIgnoringASCIICase(".pdf"_s)) {
        WTFLogAlways("Cannot save file without .pdf extension to the temporary directory.");
        return;
    }
    RetainPtr nsPath = pathToPDFOnDisk(sanitizedFilename);

    if (!nsPath)
        return;

    auto permissions = adoptNS([[NSNumber alloc] initWithInt:S_IRUSR]);
    auto fileAttributes = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:permissions.get(), NSFilePosixPermissions, nil]);
    RetainPtr nsData = toNSDataNoCopy(data, FreeWhenDone::No);

    if (![[NSFileManager defaultManager] createFileAtPath:nsPath.get() contents:nsData.get() attributes:fileAttributes.get()]) {
        WTFLogAlways("Cannot create PDF file in the temporary directory (%s).", sanitizedFilename.utf8().data());
        return;
    }
    auto originatingURLString = frameInfo.request.url().string();
    FileSystem::setMetadataURL(nsPath.get(), originatingURLString);

    auto pdfFileURL = URL::fileURLWithFileSystemPath(String(nsPath.get()));
    m_uiClient->confirmPDFOpening(*this, pdfFileURL, WTF::move(frameInfo), [pdfFileURL] (bool allowed) {
        if (!allowed)
            return;
        [[NSWorkspace sharedWorkspace] openURL:pdfFileURL.createNSURL().get()];
    });
}

#if ENABLE(PDF_PLUGIN)
void WebPageProxy::showPDFContextMenu(const WebKit::PDFContextMenu& contextMenu, PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID, CompletionHandler<void(std::optional<int32_t>&&)>&& completionHandler)
{
    if (!contextMenu.items.size())
        return completionHandler(std::nullopt);

    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return completionHandler(std::nullopt);
    
    RetainPtr menuTarget = adoptNS([[WKPDFMenuTarget alloc] init]);
    RetainPtr nsMenu = adoptNS([[NSMenu alloc] init]);
    [nsMenu setAllowsContextMenuPlugIns:false];
    for (unsigned i = 0; i < contextMenu.items.size(); i++) {
        auto& item = contextMenu.items[i];
        auto isOpenWithDefaultViewerItem = item.action == WebCore::ContextMenuItemTagOpenWithDefaultApplication;

        if (item.separator == ContextMenuItemIsSeparator::Yes) {
            [nsMenu insertItem:[NSMenuItem separatorItem] atIndex:i];
            continue;
        }

        RetainPtr nsItem = adoptNS([[NSMenuItem alloc] init]);

        if (isOpenWithDefaultViewerItem) {
            RetainPtr defaultPDFViewerPath = [[[NSWorkspace sharedWorkspace] URLForApplicationToOpenContentType:UTTypePDF] path];
            RetainPtr defaultPDFViewerName = [[NSFileManager defaultManager] displayNameAtPath:defaultPDFViewerPath.get()];

            String itemTitle = contextMenuItemPDFOpenWithDefaultViewer(defaultPDFViewerName.get());
            [nsItem setTitle:itemTitle.createNSString().get()];
#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
            RetainPtr icon = [[NSWorkspace sharedWorkspace] iconForFile:defaultPDFViewerPath.get()];
            [icon setSize:NSMakeSize(16.f, 16.f)];
            [nsItem _setActionImage:icon.get()];
#endif
        } else
            [nsItem setTitle:item.title.createNSString().get()];

        [nsItem setEnabled:item.enabled == ContextMenuItemEnablement::Enabled];
        [nsItem setState:item.state];
#if ENABLE(CONTEXT_MENU_IMAGES_ON_MAC)
        if (![nsItem _hasActionImage])
            addImageToMenuItem(nsItem.get(), item.action, false);
#endif
        if (item.hasAction == ContextMenuItemHasAction::Yes) {
            [nsItem setTarget:menuTarget.get()];
            [nsItem setAction:@selector(contextMenuAction:)];
        }
        [nsItem setTag:item.tag];
        [nsMenu insertItem:nsItem.get() atIndex:i];
    }
    RetainPtr window = pageClient->platformWindow();
    auto location = [window convertRectFromScreen: { contextMenu.point, NSZeroSize }].origin;
    auto event = createSyntheticEventForContextMenu(location);

    RetainPtr<NSView> view = window.get().contentView;
    [NSMenu popUpContextMenu:nsMenu.get() withEvent:event.get() forView:view.get()];

    if (RetainPtr selectedMenuItem = [menuTarget selectedMenuItem]) {
        NSInteger tag = selectedMenuItem.get().tag;
        if (contextMenu.openInDefaultViewerTag == tag)
            pdfOpenWithPreview(identifier, frameID);
        return completionHandler(tag);
    }
    completionHandler(std::nullopt);
}
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPageProxy::showTelephoneNumberMenu(const String& telephoneNumber, const WebCore::IntPoint& point, const WebCore::IntRect& rect)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    RetainPtr menu = menuForTelephoneNumber(telephoneNumber, pageClient->protectedViewForPresentingRevealPopover().get(), rect);
    pageClient->showPlatformContextMenu(menu.get(), point);
}
#endif

CGRect WebPageProxy::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->boundsOfLayerInLayerBackedWindowCoordinates(layer) : CGRect { };
}

void WebPageProxy::didUpdateEditorState(const EditorState& oldEditorState, const EditorState& newEditorState)
{
    bool couldChangeSecureInputState = newEditorState.isInPasswordField != oldEditorState.isInPasswordField || oldEditorState.selectionIsNone;
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !newEditorState.selectionIsNone) {
        if (RefPtr pageClient = this->pageClient())
            pageClient->updateSecureInputState();
    }
    
    if (newEditorState.shouldIgnoreSelectionChanges)
        return;

    updateFontAttributesAfterEditorStateChange();
    if (RefPtr pageClient = this->pageClient())
        pageClient->selectionDidChange();
}

void WebPageProxy::startWindowDrag()
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->startWindowDrag();
}

NSWindow *WebPageProxy::platformWindow()
{
    RefPtr pageClient = m_pageClient.get();
    return pageClient ? pageClient->platformWindow() : nullptr;
}

RetainPtr<NSWindow> WebPageProxy::protectedPlatformWindow()
{
    return platformWindow();
}

void WebPageProxy::rootViewToWindow(const WebCore::IntRect& viewRect, WebCore::IntRect& windowRect)
{
    RefPtr pageClient = this->pageClient();
    windowRect = pageClient ? pageClient->rootViewToWindow(viewRect) : WebCore::IntRect { };
}

void WebPageProxy::showValidationMessage(const IntRect& anchorClientRect, String&& message)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    m_validationBubble = protectedPageClient()->createValidationBubble(WTF::move(message), { protectedPreferences()->minimumFontSize() });
    RefPtr { m_validationBubble }->showRelativeTo(anchorClientRect);
}

RetainPtr<NSView> WebPageProxy::inspectorAttachmentView()
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->inspectorAttachmentView() : nullptr;
}

_WKRemoteObjectRegistry *WebPageProxy::remoteObjectRegistry()
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->remoteObjectRegistry() : nullptr;
}

#if ENABLE(CONTEXT_MENUS)

NSMenu *WebPageProxy::activeContextMenu() const
{
    if (RefPtr activeContextMenu = m_activeContextMenu)
        return activeContextMenu->platformMenu();
    return nil;
}

RetainPtr<NSEvent> WebPageProxy::createSyntheticEventForContextMenu(FloatPoint location) const
{
    RetainPtr window = protectedPageClient()->platformWindow();
    return [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:location modifierFlags:0 timestamp:0 windowNumber:[window windowNumber] context:nil eventNumber:0 clickCount:0 pressure:0];
}

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData& item, CompletionHandler<void()>&& completionHandler)
{
    if (item.action() == ContextMenuItemTagPaste)
        grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral, WTF::move(completionHandler));
    else
        completionHandler();
}

#endif

std::optional<IPC::AsyncReplyID> WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory pasteAccessCategory, CompletionHandler<void()>&& completionHandler, std::optional<FrameIdentifier> frameID)
{
    switch (pasteAccessCategory) {
    case DOMPasteAccessCategory::General:
        return grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral, WTF::move(completionHandler), frameID);
    case DOMPasteAccessCategory::Fonts:
        return grantAccessToCurrentPasteboardData(NSPasteboardNameFont, WTF::move(completionHandler), frameID);
    }
}

RetainPtr<NSView> WebPageProxy::Internals::platformView() const
{
    RefPtr pageClient = protectedPage()->pageClient();
    if (!pageClient)
        return nullptr;
    RetainPtr window = pageClient->platformWindow();
    return [window contentView];
}

#if ENABLE(PDF_PLUGIN)

void WebPageProxy::createPDFHUD(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID, const WebCore::IntRect& rect)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->createPDFHUD(identifier, frameID, rect);
}

void WebPageProxy::removePDFHUD(PDFPluginIdentifier identifier)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->removePDFHUD(identifier);
}

void WebPageProxy::updatePDFHUDLocation(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->updatePDFHUDLocation(identifier, rect);
}

void WebPageProxy::pdfZoomIn(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID)
{
    sendToProcessContainingFrame(frameID, Messages::WebPage::ZoomPDFIn(identifier));
}

void WebPageProxy::pdfZoomOut(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID)
{
    sendToProcessContainingFrame(frameID, Messages::WebPage::ZoomPDFOut(identifier));
}

void WebPageProxy::pdfSaveToPDF(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::SavePDF(identifier), Messages::WebPage::SavePDF::Reply { [this, protectedThis = Ref { *this }] (String&& suggestedFilename, URL&& originatingURL, std::span<const uint8_t> dataReference) {
        savePDFToFileInDownloadsFolder(WTF::move(suggestedFilename), WTF::move(originatingURL), dataReference);
    } });
}

// FIXME: This is a misnomer since we conflate Preview.app with the default PDF viewer. Consider renaming.
void WebPageProxy::pdfOpenWithPreview(PDFPluginIdentifier identifier, WebCore::FrameIdentifier frameID)
{
    sendWithAsyncReplyToProcessContainingFrame(frameID, Messages::WebPage::OpenPDFWithPreview(identifier), Messages::WebPage::OpenPDFWithPreview::Reply { [this, protectedThis = Ref { *this }](String&& suggestedFilename, std::optional<FrameInfoData>&& frameInfo, std::span<const uint8_t> data) {
        if (!frameInfo)
            return;
        savePDFToTemporaryFolderAndOpenWithNativeApplication(WTF::move(suggestedFilename), WTF::move(*frameInfo), data);
    } });
}

#endif // #if ENABLE(PDF_PLUGIN)

void WebPageProxy::changeUniversalAccessZoomFocus(const WebCore::IntRect& viewRect, const WebCore::IntRect& selectionRect)
{
    WebCore::changeUniversalAccessZoomFocus(viewRect, selectionRect);
}

void WebPageProxy::showFontPanel()
{
    // FIXME (rdar://21577518): Enable the system font panel for all web views, not just editable ones.
    if (m_isEditable)
        [[NSFontManager sharedFontManager] orderFrontFontPanel:nil];
}

void WebPageProxy::showStylesPanel()
{
    if (m_isEditable)
        [[NSFontManager sharedFontManager] orderFrontStylesPanel:nil];
}

void WebPageProxy::showColorPanel()
{
    if (m_isEditable)
        [[NSApplication sharedApplication] orderFrontColorPanel:nil];
}

Color WebPageProxy::platformUnderPageBackgroundColor() const
{
#if ENABLE(DARK_MODE_CSS)
    return WebCore::roundAndClampToSRGBALossy(RetainPtr { NSColor.controlBackgroundColor.CGColor }.get());
#else
    return WebCore::Color::white;
#endif
}

void WebPageProxy::beginPreviewPanelControl(QLPreviewPanel *panel)
{
#if ENABLE(IMAGE_ANALYSIS)
    [m_quickLookPreviewController beginControl:panel];
#endif
}

void WebPageProxy::endPreviewPanelControl(QLPreviewPanel *panel)
{
#if ENABLE(IMAGE_ANALYSIS)
    if (auto controller = std::exchange(m_quickLookPreviewController, nil))
        [controller endControl:panel];
#endif
}

void WebPageProxy::closeSharedPreviewPanelIfNecessary()
{
#if ENABLE(IMAGE_ANALYSIS)
    [m_quickLookPreviewController closePanelIfNecessary];
#endif
}

#if ENABLE(IMAGE_ANALYSIS)

void WebPageProxy::handleContextMenuLookUpImage()
{
    auto result = internals().activeContextMenuContextData.webHitTestResultData().value();
    RefPtr imageBitmap = result.imageBitmap;
    if (!imageBitmap)
        return;

    showImageInQuickLookPreviewPanel(*imageBitmap, result.tooltipText, URL { result.absoluteImageURL }, QuickLookPreviewActivity::VisualSearch);
}

void WebPageProxy::showImageInQuickLookPreviewPanel(ShareableBitmap& imageBitmap, const String& tooltip, const URL& imageURL, QuickLookPreviewActivity activity)
{
    if (!PAL::isQuickLookUIFrameworkAvailable() || !PAL::getQLPreviewPanelClassSingleton() || ![PAL::getQLItemClassSingleton() instancesRespondToSelector:@selector(initWithDataProvider:contentType:previewTitle:)])
        return;

    RetainPtr image = imageBitmap.createPlatformImage(DontCopyBackingStore);
    if (!image)
        return;

    auto imageData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    auto destination = adoptCF(CGImageDestinationCreateWithData(imageData.get(), (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr));
    if (!destination)
        return;

    CGImageDestinationAddImage(destination.get(), image.get(), nil);
    if (!CGImageDestinationFinalize(destination.get()))
        return;

    m_quickLookPreviewController = adoptNS([[WKQuickLookPreviewController alloc] initWithPage:*this imageData:(__bridge NSData *)imageData.get() title:tooltip.createNSString().get() imageURL:imageURL.createNSURL().get() activity:activity]);

    // When presenting the shared QLPreviewPanel, QuickLook will search the responder chain for a suitable panel controller.
    // Make sure that we (by default) start the search at the web view, which knows how to vend the Visual Search preview
    // controller as a delegate and data source for the preview panel.
    if (RefPtr pageClient = this->pageClient())
        pageClient->makeFirstResponder();

    RetainPtr previewPanel = [PAL::getQLPreviewPanelClassSingleton() sharedPreviewPanel];
    [previewPanel makeKeyAndOrderFront:nil];

    if (![m_quickLookPreviewController isControlling:previewPanel.get()]) {
        // The WebKit client may have overridden QLPreviewPanelController methods on the view without calling into the superclass.
        // In this case, hand over control to the client and clear out our state eagerly, since we don't expect any further delegate
        // calls once the preview panel is dismissed.
        m_quickLookPreviewController.clear();
    }
}

#endif // ENABLE(IMAGE_ANALYSIS)

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

void WebPageProxy::handleContextMenuCopySubject(const String& preferredMIMEType)
{
    RefPtr activeContextMenu = m_activeContextMenu;
    if (!activeContextMenu)
        return;

    auto image = activeContextMenu->imageForCopySubject();
    if (!image)
        return;

    auto [data, type] = imageDataForRemoveBackground(image.get(), preferredMIMEType.createCFString().get());
    if (!data)
        return;

    RetainPtr<NSPasteboard> pasteboard = NSPasteboard.generalPasteboard;
    RetainPtr pasteboardType = bridge_cast(type.get());
    [pasteboard clearContents];
    if (sessionID().isEphemeral())
        [pasteboard _setExpirationDate:[NSDate dateWithTimeIntervalSinceNow:pasteboardExpirationDelay.seconds()]];
    [pasteboard addTypes:@[pasteboardType.get()] owner:nil];
    [pasteboard setData:data.get() forType:pasteboardType.get()];
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(WRITING_TOOLS)

bool WebPageProxy::shouldEnableWritingToolsRequestedTool(WebCore::WritingTools::RequestedTool tool) const
{
    WTRequestedTool requestedTool = convertToPlatformRequestedTool(tool);

    if (requestedTool == WTRequestedToolIndex)
        return true;

    auto& editorState = this->editorState();
    bool editorStateIsContentEditable = editorState.isContentEditable;

    if (requestedTool == WTRequestedToolCompose)
        return editorStateIsContentEditable;

    if (editorStateIsContentEditable)
        return editorState.hasPostLayoutData() && !editorState.postLayoutData->paragraphContextForCandidateRequest.isEmpty();

    return true;
}

void WebPageProxy::handleContextMenuWritingTools(const WebContextMenuItemData& item)
{
    WTRequestedTool tool = WTRequestedToolIndex;
    switch (item.action()) {
    case ContextMenuItemTagWritingTools:
        break;
    case ContextMenuItemTagProofread:
        tool = WTRequestedToolProofread;
        break;
    case ContextMenuItemTagRewrite:
        tool = WTRequestedToolRewrite;
        break;
    case ContextMenuItemTagSummarize:
        tool = WTRequestedToolTransformSummary;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    handleContextMenuWritingTools(convertToWebRequestedTool(tool));
}

void WebPageProxy::handleContextMenuWritingTools(WebCore::WritingTools::RequestedTool tool)
{
    auto& editorState = this->editorState();
    if (!editorState.hasPostLayoutData()) {
        ASSERT_NOT_REACHED();
        return;
    }
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    auto selectionRect = editorState.postLayoutData->selectionBoundingRect;
    pageClient->handleContextMenuWritingTools(tool, selectionRect);
}

#endif

WebCore::FloatRect WebPageProxy::selectionBoundingRectInRootViewCoordinates() const
{
    if (editorState().selectionIsNone)
        return { };

    if (!editorState().hasPostLayoutData())
        return { };

    auto bounds = WebCore::FloatRect { editorState().postLayoutData->selectionBoundingRect };
    bounds.move(internals().scrollPositionDuringLastEditorStateUpdate - mainFrameScrollPosition());
    return bounds;
}

WebContentMode WebPageProxy::effectiveContentModeAfterAdjustingPolicies(API::WebsitePolicies& policies, const WebCore::ResourceRequest& request)
{
    Ref preferences = m_preferences;
    if (preferences->needsSiteSpecificQuirks()) {
        if (policies.customUserAgent().isEmpty()) {
            // FIXME (263619): This is done here for adding a UA override to tiktok. Should be in a common location.
            // needsCustomUserAgentOverride() is currently very generic on purpose.
            // In the future we want to pass more parameters for targeting specific domains.
            // Domain-specific quirks should take precedence over page-level defaults
            if (auto customUserAgentForQuirk = Quirks::needsCustomUserAgentOverride(request.url(), m_applicationNameForUserAgent, m_userAgent))
                policies.setCustomUserAgent(WTF::move(*customUserAgentForQuirk));
            const auto& customUserAgentAsSiteSpecificQuirks = policies.customUserAgentAsSiteSpecificQuirks();
            if (!customUserAgentAsSiteSpecificQuirks.isEmpty()) {
                if (auto correctedCustomUserAgentAsSiteSpecificQuirk = Quirks::needsCustomUserAgentOverride(request.url(), m_applicationNameForUserAgent, customUserAgentAsSiteSpecificQuirks))
                    policies.setCustomUserAgentAsSiteSpecificQuirks(WTF::move(*correctedCustomUserAgentAsSiteSpecificQuirk));
            }
        }
    }

    return WebContentMode::Recommended;
}

#if ENABLE(POINTER_LOCK)

void WebPageProxy::platformLockPointer()
{
    CGDisplayHideCursor(CGMainDisplayID());
    CGAssociateMouseAndMouseCursorPosition(false);
}

void WebPageProxy::platformUnlockPointer()
{
    CGAssociateMouseAndMouseCursorPosition(true);
    CGDisplayShowCursor(CGMainDisplayID());
}

#endif

} // namespace WebKit

#endif // PLATFORM(MAC)

#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
