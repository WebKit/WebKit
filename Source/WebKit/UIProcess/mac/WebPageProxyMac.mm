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
#import "DataReference.h"
#import "EditorState.h"
#import "FontInfo.h"
#import "FrameInfoData.h"
#import "ImageAnalysisUtilities.h"
#import "InsertTextOptions.h"
#import "MenuUtilities.h"
#import "NativeWebKeyboardEvent.h"
#import "PDFContextMenu.h"
#import "PageClient.h"
#import "PageClientImplMac.h"
#import "RemoteLayerTreeHost.h"
#import "StringUtilities.h"
#import "TextChecker.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKQuickLookPreviewController.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuProxyMac.h"
#import "WebPageMessages.h"
#import "WebPageProxyMessages.h"
#import "WebPreferencesKeys.h"
#import "WebProcessProxy.h"
#import <WebCore/AttributedString.h>
#import <WebCore/DestinationColorSpace.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragItem.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextAlternativeWithRange.h>
#import <WebCore/UniversalAccessZoom.h>
#import <WebCore/UserAgent.h>
#import <WebCore/ValidationBubble.h>
#import <mach-o/dyld.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <pal/spi/mac/NSMenuSPI.h>
#import <wtf/ProcessPrivilege.h>
#import <wtf/text/StringConcatenate.h>

#define MESSAGE_CHECK(assertion) MESSAGE_CHECK_BASE(assertion, process().connection())
#define MESSAGE_CHECK_URL(url) MESSAGE_CHECK_BASE(checkURLReceivedFromCurrentOrPreviousWebProcess(m_process, url), m_process->connection())
#define MESSAGE_CHECK_WITH_RETURN_VALUE(assertion, returnValue) MESSAGE_CHECK_WITH_RETURN_VALUE_BASE(assertion, process().connection(), returnValue)

@interface NSApplication ()
- (BOOL)isSpeaking;
- (void)speakString:(NSString *)string;
- (void)stopSpeaking:(id)sender;
@end

#if ENABLE(PDFKIT_PLUGIN)
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
    if (MacApplication::isSafari()) {
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
    [NSApp speakString:nsStringFromWebCoreString(string)];
}

void WebPageProxy::stopSpeaking()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    [NSApp stopSpeaking:nil];
}

void WebPageProxy::searchWithSpotlight(const String& string)
{
    [[NSWorkspace sharedWorkspace] showSearchResultsForQueryString:nsStringFromWebCoreString(string)];
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
    if (!hasRunningProcess())
        return;

    // In case the UI client overrides getWindowFrame(), we call it here to make sure we send the appropriate window frame.
    m_uiClient->windowFrame(*this, [this, protectedThis = Ref { *this }, viewFrameInWindowCoordinates, accessibilityViewCoordinates] (FloatRect windowFrameInScreenCoordinates) {
        FloatRect windowFrameInUnflippedScreenCoordinates = pageClient().convertToUserSpace(windowFrameInScreenCoordinates);
        send(Messages::WebPage::WindowAndViewFramesChanged(windowFrameInScreenCoordinates, windowFrameInUnflippedScreenCoordinates, viewFrameInWindowCoordinates, accessibilityViewCoordinates));
    });
}

void WebPageProxy::setMainFrameIsScrollable(bool isScrollable)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::SetMainFrameIsScrollable(isScrollable));
}

void WebPageProxy::attributedSubstringForCharacterRangeAsync(const EditingRange& range, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction({ }, { });
        return;
    }

    sendWithAsyncReply(Messages::WebPage::AttributedSubstringForCharacterRangeAsync(range), WTFMove(callbackFunction));
}

String WebPageProxy::stringSelectionForPasteboard()
{
    if (!hasRunningProcess())
        return { };
    
    const Seconds messageTimeout(20);
    auto sendResult = sendSync(Messages::WebPage::GetStringSelectionForPasteboard(), messageTimeout);
    auto [value] = sendResult.takeReplyOr(String { });
    return value;
}

RefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String& pasteboardType)
{
    if (!hasRunningProcess())
        return nullptr;

    const Seconds messageTimeout(20);
    auto sendResult = sendSync(Messages::WebPage::GetDataSelectionForPasteboard(pasteboardType), messageTimeout);
    auto [buffer] = sendResult.takeReplyOr(nullptr);
    return buffer;
}

bool WebPageProxy::readSelectionFromPasteboard(const String& pasteboardName)
{
    if (!hasRunningProcess())
        return false;

    grantAccessToCurrentPasteboardData(pasteboardName);

    const Seconds messageTimeout(20);
    auto sendResult = sendSync(Messages::WebPage::ReadSelectionFromPasteboard(pasteboardName), messageTimeout);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::setPromisedDataForImage(const String& pasteboardName, const SharedMemory::Handle& imageHandle, const String& filename, const String& extension,
    const String& title, const String& url, const String& visibleURL, const SharedMemory::Handle& archiveHandle, const String& originIdentifier)
{
    MESSAGE_CHECK_URL(url);
    MESSAGE_CHECK_URL(visibleURL);
    MESSAGE_CHECK(!imageHandle.isNull());

    auto sharedMemoryImage = SharedMemory::map(imageHandle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryImage)
        return;
    auto imageBuffer = sharedMemoryImage->createSharedBuffer(sharedMemoryImage->size());

    RefPtr<FragmentedSharedBuffer> archiveBuffer;
    if (!archiveHandle.isNull()) {
        auto sharedMemoryArchive = SharedMemory::map(archiveHandle, SharedMemory::Protection::ReadOnly);
        if (!sharedMemoryArchive)
            return;
        archiveBuffer = sharedMemoryArchive->createSharedBuffer(sharedMemoryArchive->size());
    }
    pageClient().setPromisedDataForImage(pasteboardName, WTFMove(imageBuffer), ResourceResponseBase::sanitizeSuggestedFilename(filename), extension, title, url, visibleURL, WTFMove(archiveBuffer), originIdentifier);
}

#endif

void WebPageProxy::uppercaseWord()
{
    send(Messages::WebPage::UppercaseWord());
}

void WebPageProxy::lowercaseWord()
{
    send(Messages::WebPage::LowercaseWord());
}

void WebPageProxy::capitalizeWord()
{
    send(Messages::WebPage::CapitalizeWord());
}

void WebPageProxy::setSmartInsertDeleteEnabled(bool isSmartInsertDeleteEnabled)
{
    if (m_isSmartInsertDeleteEnabled == isSmartInsertDeleteEnabled)
        return;

    TextChecker::setSmartInsertDeleteEnabled(isSmartInsertDeleteEnabled);
    m_isSmartInsertDeleteEnabled = isSmartInsertDeleteEnabled;
    send(Messages::WebPage::SetSmartInsertDeleteEnabled(isSmartInsertDeleteEnabled));
}

void WebPageProxy::didPerformDictionaryLookup(const DictionaryPopupInfo& dictionaryPopupInfo)
{
    pageClient().didPerformDictionaryLookup(dictionaryPopupInfo);
}
    
void WebPageProxy::registerWebProcessAccessibilityToken(const IPC::DataReference& data)
{
    if (!hasRunningProcess())
        return;
    
    pageClient().accessibilityWebProcessTokenReceived(data);
}    
    
void WebPageProxy::makeFirstResponder()
{
    pageClient().makeFirstResponder();
}

void WebPageProxy::assistiveTechnologyMakeFirstResponder()
{
    pageClient().assistiveTechnologyMakeFirstResponder();
}

WebCore::DestinationColorSpace WebPageProxy::colorSpace()
{
    return pageClient().colorSpace();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken));
}

void WebPageProxy::executeSavedCommandBySelector(const String& selector, CompletionHandler<void(bool)>&& completionHandler)
{
    MESSAGE_CHECK(isValidKeypressCommandName(selector));

    completionHandler(pageClient().executeSavedCommandBySelector(selector));
}

