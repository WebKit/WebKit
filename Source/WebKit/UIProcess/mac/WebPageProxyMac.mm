/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#import "RemoteLayerTreeHost.h"
#import "RemoteLayerTreeNode.h"
#import "TextChecker.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKQuickLookPreviewController.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuProxyMac.h"
#import "WebPageMessages.h"
#import "WebPageProxyInternals.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebProcessProxy.h"
#import <WebCore/AttributedString.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragItem.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/UniversalAccessZoom.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <mach-o/dyld.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>

#define MESSAGE_CHECK(assertion, connection) MESSAGE_CHECK_BASE(assertion, connection)
#define MESSAGE_CHECK_COMPLETION(assertion, connection, completion) MESSAGE_CHECK_COMPLETION_BASE(assertion, connection, completion)
#define MESSAGE_CHECK_URL(process, url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(process, url), process->connection())
#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, process().connection(), returnValue)

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

#if ENABLE(PDF_PLUGIN)
@interface WKPDFMenuTarget : NSObject {
    NSMenuItem *_selectedMenuItem;
}
- (NSMenuItem *)selectedMenuItem;
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

- (NSMenuItem *)selectedMenuItem
{
    return _selectedMenuItem;
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
    [NSApp speakString:string];
}

void WebPageProxy::stopSpeaking()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp stopSpeaking:nil];
}

void WebPageProxy::searchTheWeb(const String& string)
{
    NSPasteboard *pasteboard = [NSPasteboard pasteboardWithUniqueName];
    [pasteboard declareTypes:@[legacyStringPasteboardType()] owner:nil];
    [pasteboard setString:string forType:legacyStringPasteboardType()];
    
    NSPerformService(@"Search With %WebSearchProvider@", pasteboard);
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

        protectedLegacyMainFrameProcess()->send(Messages::WebPage::WindowAndViewFramesChanged(*m_viewWindowCoordinates), webPageIDInMainFrameProcess());
    });
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

    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::AttributedSubstringForCharacterRangeAsync(range), WTFMove(callbackFunction), webPageIDInMainFrameProcess());
}

String WebPageProxy::stringSelectionForPasteboard()
{
    if (!hasRunningProcess())
        return { };
    
    const Seconds messageTimeout(20);
    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::GetStringSelectionForPasteboard(), webPageIDInMainFrameProcess(), messageTimeout);
    auto [value] = sendResult.takeReplyOr(String { });
    return value;
}

RefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String& pasteboardType)
{
    if (!hasRunningProcess())
        return nullptr;

    const Seconds messageTimeout(20);
    auto sendResult = protectedLegacyMainFrameProcess()->sendSync(Messages::WebPage::GetDataSelectionForPasteboard(pasteboardType), webPageIDInMainFrameProcess(), messageTimeout);
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
    Ref process = *downcast<WebProcessProxy>(AuxiliaryProcessProxy::fromConnection(connection));
    MESSAGE_CHECK_URL(process, url);
    MESSAGE_CHECK_URL(process, visibleURL);
    MESSAGE_CHECK(extension == FileSystem::lastComponentOfPathIgnoringTrailingSlash(extension), connection);

    auto sharedMemoryImage = SharedMemory::map(WTFMove(imageHandle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryImage)
        return;
    auto imageBuffer = sharedMemoryImage->createSharedBuffer(sharedMemoryImage->size());

    RefPtr<FragmentedSharedBuffer> archiveBuffer;
    auto sharedMemoryArchive = SharedMemory::map(WTFMove(archiveHandle), SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryArchive)
        return;
    archiveBuffer = sharedMemoryArchive->createSharedBuffer(sharedMemoryArchive->size());
    if (RefPtr pageClient = this->pageClient())
        pageClient->setPromisedDataForImage(pasteboardName, WTFMove(imageBuffer), ResourceResponseBase::sanitizeSuggestedFilename(filename), extension, title, url, visibleURL, WTFMove(archiveBuffer), originIdentifier);
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
    if (RefPtr pageClient = this->pageClient())
        pageClient->didPerformDictionaryLookup(dictionaryPopupInfo);
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

void WebPageProxy::registerUIProcessAccessibilityTokens(std::span<const uint8_t> elementToken, std::span<const uint8_t> windowToken)
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
    m_automaticallyAdjustsContentInsets = automaticallyAdjustsContentInsets;
    updateContentInsetsIfAutomatic();
}

void WebPageProxy::updateContentInsetsIfAutomatic()
{
    if (!m_automaticallyAdjustsContentInsets)
        return;

    m_pendingTopContentInset = std::nullopt;

    scheduleSetTopContentInsetDispatch();
}

void WebPageProxy::setTopContentInsetAsync(float contentInset)
{
    m_pendingTopContentInset = contentInset;
    scheduleSetTopContentInsetDispatch();
}

float WebPageProxy::pendingOrActualTopContentInset() const
{
    return m_pendingTopContentInset.value_or(m_topContentInset);
}

void WebPageProxy::scheduleSetTopContentInsetDispatch()
{
    if (m_didScheduleSetTopContentInsetDispatch)
        return;

    m_didScheduleSetTopContentInsetDispatch = true;

    callOnMainRunLoop([weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        weakThis->dispatchSetTopContentInset();
    });
}

void WebPageProxy::dispatchSetTopContentInset()
{
    bool wasScheduled = std::exchange(m_didScheduleSetTopContentInsetDispatch, false);
    if (!wasScheduled)
        return;

    if (!m_pendingTopContentInset) {
        if (!m_automaticallyAdjustsContentInsets)
            return;

        if (RefPtr pageClient = this->pageClient())
            m_pendingTopContentInset = pageClient->computeAutomaticTopContentInset();

        if (!m_pendingTopContentInset)
            m_pendingTopContentInset = 0;
    }

    setTopContentInset(*m_pendingTopContentInset);
    m_pendingTopContentInset = std::nullopt;
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

CALayer *WebPageProxy::headerBannerLayer() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->headerBannerLayer() : nullptr;
}

CALayer *WebPageProxy::footerBannerLayer() const
{
    RefPtr pageClient = this->pageClient();
    return pageClient ? pageClient->footerBannerLayer() : nullptr;
}

int WebPageProxy::headerBannerHeight() const
{
    if (auto *headerBannerLayer = this->headerBannerLayer())
        return headerBannerLayer.frame.size.height;
    return 0;
}

int WebPageProxy::footerBannerHeight() const
{
    if (auto *footerBannerLayer = this->footerBannerLayer())
        return footerBannerLayer.frame.size.height;
    return 0;
}

static NSString *temporaryPDFDirectoryPath()
{
    static NeverDestroyed path = [] {
        auto temporaryDirectoryTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitPDFs-XXXXXX"];
        CString templateRepresentation = [temporaryDirectoryTemplate fileSystemRepresentation];
        if (mkdtemp(templateRepresentation.mutableSpanIncludingNullTerminator().data()))
            return adoptNS([[[NSFileManager defaultManager] stringWithFileSystemRepresentation:templateRepresentation.data() length:templateRepresentation.length()] copy]);
        return RetainPtr<id> { };
    }();
    return path.get().get();
}

static NSString *pathToPDFOnDisk(const String& suggestedFilename)
{
    NSString *pdfDirectoryPath = temporaryPDFDirectoryPath();
    if (!pdfDirectoryPath) {
        WTFLogAlways("Cannot create temporary PDF download directory.");
        return nil;
    }

    // The NSFileManager expects a path string, while NSWorkspace uses file URLs, and will decode any percent encoding
    // in its passed URLs before loading from disk. Create the files using decoded file paths so they match up.
    NSString *path = [[pdfDirectoryPath stringByAppendingPathComponent:suggestedFilename] stringByRemovingPercentEncoding];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path]) {
        NSString *pathTemplatePrefix = [pdfDirectoryPath stringByAppendingPathComponent:@"XXXXXX-"];
        NSString *pathTemplate = [pathTemplatePrefix stringByAppendingString:suggestedFilename];
        CString pathTemplateRepresentation = [pathTemplate fileSystemRepresentation];

        int fd = mkstemps(pathTemplateRepresentation.mutableSpanIncludingNullTerminator().data(), pathTemplateRepresentation.length() - strlen([pathTemplatePrefix fileSystemRepresentation]) + 1);
        if (fd < 0) {
            WTFLogAlways("Cannot create PDF file in the temporary directory (%s).", suggestedFilename.utf8().data());
            return nil;
        }

        close(fd);
        path = [fileManager stringWithFileSystemRepresentation:pathTemplateRepresentation.data() length:pathTemplateRepresentation.length()];
    }

    return path;
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, FrameInfoData&& frameInfo, std::span<const uint8_t> data, const String& pdfUUID)
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
    auto nsPath = retainPtr(pathToPDFOnDisk(sanitizedFilename));

    if (!nsPath)
        return;

    auto permissions = adoptNS([[NSNumber alloc] initWithInt:S_IRUSR]);
    auto fileAttributes = adoptNS([[NSDictionary alloc] initWithObjectsAndKeys:permissions.get(), NSFilePosixPermissions, nil]);
    auto nsData = adoptNS([[NSData alloc] initWithBytesNoCopy:(void*)data.data() length:data.size() freeWhenDone:NO]);

    if (![[NSFileManager defaultManager] createFileAtPath:nsPath.get() contents:nsData.get() attributes:fileAttributes.get()]) {
        WTFLogAlways("Cannot create PDF file in the temporary directory (%s).", sanitizedFilename.utf8().data());
        return;
    }
    auto originatingURLString = frameInfo.request.url().string();
    FileSystem::setMetadataURL(nsPath.get(), originatingURLString);

    if (TemporaryPDFFileMap::isValidKey(pdfUUID))
        m_temporaryPDFFiles.add(pdfUUID, nsPath.get());

    auto pdfFileURL = URL::fileURLWithFileSystemPath(String(nsPath.get()));
    m_uiClient->confirmPDFOpening(*this, pdfFileURL, WTFMove(frameInfo), [pdfFileURL] (bool allowed) {
        if (!allowed)
            return;
        [[NSWorkspace sharedWorkspace] openURL:pdfFileURL];
    });
}

