/*
 * Copyright (C) 2010-2020 Apple Inc. All rights reserved.
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
#import "ColorSpaceData.h"
#import "Connection.h"
#import "DataReference.h"
#import "EditorState.h"
#import "FontInfo.h"
#import "FrameInfoData.h"
#import "InsertTextOptions.h"
#import "MenuUtilities.h"
#import "NativeWebKeyboardEvent.h"
#import "PDFContextMenu.h"
#import "PageClient.h"
#import "PageClientImplMac.h"
#import "PluginComplexTextInputState.h"
#import "RemoteLayerTreeHost.h"
#import "StringUtilities.h"
#import "TextChecker.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKSharingServicePickerDelegate.h"
#import "WebContextMenuProxyMac.h"
#import "WebPageMessages.h"
#import "WebProcessProxy.h"
#import <WebCore/AttributedString.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DragItem.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/TextAlternativeWithRange.h>
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
    m_uiClient->windowFrame(*this, [this, protectedThis = makeRef(*this), viewFrameInWindowCoordinates, accessibilityViewCoordinates] (FloatRect windowFrameInScreenCoordinates) {
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

void WebPageProxy::attributedSubstringForCharacterRangeAsync(const EditingRange& range, Function<void(const WebCore::AttributedString&, const EditingRange&, CallbackBase::Error)>&& callbackFunction)
{
    if (!hasRunningProcess()) {
        callbackFunction({ }, EditingRange(), CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callbackFunction), m_process->throttler().backgroundActivity("WebPageProxy::attributedSubstringForCharacterRangeAsync"_s));

    send(Messages::WebPage::AttributedSubstringForCharacterRangeAsync(range, callbackID));
}

void WebPageProxy::attributedStringForCharacterRangeCallback(const WebCore::AttributedString& string, const EditingRange& actualRange, CallbackID callbackID)
{
    MESSAGE_CHECK(actualRange.isValid());

    auto callback = m_callbacks.take<AttributedStringForCharacterRangeCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        // this can validly happen if a load invalidated the callback, though
        return;
    }

    callback->performCallbackWithReturnValue(string, actualRange);
}

void WebPageProxy::fontAtSelection(Function<void(const FontInfo&, double, bool, CallbackBase::Error)>&& callback)
{
    if (!hasRunningProcess()) {
        callback({ }, 0, false, CallbackBase::Error::Unknown);
        return;
    }

    auto callbackID = m_callbacks.put(WTFMove(callback), m_process->throttler().backgroundActivity("WebPageProxy::fontAtSelection"_s));
    send(Messages::WebPage::FontAtSelection(callbackID));
}

void WebPageProxy::fontAtSelectionCallback(const FontInfo& fontInfo, double fontSize, bool selectionHasMultipleFonts, CallbackID callbackID)
{
    auto callback = m_callbacks.take<FontAtSelectionCallback>(callbackID);
    if (!callback) {
        // FIXME: Log error or assert.
        return;
    }

    callback->performCallbackWithReturnValue(fontInfo, fontSize, selectionHasMultipleFonts);
}

String WebPageProxy::stringSelectionForPasteboard()
{
    String value;
    if (!hasRunningProcess())
        return value;
    
    const Seconds messageTimeout(20);
    sendSync(Messages::WebPage::GetStringSelectionForPasteboard(), Messages::WebPage::GetStringSelectionForPasteboard::Reply(value), messageTimeout);
    return value;
}

RefPtr<WebCore::SharedBuffer> WebPageProxy::dataSelectionForPasteboard(const String& pasteboardType)
{
    if (!hasRunningProcess())
        return nullptr;

    SharedMemory::IPCHandle ipcHandle;
    uint64_t size = 0;
    const Seconds messageTimeout(20);
    sendSync(Messages::WebPage::GetDataSelectionForPasteboard(pasteboardType), Messages::WebPage::GetDataSelectionForPasteboard::Reply(ipcHandle), messageTimeout);
    MESSAGE_CHECK_WITH_RETURN_VALUE(!ipcHandle.handle.isNull(), nullptr);

    auto sharedMemoryBuffer = SharedMemory::map(ipcHandle.handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryBuffer)
        return nullptr;
    return SharedBuffer::create(static_cast<unsigned char *>(sharedMemoryBuffer->data()), static_cast<size_t>(size));
}

bool WebPageProxy::readSelectionFromPasteboard(const String& pasteboardName)
{
    if (!hasRunningProcess())
        return false;

    bool result = false;
    const Seconds messageTimeout(20);
    sendSync(Messages::WebPage::ReadSelectionFromPasteboard(pasteboardName), Messages::WebPage::ReadSelectionFromPasteboard::Reply(result), messageTimeout);
    return result;
}

#if ENABLE(SERVICE_CONTROLS)
void WebPageProxy::replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference& data)
{
    send(Messages::WebPage::ReplaceSelectionWithPasteboardData(types, data));
}
#endif

#if ENABLE(DRAG_SUPPORT)

void WebPageProxy::setPromisedDataForImage(const String& pasteboardName, const SharedMemory::IPCHandle& imageHandle, const String& filename, const String& extension,
    const String& title, const String& url, const String& visibleURL, const SharedMemory::IPCHandle& archiveHandle)
{
    MESSAGE_CHECK_URL(url);
    MESSAGE_CHECK_URL(visibleURL);
    MESSAGE_CHECK(!imageHandle.handle.isNull());

    auto sharedMemoryImage = SharedMemory::map(imageHandle.handle, SharedMemory::Protection::ReadOnly);
    if (!sharedMemoryImage)
        return;

    auto imageBuffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryImage->data()), static_cast<size_t>(imageHandle.dataSize));
    RefPtr<SharedBuffer> archiveBuffer;

    if (!archiveHandle.handle.isNull()) {
        auto sharedMemoryArchive = SharedMemory::map(archiveHandle.handle, SharedMemory::Protection::ReadOnly);
        if (!sharedMemoryArchive)
            return;
        archiveBuffer = SharedBuffer::create(static_cast<unsigned char*>(sharedMemoryArchive->data()), static_cast<size_t>(archiveHandle.dataSize));
    }
    pageClient().setPromisedDataForImage(pasteboardName, WTFMove(imageBuffer), ResourceResponseBase::sanitizeSuggestedFilename(filename), extension, title, url, visibleURL, WTFMove(archiveBuffer));
}

#endif

// Complex text input support for plug-ins.
void WebPageProxy::sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    if (!hasRunningProcess())
        return;
    
    send(Messages::WebPage::SendComplexTextInputToPlugin(pluginComplexTextInputIdentifier, textInput));
}

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

ColorSpaceData WebPageProxy::colorSpace()
{
    return pageClient().colorSpace();
}

void WebPageProxy::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    if (!hasRunningProcess())
        return;

    send(Messages::WebPage::RegisterUIProcessAccessibilityTokens(elementToken, windowToken));
}

void WebPageProxy::pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus)
{
    pageClient().pluginFocusOrWindowFocusChanged(pluginComplexTextInputIdentifier, pluginHasFocusAndWindowHasFocus);
}

void WebPageProxy::setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, uint64_t pluginComplexTextInputState)
{
    MESSAGE_CHECK(isValidPluginComplexTextInputState(pluginComplexTextInputState));

    pageClient().setPluginComplexTextInputState(pluginComplexTextInputIdentifier, static_cast<PluginComplexTextInputState>(pluginComplexTextInputState));
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

    bool result = false;
    const Seconds messageTimeout(3);
    sendSync(Messages::WebPage::ShouldDelayWindowOrderingEvent(event), Messages::WebPage::ShouldDelayWindowOrderingEvent::Reply(result), messageTimeout);
    return result;
}

bool WebPageProxy::acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent& event)
{
    if (!hasRunningProcess())
        return false;

    bool result = false;
    const Seconds messageTimeout(3);
    sendSync(Messages::WebPage::AcceptsFirstMouse(eventNumber, event), Messages::WebPage::AcceptsFirstMouse::Reply(result), messageTimeout);
    return result;
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
    static NSString *temporaryPDFDirectoryPath;

    if (!temporaryPDFDirectoryPath) {
        NSString *temporaryDirectoryTemplate = [NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitPDFs-XXXXXX"];
        CString templateRepresentation = [temporaryDirectoryTemplate fileSystemRepresentation];

        if (mkdtemp(templateRepresentation.mutableData()))
            temporaryPDFDirectoryPath = [[[NSFileManager defaultManager] stringWithFileSystemRepresentation:templateRepresentation.data() length:templateRepresentation.length()] copy];
    }

    return temporaryPDFDirectoryPath;
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
    MESSAGE_CHECK(TemporaryPDFFileMap::isValidKey(pdfUUID));

    if (data.isEmpty()) {
        WTFLogAlways("Cannot save empty PDF file to the temporary directory.");
        return;
    }

    auto sanitizedFilename = ResourceResponseBase::sanitizeSuggestedFilename(suggestedFilename);
    if (!sanitizedFilename.endsWithIgnoringASCIICase(".pdf")) {
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

    m_temporaryPDFFiles.add(pdfUUID, nsPath.get());

    auto pdfFileURL = URL::fileURLWithFileSystemPath(String(nsPath.get()));
    m_uiClient->confirmPDFOpening(*this, pdfFileURL, WTFMove(frameInfo), [pdfFileURL] (bool allowed) {
        if (!allowed)
            return;
        [[NSWorkspace sharedWorkspace] openURL:pdfFileURL];
    });
}

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

#if ENABLE(PDFKIT_PLUGIN)
void WebPageProxy::showPDFContextMenu(const WebKit::PDFContextMenu& contextMenu, CompletionHandler<void(Optional<int32_t>&&)>&& completionHandler)
{
    if (!contextMenu.m_items.size())
        return completionHandler(WTF::nullopt);
    
    RetainPtr<WKPDFMenuTarget> menuTarget = adoptNS([[WKPDFMenuTarget alloc] init]);
    RetainPtr<NSMenu> nsMenu = adoptNS([[NSMenu alloc] init]);
    [nsMenu setAllowsContextMenuPlugIns:false];
    for (unsigned i = 0; i < contextMenu.m_items.size(); i++) {
        auto& item = contextMenu.m_items[i];
        
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
    auto windowNumber = [window windowNumber];
    auto location = [window convertRectFromScreen: { contextMenu.m_point, NSZeroSize }].origin;
    NSEvent* event = [NSEvent mouseEventWithType:NSEventTypeRightMouseDown location:location modifierFlags:0 timestamp:0 windowNumber:windowNumber context:0 eventNumber:0 clickCount:1 pressure:1];

    auto view = [pageClient().platformWindow() contentView];
    [NSMenu popUpContextMenu:nsMenu.get() withEvent:event forView:view];

    if (auto selectedMenuItem = [menuTarget selectedMenuItem])
        return completionHandler([selectedMenuItem tag]);
    completionHandler(WTF::nullopt);
}
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPageProxy::showTelephoneNumberMenu(const String& telephoneNumber, const WebCore::IntPoint& point)
{
    RetainPtr<NSMenu> menu = menuForTelephoneNumber(telephoneNumber);
    pageClient().showPlatformContextMenu(menu.get(), point);
}
#endif

CGRect WebPageProxy::boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const
{
    return pageClient().boundsOfLayerInLayerBackedWindowCoordinates(layer);
}

bool WebPageProxy::appleMailPaginationQuirkEnabled()
{
    return MacApplication::isAppleMail();
}

bool WebPageProxy::appleMailLinesClampEnabled()
{
    return MacApplication::isAppleMail();
}

void WebPageProxy::updateEditorState(const EditorState& editorState)
{
    bool couldChangeSecureInputState = m_editorState.isInPasswordField != editorState.isInPasswordField || m_editorState.selectionIsNone;
    
    m_editorState = editorState;
    
    // Selection being none is a temporary state when editing. Flipping secure input state too quickly was causing trouble (not fully understood).
    if (couldChangeSecureInputState && !editorState.selectionIsNone)
        pageClient().updateSecureInputState();
    
    if (editorState.shouldIgnoreSelectionChanges)
        return;
    
    pageClient().selectionDidChange();
    updateFontAttributesAfterEditorStateChange();
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

void WebPageProxy::platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData& item)
{
    if (item.action() == ContextMenuItemTagPaste)
        grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral);
}

#endif

void WebPageProxy::willPerformPasteCommand()
{
    grantAccessToCurrentPasteboardData(NSPasteboardNameGeneral);
}

PlatformView* WebPageProxy::platformView() const
{
    return [pageClient().platformWindow() contentView];
}

} // namespace WebKit

#endif // PLATFORM(MAC)

#undef MESSAGE_CHECK_WITH_RETURN_VALUE
#undef MESSAGE_CHECK_URL
#undef MESSAGE_CHECK