bool WebPageProxy::shouldDelayWindowOrderingForEvent(const WebKit::WebMouseEvent& event)
{
    if (process().state() != WebProcessProxy::State::Running)
        return false;

    const Seconds messageTimeout(3);
    auto sendResult = sendSync(Messages::WebPage::ShouldDelayWindowOrderingEvent(event), messageTimeout);
    auto [result] = sendResult.takeReplyOr(false);
    return result;
}

bool WebPageProxy::acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent& event)
{
    if (!hasRunningProcess())
        return false;

    if (!m_process->hasConnection())
        return false;

    send(Messages::WebPage::RequestAcceptsFirstMouse(eventNumber, event), IPC::SendOption::DispatchMessageEvenWhenWaitingForUnboundedSyncReply);
    bool receivedReply = m_process->connection()->waitForAndDispatchImmediately<Messages::WebPageProxy::HandleAcceptsFirstMouse>(webPageID(), 3_s, IPC::WaitForOption::InterruptWaitingIfSyncMessageArrives);

    if (!receivedReply)
        return false;

    return m_acceptsFirstMouse;
}

void WebPageProxy::handleAcceptsFirstMouse(bool acceptsFirstMouse)
{
    m_acceptsFirstMouse = acceptsFirstMouse;
}

void WebPageProxy::setRemoteLayerTreeRootNode(RemoteLayerTreeNode* rootNode)
{
    pageClient().setRemoteLayerTreeRootNode(rootNode);
    m_frozenRemoteLayerTreeHost = nullptr;
}

CALayer *WebPageProxy::acceleratedCompositingRootLayer() const
{
    return pageClient().acceleratedCompositingRootLayer();
}

static NSString *temporaryPDFDirectoryPath()
{
    static NeverDestroyed path = [] {
        auto temporaryDirectoryTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitPDFs-XXXXXX"];
        CString templateRepresentation = [temporaryDirectoryTemplate fileSystemRepresentation];
        if (mkdtemp(templateRepresentation.mutableData()))
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

    NSString *path = [pdfDirectoryPath stringByAppendingPathComponent:suggestedFilename];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:path]) {
        NSString *pathTemplatePrefix = [pdfDirectoryPath stringByAppendingPathComponent:@"XXXXXX-"];
        NSString *pathTemplate = [pathTemplatePrefix stringByAppendingString:suggestedFilename];
        CString pathTemplateRepresentation = [pathTemplate fileSystemRepresentation];

        int fd = mkstemps(pathTemplateRepresentation.mutableData(), pathTemplateRepresentation.length() - strlen([pathTemplatePrefix fileSystemRepresentation]) + 1);
        if (fd < 0) {
            WTFLogAlways("Cannot create PDF file in the temporary directory (%s).", suggestedFilename.utf8().data());
            return nil;
        }

        close(fd);
        path = [fileManager stringWithFileSystemRepresentation:pathTemplateRepresentation.data() length:pathTemplateRepresentation.length()];
    }

    return path;
}

void WebPageProxy::savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, FrameInfoData&& frameInfo, const IPC::DataReference& data, const String& pdfUUID)
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

#if ENABLE(PDFKIT_PLUGIN) && !ENABLE(UI_PROCESS_PDF_HUD)
void WebPageProxy::openPDFFromTemporaryFolderWithNativeApplication(FrameInfoData&& frameInfo, const String& pdfUUID)
{
    MESSAGE_CHECK(TemporaryPDFFileMap::isValidKey(pdfUUID));

    String pdfFilename = m_temporaryPDFFiles.get(pdfUUID);

    if (!pdfFilename.endsWithIgnoringASCIICase(".pdf"))
        return;

    auto pdfFileURL = URL::fileURLWithFileSystemPath(pdfFilename);
    m_uiClient->confirmPDFOpening(*this, pdfFileURL, WTFMove(frameInfo), [pdfFileURL] (bool allowed) {
        if (!allowed)
            return;
        [[NSWorkspace sharedWorkspace] openURL:pdfFileURL];
    });
}
#endif