#if ENABLE(PDF_PLUGIN)
void WebPageProxy::showPDFContextMenu(const WebKit::PDFContextMenu& contextMenu, PDFPluginIdentifier identifier, CompletionHandler<void(std::optional<int32_t>&&)>&& completionHandler)
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
        
        if (item.separator == ContextMenuItemIsSeparator::Yes) {
            [nsMenu insertItem:[NSMenuItem separatorItem] atIndex:i];
            continue;
        }
        
        RetainPtr nsItem = adoptNS([[NSMenuItem alloc] init]);
        [nsItem setTitle:item.title];
        [nsItem setEnabled:item.enabled == ContextMenuItemEnablement::Enabled];
        [nsItem setState:item.state];
        if (item.hasAction == ContextMenuItemHasAction::Yes) {
            [nsItem setTarget:menuTarget.get()];
            [nsItem setAction:@selector(contextMenuAction:)];
        }
        [nsItem setTag:item.tag];
        [nsMenu insertItem:nsItem.get() atIndex:i];
    }
    NSWindow *window = pageClient->platformWindow();
    auto location = [window convertRectFromScreen: { contextMenu.point, NSZeroSize }].origin;
    auto event = createSyntheticEventForContextMenu(location);

    auto view = window.contentView;
    [NSMenu popUpContextMenu:nsMenu.get() withEvent:event.get() forView:view];

    if (auto selectedMenuItem = [menuTarget selectedMenuItem]) {
        NSInteger tag = selectedMenuItem.tag;
        if (contextMenu.openInPreviewTag && *contextMenu.openInPreviewTag == tag)
            pdfOpenWithPreview(identifier);
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

    RetainPtr menu = menuForTelephoneNumber(telephoneNumber, pageClient->viewForPresentingRevealPopover(), rect);
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

void WebPageProxy::rootViewToWindow(const WebCore::IntRect& viewRect, WebCore::IntRect& windowRect)
{
    RefPtr pageClient = this->pageClient();
    windowRect = pageClient ? pageClient->rootViewToWindow(viewRect) : WebCore::IntRect { };
}

void WebPageProxy::showValidationMessage(const IntRect& anchorClientRect, const String& message)
{
    RefPtr pageClient = this->pageClient();
    if (!pageClient)
        return;

    m_validationBubble = protectedPageClient()->createValidationBubble(message, { protectedPreferences()->minimumFontSize() });
    RefPtr { m_validationBubble }->showRelativeTo(anchorClientRect);
}

NSView *WebPageProxy::inspectorAttachmentView()
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
    return [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:location modifierFlags:0 timestamp:0 windowNumber:protectedPageClient()->platformWindow().windowNumber context:nil eventNumber:0 clickCount:0 pressure:0];
}

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData& item, CompletionHandler<void()>&& completionHandler)
{
    if (item.action() == ContextMenuItemTagPaste)
        grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral, WTFMove(completionHandler));
    else
        completionHandler();
}

#endif

std::optional<IPC::AsyncReplyID> WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory pasteAccessCategory, CompletionHandler<void()>&& completionHandler, std::optional<FrameIdentifier> frameID)
{
    switch (pasteAccessCategory) {
    case DOMPasteAccessCategory::General:
        return grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral, WTFMove(completionHandler), frameID);
    case DOMPasteAccessCategory::Fonts:
        return grantAccessToCurrentPasteboardData(NSPasteboardNameFont, WTFMove(completionHandler), frameID);
    }
}