#if ENABLE(PDFKIT_PLUGIN)
void WebPageProxy::showPDFContextMenu(const WebKit::PDFContextMenu& contextMenu, PDFPluginIdentifier identifier, CompletionHandler<void(std::optional<int32_t>&&)>&& completionHandler)
{
    if (!contextMenu.items.size())
        return completionHandler(std::nullopt);
    
    RetainPtr<WKPDFMenuTarget> menuTarget = adoptNS([[WKPDFMenuTarget alloc] init]);
    RetainPtr<NSMenu> nsMenu = adoptNS([[NSMenu alloc] init]);
    [nsMenu setAllowsContextMenuPlugIns:false];
    for (unsigned i = 0; i < contextMenu.items.size(); i++) {
        auto& item = contextMenu.items[i];
        
        if (item.separator) {
            [nsMenu insertItem:[NSMenuItem separatorItem] atIndex:i];
            continue;
        }
        
        RetainPtr<NSMenuItem> nsItem = adoptNS([[NSMenuItem alloc] init]);
        [nsItem setTitle:item.title];
        [nsItem setEnabled:item.enabled];
        [nsItem setState:item.state];
        if (item.hasAction) {
            [nsItem setTarget:menuTarget.get()];
            [nsItem setAction:@selector(contextMenuAction:)];
        }
        [nsItem setTag:item.tag];
        [nsMenu insertItem:nsItem.get() atIndex:i];
    }
    NSWindow *window = pageClient().platformWindow();
    auto location = [window convertRectFromScreen: { contextMenu.point, NSZeroSize }].origin;
    auto event = createSyntheticEventForContextMenu(location);

    auto view = window.contentView;
    [NSMenu popUpContextMenu:nsMenu.get() withEvent:event.get() forView:view];

    if (auto selectedMenuItem = [menuTarget selectedMenuItem]) {
        NSInteger tag = selectedMenuItem.tag;
#if ENABLE(UI_PROCESS_PDF_HUD)
        if (contextMenu.openInPreviewIndex && *contextMenu.openInPreviewIndex == tag)
            pdfOpenWithPreview(identifier);
#else
        UNUSED_PARAM(identifier);
#endif
        return completionHandler(tag);
    }
    completionHandler(std::nullopt);
}
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPageProxy::showTelephoneNumberMenu(const String& telephoneNumber, const WebCore::IntPoint& point, const WebCore::IntRect& rect)
{
    RetainPtr<NSMenu> menu = menuForTelephoneNumber(telephoneNumber, pageClient().viewForPresentingRevealPopover(), rect);
    pageClient().showPlatformContextMenu(menu.get(), point);
}
#endif

CGRect WebPageProxy::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    return pageClient().boundsOfLayerInLayerBackedWindowCoordinates(layer);
}

void WebPageProxy::didUpdateEditorState(const EditorState& oldEditorState, const EditorState& newEditorState)
{
    bool couldChangeSecureInputState = newEditorState.isInPasswordField != oldEditorState.isInPasswordField || oldEditorState.selectionIsNone;
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !newEditorState.selectionIsNone)
        pageClient().updateSecureInputState();
    
    if (newEditorState.shouldIgnoreSelectionChanges)
        return;

    updateFontAttributesAfterEditorStateChange();
    pageClient().selectionDidChange();
}

void WebPageProxy::startWindowDrag()
{
    pageClient().startWindowDrag();
}

NSWindow *WebPageProxy::platformWindow()
{
    return m_pageClient ? m_pageClient->platformWindow() : nullptr;
}

void WebPageProxy::rootViewToWindow(const WebCore::IntRect& viewRect, WebCore::IntRect& windowRect)
{
    windowRect = pageClient().rootViewToWindow(viewRect);
}

void WebPageProxy::showValidationMessage(const IntRect& anchorClientRect, const String& message)
{
    m_validationBubble = pageClient().createValidationBubble(message, { m_preferences->minimumFontSize() });
    m_validationBubble->showRelativeTo(anchorClientRect);
}

NSView *WebPageProxy::inspectorAttachmentView()
{
    return pageClient().inspectorAttachmentView();
}

_WKRemoteObjectRegistry *WebPageProxy::remoteObjectRegistry()
{
    return pageClient().remoteObjectRegistry();
}