RetainPtr<NSView> WebPageProxy::Internals::platformView() const
{
    RefPtr pageClient = protectedPage()->pageClient();
    return pageClient ? [pageClient->platformWindow() contentView] : nullptr;
}

#if ENABLE(PDF_PLUGIN)

void WebPageProxy::createPDFHUD(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    if (RefPtr pageClient = this->pageClient())
        pageClient->createPDFHUD(identifier, rect);
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

void WebPageProxy::pdfZoomIn(PDFPluginIdentifier identifier)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ZoomPDFIn(identifier), webPageIDInMainFrameProcess());
}

void WebPageProxy::pdfZoomOut(PDFPluginIdentifier identifier)
{
    protectedLegacyMainFrameProcess()->send(Messages::WebPage::ZoomPDFOut(identifier), webPageIDInMainFrameProcess());
}

void WebPageProxy::pdfSaveToPDF(PDFPluginIdentifier identifier)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::SavePDF(identifier), [this, protectedThis = Ref { *this }] (String&& suggestedFilename, URL&& originatingURL, std::span<const uint8_t> dataReference) {
        savePDFToFileInDownloadsFolder(WTFMove(suggestedFilename), WTFMove(originatingURL), dataReference);
    }, webPageIDInMainFrameProcess());
}

void WebPageProxy::pdfOpenWithPreview(PDFPluginIdentifier identifier)
{
    protectedLegacyMainFrameProcess()->sendWithAsyncReply(Messages::WebPage::OpenPDFWithPreview(identifier), [this, protectedThis = Ref { *this }] (String&& suggestedFilename, FrameInfoData&& frameInfo, std::span<const uint8_t> data, const String& pdfUUID) {
        savePDFToTemporaryFolderAndOpenWithNativeApplication(WTFMove(suggestedFilename), WTFMove(frameInfo), data, pdfUUID);
    }, webPageIDInMainFrameProcess());
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
    return WebCore::roundAndClampToSRGBALossy(NSColor.controlBackgroundColor.CGColor);
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

    showImageInQuickLookPreviewPanel(*imageBitmap, result.toolTipText, URL { result.absoluteImageURL }, QuickLookPreviewActivity::VisualSearch);
}

void WebPageProxy::showImageInQuickLookPreviewPanel(ShareableBitmap& imageBitmap, const String& tooltip, const URL& imageURL, QuickLookPreviewActivity activity)
{
    if (!PAL::isQuickLookUIFrameworkAvailable() || !PAL::getQLPreviewPanelClass() || ![PAL::getQLItemClass() instancesRespondToSelector:@selector(initWithDataProvider:contentType:previewTitle:)])
        return;

    auto image = imageBitmap.makeCGImage();
    if (!image)
        return;

    auto imageData = adoptCF(CFDataCreateMutable(kCFAllocatorDefault, 0));
    auto destination = adoptCF(CGImageDestinationCreateWithData(imageData.get(), (__bridge CFStringRef)UTTypePNG.identifier, 1, nullptr));
    if (!destination)
        return;

    CGImageDestinationAddImage(destination.get(), image.get(), nil);
    if (!CGImageDestinationFinalize(destination.get()))
        return;

    m_quickLookPreviewController = adoptNS([[WKQuickLookPreviewController alloc] initWithPage:*this imageData:(__bridge NSData *)imageData.get() title:tooltip imageURL:imageURL activity:activity]);

    // When presenting the shared QLPreviewPanel, QuickLook will search the responder chain for a suitable panel controller.
    // Make sure that we (by default) start the search at the web view, which knows how to vend the Visual Search preview
    // controller as a delegate and data source for the preview panel.
    if (RefPtr pageClient = this->pageClient())
        pageClient->makeFirstResponder();

    auto previewPanel = [PAL::getQLPreviewPanelClass() sharedPreviewPanel];
    [previewPanel makeKeyAndOrderFront:nil];

    if (![m_quickLookPreviewController isControlling:previewPanel]) {
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

    auto pasteboard = NSPasteboard.generalPasteboard;
    auto pasteboardType = (__bridge NSString *)type.get();
    [pasteboard declareTypes:@[pasteboardType] owner:nil];
    [pasteboard setData:data.get() forType:pasteboardType];
}

#endif // ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)

#if ENABLE(WRITING_TOOLS)

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

} // namespace WebKit

#endif // PLATFORM(MAC)

#undef MESSAGE_CHECK_WITH_RETURN_VALUE
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK_COMPLETION
#undef MESSAGE_CHECK