#if ENABLE(APPLE_PAY)

NSWindow *WebPageProxy::paymentCoordinatorPresentingWindow(const WebPaymentCoordinatorProxy&)
{
    return platformWindow();
}

#endif

#if ENABLE(CONTEXT_MENUS)

NSMenu *WebPageProxy::activeContextMenu() const
{
    if (m_activeContextMenu)
        return m_activeContextMenu->platformMenu();
    return nil;
}

RetainPtr<NSEvent> WebPageProxy::createSyntheticEventForContextMenu(FloatPoint location) const
{
    return [NSEvent mouseEventWithType:NSEventTypeRightMouseUp location:location modifierFlags:0 timestamp:0 windowNumber:pageClient().platformWindow().windowNumber context:nil eventNumber:0 clickCount:0 pressure:0];
}

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (item.action() == ContextMenuItemTagPaste)
        grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral);
}

#endif

void WebPageProxy::willPerformPasteCommand(DOMPasteAccessCategory pasteAccessCategory)
{
    switch (pasteAccessCategory) {
    case DOMPasteAccessCategory::General:
        grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral);
        return;

    case DOMPasteAccessCategory::Fonts:
        grantAccessToCurrentPasteboardData(NSPasteboardNameFont);
        return;
    }
}

PlatformView* WebPageProxy::platformView() const
{
    return [pageClient().platformWindow() contentView];
}

#if ENABLE(UI_PROCESS_PDF_HUD)

void WebPageProxy::createPDFHUD(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    pageClient().createPDFHUD(identifier, rect);
}

void WebPageProxy::removePDFHUD(PDFPluginIdentifier identifier)
{
    pageClient().removePDFHUD(identifier);
}

void WebPageProxy::updatePDFHUDLocation(PDFPluginIdentifier identifier, const WebCore::IntRect& rect)
{
    pageClient().updatePDFHUDLocation(identifier, rect);
}

void WebPageProxy::pdfZoomIn(PDFPluginIdentifier identifier)
{
    send(Messages::WebPage::ZoomPDFIn(identifier));
}

void WebPageProxy::pdfZoomOut(PDFPluginIdentifier identifier)
{
    send(Messages::WebPage::ZoomPDFOut(identifier));
}

void WebPageProxy::pdfSaveToPDF(PDFPluginIdentifier identifier)
{
    sendWithAsyncReply(Messages::WebPage::SavePDF(identifier), [this, protectedThis = Ref { *this }] (String&& suggestedFilename, URL&& originatingURL, const IPC::DataReference& dataReference) {
        savePDFToFileInDownloadsFolder(WTFMove(suggestedFilename), WTFMove(originatingURL), dataReference);
    });
}

void WebPageProxy::pdfOpenWithPreview(PDFPluginIdentifier identifier)
{
    sendWithAsyncReply(Messages::WebPage::OpenPDFWithPreview(identifier), [this, protectedThis = Ref { *this }] (String&& suggestedFilename, FrameInfoData&& frameInfo, const IPC::DataReference& data, const String& pdfUUID) {
        savePDFToTemporaryFolderAndOpenWithNativeApplication(WTFMove(suggestedFilename), WTFMove(frameInfo), data, pdfUUID);
    });
}

#endif // ENABLE(UI_PROCESS_PDF_HUD)

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
    ASSERT(m_activeContextMenuContextData.webHitTestResultData());
    
    auto result = m_activeContextMenuContextData.webHitTestResultData().value();
    if (!result.imageBitmap)
        return;

    showImageInQuickLookPreviewPanel(*result.imageBitmap, result.toolTipText, URL { result.absoluteImageURL }, QuickLookPreviewActivity::VisualSearch);
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
    pageClient().makeFirstResponder();

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
    if (!m_activeContextMenu)
        return;

    RetainPtr image = m_activeContextMenu->copySubjectResult();
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

} // namespace WebKit

#endif // PLATFORM(MAC)

#undef MESSAGE_CHECK_WITH_RETURN_VALUE
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
