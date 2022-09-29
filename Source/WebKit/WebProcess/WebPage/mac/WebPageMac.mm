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
#import "WebPage.h"

#if PLATFORM(MAC)

#import "ContextMenuContextData.h"
#import "DataReference.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "FontInfo.h"
#import "FrameInfoData.h"
#import "InjectedBundleHitTestResult.h"
#import "PDFPlugin.h"
#import "PageBanner.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "UserData.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebCoreArgumentCoders.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebHitTestResultData.h"
#import "WebImage.h"
#import "WebInspector.h"
#import "WebKeyboardEvent.h"
#import "WebMouseEvent.h"
#import "WebPageOverlay.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPreferencesStore.h"
#import "WebProcess.h"
#import <Quartz/Quartz.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLAttachmentElement.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLPlugInImageElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/PointerCharacteristics.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderStyle.h>
#import <WebCore/RenderView.h>
#import <WebCore/RuntimeApplicationChecks.h>
#import <WebCore/ScrollView.h>
#import <WebCore/StyleInheritedData.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <wtf/SetForScope.h>
#import <wtf/SortedArrayMap.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetCocoa.h>
#import <WebCore/MediaPlaybackTargetContext.h>
#import <WebCore/MediaPlaybackTargetMock.h>
#endif

#import "PDFKitSoftLink.h"

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitializeAccessibility()
{
    // Need to initialize accessibility for VoiceOver to work when the WebContent process is using NSRunLoop.
    // Currently, it is also needed to allocate and initialize an NSApplication object.
    [NSApplication _accessibilityInitialize];

    auto mockAccessibilityElement = adoptNS([[WKAccessibilityWebPageObject alloc] init]);

    // Get the pid for the starting process.
    pid_t pid = WebCore::presentingApplicationPID();
    // FIXME: WKAccessibilityWebPageObject doesn't respond to -accessibilitySetPresenterProcessIdentifier:.
    // Either it needs to or this call should be removed.
    if ([mockAccessibilityElement respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)mockAccessibilityElement.get() accessibilitySetPresenterProcessIdentifier:pid];
    [mockAccessibilityElement setWebPage:this];
    m_mockAccessibilityElement = WTFMove(mockAccessibilityElement);

    accessibilityTransferRemoteToken(accessibilityRemoteTokenData());
}

void WebPage::platformReinitialize()
{
    accessibilityTransferRemoteToken(accessibilityRemoteTokenData());
}

RetainPtr<NSData> WebPage::accessibilityRemoteTokenData() const
{
    ASSERT(m_mockAccessibilityElement);
    return [NSAccessibilityRemoteUIElement remoteTokenForLocalUIElement:m_mockAccessibilityElement.get()];
}

void WebPage::platformDetach()
{
    [m_mockAccessibilityElement setWebPage:nullptr];
}

void WebPage::getPlatformEditorState(Frame& frame, EditorState& result) const
{
    getPlatformEditorStateCommon(frame, result);

    result.canEnableAutomaticSpellingCorrection = result.isContentEditable && frame.editor().canEnableAutomaticSpellingCorrection();

    if (result.isMissingPostLayoutData)
        return;

    auto& selection = frame.selection().selection();
    auto selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return;

    auto& postLayoutData = result.postLayoutData();
    VisiblePosition selectionStart = selection.visibleStart();
    auto selectionStartBoundary = makeBoundaryPoint(selectionStart);
    auto selectionEnd = makeBoundaryPoint(selection.visibleEnd());
    auto paragraphStart = makeBoundaryPoint(startOfParagraph(selectionStart));

    if (!selectionStartBoundary || !selectionEnd || !paragraphStart)
        return;

    auto contextRangeForCandidateRequest = frame.editor().contextRangeForCandidateRequest();

    postLayoutData.candidateRequestStartPosition = characterCount({ *paragraphStart, *selectionStartBoundary });
    postLayoutData.selectedTextLength = characterCount({ *selectionStartBoundary, *selectionEnd });
    postLayoutData.paragraphContextForCandidateRequest = contextRangeForCandidateRequest ? plainText(*contextRangeForCandidateRequest) : String();
    postLayoutData.stringForCandidateRequest = frame.editor().stringForCandidateRequest();

    auto quads = RenderObject::absoluteTextQuads(*selectedRange);
    if (!quads.isEmpty())
        postLayoutData.selectionBoundingRect = frame.view()->contentsToWindow(quads[0].enclosingBoundingBox());
    else if (selection.isCaret()) {
        // Quads will be empty at the start of a paragraph.
        postLayoutData.selectionBoundingRect = frame.view()->contentsToWindow(frame.selection().absoluteCaretBounds());
    }
}

void WebPage::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    Frame* frame = m_page->focusController().focusedFrame();
    if (!frame)
        return;

    frame->editor().handleAcceptedCandidate(acceptedCandidate);
    send(Messages::WebPageProxy::DidHandleAcceptedCandidate());
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
    if (!m_page)
        return nil;
    
    if (auto* pluginView = mainFramePlugIn())
        return pluginView->accessibilityObject();

    return nil;
}

bool WebPage::shouldUsePDFPlugin(const String& contentType, StringView path) const
{
    return pdfPluginEnabled()
#if ENABLE(PDFJS)
        && !corePage()->settings().pdfJSViewerEnabled()
#endif
        && getPDFLayerControllerClass()
        && (MIMETypeRegistry::isPDFOrPostScriptMIMEType(contentType)
            || (contentType.isEmpty()
                && (path.endsWithIgnoringASCIICase(".pdf"_s) || path.endsWithIgnoringASCIICase(".ps"_s))));
}

static String commandNameForSelectorName(const String& selectorName)
{
    // Map selectors into Editor command names.
    // This is not needed for any selectors that have the same name as the Editor command.
    static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> selectorExceptions[] = {
        { "insertNewlineIgnoringFieldEditor:", "InsertNewline"_s },
        { "insertParagraphSeparator:", "InsertNewline"_s },
        { "insertTabIgnoringFieldEditor:", "InsertTab"_s },
        { "pageDown:", "MovePageDown"_s },
        { "pageDownAndModifySelection:", "MovePageDownAndModifySelection"_s },
        { "pageUp:", "MovePageUp"_s },
        { "pageUpAndModifySelection:", "MovePageUpAndModifySelection"_s },
    };
    static constexpr SortedArrayMap map { selectorExceptions };
    if (auto commandName = map.tryGet(selectorName))
        return *commandName;

    // Remove the trailing colon.
    // No need to capitalize the command name since Editor command names are not case sensitive.
    size_t selectorNameLength = selectorName.length();
    if (selectorNameLength < 2 || selectorName[selectorNameLength - 1] != ':')
        return String();
    return selectorName.left(selectorNameLength - 1);
}

static Frame* frameForEvent(KeyboardEvent* event)
{
    ASSERT(event->target());
    Frame* frame = downcast<Node>(event->target())->document().frame();
    ASSERT(frame);
    return frame;
}

bool WebPage::executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>& commands, KeyboardEvent* event)
{
    Frame& frame = event ? *frameForEvent(event) : m_page->focusController().focusedOrMainFrame();
    ASSERT(frame.page() == corePage());

    bool eventWasHandled = false;
    for (size_t i = 0; i < commands.size(); ++i) {
        if (commands[i].commandName == "insertText:"_s) {
            if (frame.editor().hasComposition()) {
                eventWasHandled = true;
                frame.editor().confirmComposition(commands[i].text);
            } else {
                if (!frame.editor().canEdit())
                    continue;

                // An insertText: might be handled by other responders in the chain if we don't handle it.
                // One example is space bar that results in scrolling down the page.
                eventWasHandled |= frame.editor().insertText(commands[i].text, event);
            }
        } else {
            Editor::Command command = frame.editor().command(commandNameForSelectorName(commands[i].commandName));
            if (command.isSupported()) {
                bool commandExecutedByEditor = command.execute(event);
                eventWasHandled |= commandExecutedByEditor;
                if (!commandExecutedByEditor) {
                    bool performedNonEditingBehavior = event->underlyingPlatformEvent()->type() == PlatformEvent::RawKeyDown && performNonEditingBehaviorForSelector(commands[i].commandName, event);
                    eventWasHandled |= performedNonEditingBehavior;
                }
            } else {
                auto sendResult = WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::ExecuteSavedCommandBySelector(commands[i].commandName), m_identifier);
                auto [commandWasHandledByUIProcess] = sendResult.takeReplyOr(false);
                eventWasHandled |= commandWasHandledByUIProcess;
            }
        }
    }
    return eventWasHandled;
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent& event)
{
    auto* frame = frameForEvent(&event);
    
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return false;
    auto& commands = event.keypressCommands();

    ASSERT(!platformEvent->macEvent()); // Cannot have a native event in WebProcess.

    // Don't handle Esc while handling keydown event, we need to dispatch a keypress first.
    if (platformEvent->type() != PlatformEvent::Char && platformEvent->windowsVirtualKeyCode() == VK_ESCAPE && commands.size() == 1 && commandNameForSelectorName(commands[0].commandName) == "cancelOperation"_s)
        return false;

    if (handleKeyEventByRelinquishingFocusToChrome(event))
        return true;

    bool eventWasHandled = false;

    // Are there commands that could just cause text insertion if executed via Editor?
    // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
    // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
    // (e.g. Tab that inserts a Tab character, or Enter).
    bool haveTextInsertionCommands = false;
    for (auto& command : commands) {
        if (frame->editor().command(commandNameForSelectorName(command.commandName)).isTextInsertion())
            haveTextInsertionCommands = true;
    }
    // If there are no text insertion commands, default keydown handler is the right time to execute the commands.
    // Keypress (Char event) handler is the latest opportunity to execute.
    if (!haveTextInsertionCommands || platformEvent->type() == PlatformEvent::Char) {
        eventWasHandled = executeKeypressCommandsInternal(commands, &event);
        commands.clear();
    }

    return eventWasHandled;
}

void WebPage::attributedSubstringForCharacterRangeAsync(const EditingRange& editingRange, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&& completionHandler)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone() || !selection.isContentEditable() || selection.isInPasswordField()) {
        completionHandler({ }, { });
        return;
    }

    auto range = EditingRange::toRange(frame, editingRange);
    if (!range) {
        completionHandler({ }, { });
        return;
    }

    auto attributedString = editingAttributedString(*range, IncludeImages::No).string;

    // WebCore::editingAttributedStringFromRange() insists on inserting a trailing
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([attributedString length] > editingRange.length) {
        ASSERT([attributedString length] == editingRange.length + 1);
        ASSERT([[attributedString string] characterAtIndex:editingRange.length] == '\n' || [[attributedString string] characterAtIndex:editingRange.length] == ' ');
        attributedString = [attributedString attributedSubstringFromRange:NSMakeRange(0, editingRange.length)];
    }

    EditingRange rangeToSend(editingRange.location, [attributedString length]);
    ASSERT(rangeToSend.isValid());
    if (!rangeToSend.isValid()) {
        // Send an empty EditingRange as a last resort for <rdar://problem/27078089>.
        completionHandler({ WTFMove(attributedString), nil }, EditingRange());
        return;
    }

    completionHandler({ WTFMove(attributedString), nil }, rangeToSend);
}

#if ENABLE(PDFKIT_PLUGIN)

DictionaryPopupInfo WebPage::dictionaryPopupInfoForSelectionInPDFPlugin(PDFSelection *selection, PluginView& pdfPlugin, NSDictionary *options, WebCore::TextIndicatorPresentationTransition presentationTransition)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    if (!selection.string.length)
        return dictionaryPopupInfo;

    NSRect rangeRect = pdfPlugin.rectForSelectionInRootView(selection);

    NSAttributedString *nsAttributedString = selection.attributedString;
    
    RetainPtr<NSMutableAttributedString> scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);
    
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    CGFloat scaleFactor = pdfPlugin.contentScaleFactor();

    __block CGFloat maxAscender = 0;
    __block CGFloat maxDescender = 0;
    [nsAttributedString enumerateAttributesInRange:NSMakeRange(0, [nsAttributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);
        
        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font) {
            maxAscender = std::max(maxAscender, font.ascender * scaleFactor);
            maxDescender = std::min(maxDescender, font.descender * scaleFactor);
            font = [fontManager convertFont:font toSize:[font pointSize] * scaleFactor];
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        }
        
        [scaledNSAttributedString addAttributes:scaledAttributes.get() range:range];
    }];

    rangeRect.size.height = nsAttributedString.size.height * scaleFactor;
    rangeRect.size.width = nsAttributedString.size.width * scaleFactor;
    
    TextIndicatorData dataForSelection;
    dataForSelection.selectionRectInRootViewCoordinates = rangeRect;
    dataForSelection.textBoundingRectInRootViewCoordinates = rangeRect;
    dataForSelection.contentImageScaleFactor = scaleFactor;
    dataForSelection.presentationTransition = presentationTransition;
    
    dictionaryPopupInfo.origin = rangeRect.origin;
    dictionaryPopupInfo.platformData.options = options;
    dictionaryPopupInfo.textIndicator = dataForSelection;
    dictionaryPopupInfo.platformData.attributedString = scaledNSAttributedString;
    
    return dictionaryPopupInfo;
}

#endif

bool WebPage::performNonEditingBehaviorForSelector(const String& selector, KeyboardEvent* event)
{
    // First give accessibility a chance to handle the event.
    Frame* frame = frameForEvent(event);
    frame->eventHandler().handleKeyboardSelectionMovementForAccessibility(*event);
    if (event->defaultHandled())
        return true;

    // FIXME: All these selectors have corresponding Editor commands, but the commands only work in editable content.
    // Should such non-editing behaviors be implemented in Editor or EventHandler::defaultArrowEventHandler() perhaps?
    
    bool didPerformAction = false;
    
    if (!frame->eventHandler().shouldUseSmoothKeyboardScrollingForFocusedScrollableArea()) {
        if (selector == "moveUp:"_s)
            didPerformAction = scroll(m_page.get(), ScrollUp, ScrollGranularity::Line);
        else if (selector == "moveToBeginningOfParagraph:"_s)
            didPerformAction = scroll(m_page.get(), ScrollUp, ScrollGranularity::Page);
        else if (selector == "moveToBeginningOfDocument:"_s) {
            didPerformAction = scroll(m_page.get(), ScrollUp, ScrollGranularity::Document);
            didPerformAction |= scroll(m_page.get(), ScrollLeft, ScrollGranularity::Document);
        } else if (selector == "moveDown:"_s)
            didPerformAction = scroll(m_page.get(), ScrollDown, ScrollGranularity::Line);
        else if (selector == "moveToEndOfParagraph:"_s)
            didPerformAction = scroll(m_page.get(), ScrollDown, ScrollGranularity::Page);
        else if (selector == "moveToEndOfDocument:"_s) {
            didPerformAction = scroll(m_page.get(), ScrollDown, ScrollGranularity::Document);
            didPerformAction |= scroll(m_page.get(), ScrollLeft, ScrollGranularity::Document);
        } else if (selector == "moveLeft:"_s)
            didPerformAction = scroll(m_page.get(), ScrollLeft, ScrollGranularity::Line);
        else if (selector == "moveWordLeft:"_s)
            didPerformAction = scroll(m_page.get(), ScrollLeft, ScrollGranularity::Page);
        else if (selector == "moveRight:"_s)
            didPerformAction = scroll(m_page.get(), ScrollRight, ScrollGranularity::Line);
        else if (selector == "moveWordRight:"_s)
            didPerformAction = scroll(m_page.get(), ScrollRight, ScrollGranularity::Page);
    }

    if (selector == "moveToLeftEndOfLine:"_s)
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? m_page->backForward().goBack() : m_page->backForward().goForward();
    else if (selector == "moveToRightEndOfLine:"_s)
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? m_page->backForward().goForward() : m_page->backForward().goBack();

    return didPerformAction;
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&)
{
    return false;
}

void WebPage::registerUIProcessAccessibilityTokens(const IPC::DataReference& elementToken, const IPC::DataReference& windowToken)
{
    NSData *elementTokenData = [NSData dataWithBytes:elementToken.data() length:elementToken.size()];
    NSData *windowTokenData = [NSData dataWithBytes:windowToken.data() length:windowToken.size()];
    auto remoteElement = elementTokenData.length ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:elementTokenData]) : nil;
    auto remoteWindow = windowTokenData.length ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:windowTokenData]) : nil;
    [remoteElement setWindowUIElement:remoteWindow.get()];
    [remoteElement setTopLevelUIElement:remoteWindow.get()];

    [accessibilityRemoteObject() setRemoteParent:remoteElement.get()];
}

void WebPage::getStringSelectionForPasteboard(CompletionHandler<void(String&&)>&& completionHandler)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (auto* pluginView = focusedPluginViewForFrame(frame)) {
        String selection = pluginView->getSelectionString();
        if (!selection.isNull())
            return completionHandler(WTFMove(selection));
    }

    if (frame.selection().isNone())
        return completionHandler({ });

    completionHandler(frame.editor().stringSelectionForPasteboard());
}

void WebPage::getDataSelectionForPasteboard(const String pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone())
        return completionHandler({ });

    auto buffer = frame.editor().dataSelectionForPasteboard(pasteboardType);
    if (!buffer)
        return completionHandler({ });
    completionHandler(buffer.releaseNonNull());
}

WKAccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    return m_mockAccessibilityElement.get();
}

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest& request)
{
    if ([NSURLConnection canHandleRequest:request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody)])
        return true;

    // FIXME: Return true if this scheme is any one WebKit2 knows how to handle.
    return request.url().protocolIs("applewebdata"_s);
}

void WebPage::shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent& event, CompletionHandler<void(bool)>&& completionHandler)
{
    auto& frame = m_page->focusController().focusedOrMainFrame();

    bool result = false;
#if ENABLE(DRAG_SUPPORT)
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitResult = frame.eventHandler().hitTestResultAtPoint(frame.view()->windowToContents(event.position()), hitType);
    if (hitResult.isSelected())
        result = frame.eventHandler().eventMayStartDrag(platform(event));
#endif
    completionHandler(result);
}

void WebPage::requestAcceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent& event)
{
    if (WebProcess::singleton().parentProcessConnection()->inSendSync()) {
        // In case we're already inside a sendSync message, it's possible that the page is in a
        // transitionary state, so any hit-testing could cause crashes  so we just return early in that case.
        send(Messages::WebPageProxy::HandleAcceptsFirstMouse(false));
        return;
    }

    auto& frame = m_page->focusController().focusedOrMainFrame();

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitResult = frame.eventHandler().hitTestResultAtPoint(frame.view()->windowToContents(event.position()), hitType);
    frame.eventHandler().setActivationEventNumber(eventNumber);
    bool result = false;
#if ENABLE(DRAG_SUPPORT)
    if (hitResult.isSelected())
        result = frame.eventHandler().eventMayStartDrag(platform(event));
    else
#endif
        result = !!hitResult.scrollbar();

    send(Messages::WebPageProxy::HandleAcceptsFirstMouse(result));
}

void WebPage::setTopOverhangImage(WebImage* image)
{
    auto* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    auto* layer = frameView->setWantsLayerForTopOverHangArea(image);
    if (!layer)
        return;

    auto nativeImage = image->copyNativeImage();
    if (!nativeImage)
        return;

    layer->setSize(image->size());
    layer->setPosition(FloatPoint(0, -image->size().height()));
    layer->platformLayer().contents = (__bridge id)nativeImage->platformImage().get();
}

void WebPage::setBottomOverhangImage(WebImage* image)
{
    auto* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    auto* layer = frameView->setWantsLayerForBottomOverHangArea(image);
    if (!layer)
        return;

    auto nativeImage = image->copyNativeImage();
    if (!nativeImage)
        return;

    layer->setSize(image->size());
    layer->platformLayer().contents = (__bridge id)nativeImage->platformImage().get();
}

void WebPage::updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor)
{    
    if (m_headerBanner)
        m_headerBanner->didChangeDeviceScaleFactor(scaleFactor);
    if (m_footerBanner)
        m_footerBanner->didChangeDeviceScaleFactor(scaleFactor);
}

void WebPage::computePagesForPrintingPDFDocument(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, Vector<IntRect>& resultPageRects)
{
    ASSERT(resultPageRects.isEmpty());
    WebFrame* frame = WebProcess::singleton().webFrame(frameID);
    Frame* coreFrame = frame ? frame->coreFrame() : 0;
    RetainPtr<PDFDocument> pdfDocument = coreFrame ? pdfDocumentForPrintingFrame(coreFrame) : 0;
    if ([pdfDocument allowsPrinting]) {
        NSUInteger pageCount = [pdfDocument pageCount];
        IntRect pageRect(0, 0, ceilf(printInfo.availablePaperWidth), ceilf(printInfo.availablePaperHeight));
        for (NSUInteger i = 1; i <= pageCount; ++i) {
            resultPageRects.append(pageRect);
            pageRect.move(0, pageRect.height());
        }
    }
}

static inline CGFloat roundCGFloat(CGFloat f)
{
    if (sizeof(CGFloat) == sizeof(float))
        return roundf(static_cast<float>(f));
    return static_cast<CGFloat>(round(f));
}

static void drawPDFPage(PDFDocument *pdfDocument, CFIndex pageIndex, CGContextRef context, CGFloat pageSetupScaleFactor, CGSize paperSize)
{
    CGContextSaveGState(context);

    CGContextScaleCTM(context, pageSetupScaleFactor, pageSetupScaleFactor);

    PDFPage *pdfPage = [pdfDocument pageAtIndex:pageIndex];
    NSRect cropBox = [pdfPage boundsForBox:kPDFDisplayBoxCropBox];
    if (NSIsEmptyRect(cropBox))
        cropBox = [pdfPage boundsForBox:kPDFDisplayBoxMediaBox];
    else
        cropBox = NSIntersectionRect(cropBox, [pdfPage boundsForBox:kPDFDisplayBoxMediaBox]);

    // Always auto-rotate PDF content regardless of the paper orientation.
    NSInteger rotation = [pdfPage rotation];
    if (rotation == 90 || rotation == 270)
        std::swap(cropBox.size.width, cropBox.size.height);

    bool shouldRotate = (paperSize.width < paperSize.height) != (cropBox.size.width < cropBox.size.height);
    if (shouldRotate)
        std::swap(cropBox.size.width, cropBox.size.height);

    // Center.
    CGFloat widthDifference = paperSize.width / pageSetupScaleFactor - cropBox.size.width;
    CGFloat heightDifference = paperSize.height / pageSetupScaleFactor - cropBox.size.height;
    if (widthDifference || heightDifference)
        CGContextTranslateCTM(context, roundCGFloat(widthDifference / 2), roundCGFloat(heightDifference / 2));

    if (shouldRotate) {
        CGContextRotateCTM(context, static_cast<CGFloat>(piOverTwoDouble));
        CGContextTranslateCTM(context, 0, -cropBox.size.width);
    }

    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithCGContext:context flipped:NO]];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [pdfPage drawWithBox:kPDFDisplayBoxCropBox];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [NSGraphicsContext restoreGraphicsState];

    CGAffineTransform transform = CGContextGetCTM(context);

    for (PDFAnnotation *annotation in [pdfPage annotations]) {
        if (![annotation isKindOfClass:getPDFAnnotationLinkClass()])
            continue;

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
        ALLOW_DEPRECATED_DECLARATIONS_END
        NSURL *url = [linkAnnotation URL];
        if (!url)
            continue;

        CGRect urlRect = NSRectToCGRect([linkAnnotation bounds]);
        CGRect transformedRect = CGRectApplyAffineTransform(urlRect, transform);
        CGPDFContextSetURLForRect(context, (CFURLRef)url, transformedRect);
    }

    CGContextRestoreGState(context);
}

void WebPage::drawPDFDocument(CGContextRef context, PDFDocument *pdfDocument, const PrintInfo& printInfo, const WebCore::IntRect& rect)
{
    NSUInteger pageCount = [pdfDocument pageCount];
    IntSize paperSize(ceilf(printInfo.availablePaperWidth), ceilf(printInfo.availablePaperHeight));
    IntRect pageRect(IntPoint(), paperSize);
    for (NSUInteger i = 0; i < pageCount; ++i) {
        if (pageRect.intersects(rect)) {
            CGContextSaveGState(context);

            CGContextTranslateCTM(context, pageRect.x() - rect.x(), pageRect.y() - rect.y());
            drawPDFPage(pdfDocument, i, context, printInfo.pageSetupScaleFactor, paperSize);

            CGContextRestoreGState(context);
        }
        pageRect.move(0, pageRect.height());
    }
}

void WebPage::drawPagesToPDFFromPDFDocument(CGContextRef context, PDFDocument *pdfDocument, const PrintInfo& printInfo, uint32_t first, uint32_t count)
{
    NSUInteger pageCount = [pdfDocument pageCount];
    for (uint32_t page = first; page < first + count; ++page) {
        if (page >= pageCount)
            break;

        RetainPtr<CFDictionaryRef> pageInfo = adoptCF(CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        CGPDFContextBeginPage(context, pageInfo.get());
        drawPDFPage(pdfDocument, page, context, printInfo.pageSetupScaleFactor, CGSizeMake(printInfo.availablePaperWidth, printInfo.availablePaperHeight));
        CGPDFContextEndPage(context);
    }
}

#if ENABLE(WEBGL)
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame*, const URL& url)
{
    auto sendResult = sendSync(Messages::WebPageProxy::WebGLPolicyForURL(url));
    auto [policyResult] = sendResult.takeReplyOr(WebGLLoadPolicy::WebGLAllowCreation);
    return policyResult;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame*, const URL& url)
{
    auto sendResult = sendSync(Messages::WebPageProxy::ResolveWebGLPolicyForURL(url));
    auto [policyResult] = sendResult.takeReplyOr(WebGLLoadPolicy::WebGLAllowCreation);
    return policyResult;
}
#endif // ENABLE(WEBGL)

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPage::handleTelephoneNumberClick(const String& number, const IntPoint& point, const IntRect& rect)
{
    send(Messages::WebPageProxy::ShowTelephoneNumberMenu(number, point, rect));
}
#endif

#if ENABLE(SERVICE_CONTROLS)

void WebPage::handleSelectionServiceClick(FrameSelection& selection, const Vector<String>& phoneNumbers, const IntPoint& point)
{
    auto range = selection.selection().firstRange();
    if (!range)
        return;

    auto attributedSelection = attributedString(*range).string;
    if (!attributedSelection)
        return;

    NSData *selectionData = [attributedSelection RTFDFromRange:NSMakeRange(0, [attributedSelection length]) documentAttributes:@{ }];

    flushPendingEditorStateUpdate();
    send(Messages::WebPageProxy::ShowContextMenu(ContextMenuContextData(point, Vector { reinterpret_cast<const uint8_t*>(selectionData.bytes), selectionData.length }, phoneNumbers, selection.selection().isContentEditable()), UserData()));
}

void WebPage::handleImageServiceClick(const IntPoint& point, Image& image, HTMLImageElement& element)
{
    send(Messages::WebPageProxy::ShowContextMenu(ContextMenuContextData {
        point,
        image,
        element.isContentEditable(),
        element.renderBox()->absoluteContentQuad().enclosingBoundingBox(),
        HTMLAttachmentElement::getAttachmentIdentifier(element),
        contextForElement(element),
        image.mimeType()
    }, { }));
}

void WebPage::handlePDFServiceClick(const IntPoint& point, HTMLAttachmentElement& element)
{
    send(Messages::WebPageProxy::ShowContextMenu(ContextMenuContextData {
        point,
        element.isContentEditable(),
        element.renderBox()->absoluteContentQuad().enclosingBoundingBox(),
        element.uniqueIdentifier(),
        "application/pdf"_s
    }, { }));
}

#endif

String WebPage::platformUserAgent(const URL&) const
{
    return String();
}

bool WebPage::hoverSupportedByPrimaryPointingDevice() const
{
    return true;
}

bool WebPage::hoverSupportedByAnyAvailablePointingDevice() const
{
    return true;
}

std::optional<PointerCharacteristics> WebPage::pointerCharacteristicsOfPrimaryPointingDevice() const
{
    return PointerCharacteristics::Fine;
}

OptionSet<PointerCharacteristics> WebPage::pointerCharacteristicsOfAllAvailablePointingDevices() const
{
    return PointerCharacteristics::Fine;
}

void WebPage::performImmediateActionHitTestAtLocation(WebCore::FloatPoint locationInViewCoordinates)
{
    layoutIfNeeded();

    auto& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView()) {
        send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(WebHitTestResultData(), false, UserData()));
        return;
    }

    auto locationInContentCoordinates = mainFrame.view()->rootViewToContents(roundedIntPoint(locationInViewCoordinates));
    auto hitTestResult = mainFrame.eventHandler().hitTestResultAtPoint(locationInContentCoordinates, {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::DisallowUserAgentShadowContentExceptForImageOverlays,
        HitTestRequest::Type::AllowChildFrameContent,
    });

    bool immediateActionHitTestPreventsDefault = false;
    Element* element = hitTestResult.targetElement();

    mainFrame.eventHandler().setImmediateActionStage(ImmediateActionStage::PerformedHitTest);
    if (element)
        immediateActionHitTestPreventsDefault = element->dispatchMouseForceWillBegin();

    WebHitTestResultData immediateActionResult(hitTestResult, { });

    auto selectionRange = corePage()->focusController().focusedOrMainFrame().selection().selection().firstRange();

    auto indicatorOptions = [&](const SimpleRange& range) {
        OptionSet<TextIndicatorOption> options { TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges, TextIndicatorOption::UseUserSelectAllCommonAncestor };
        if (ImageOverlay::isInsideOverlay(range))
            options.add({ TextIndicatorOption::PaintAllContent, TextIndicatorOption::PaintBackgrounds });
        return options;
    };

    URL absoluteLinkURL = hitTestResult.absoluteLinkURL();
    if (auto urlElement = RefPtr { hitTestResult.URLElement() }; !absoluteLinkURL.isEmpty() && urlElement) {
        auto elementRange = makeRangeSelectingNodeContents(*urlElement);
        immediateActionResult.linkTextIndicator = TextIndicator::createWithRange(elementRange, indicatorOptions(elementRange), TextIndicatorPresentationTransition::FadeIn);
    }

    if (auto lookupResult = lookupTextAtLocation(locationInViewCoordinates)) {
        auto [lookupRange, options] = WTFMove(*lookupResult);
        immediateActionResult.lookupText = plainText(lookupRange);
        if (auto* node = hitTestResult.innerNode()) {
            if (auto* frame = node->document().frame())
                immediateActionResult.dictionaryPopupInfo = dictionaryPopupInfoForRange(*frame, lookupRange, options, TextIndicatorPresentationTransition::FadeIn);
        }
    }

    bool pageOverlayDidOverrideDataDetectors = false;
    for (auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        auto webOverlay = WebPageOverlay::fromCoreOverlay(*overlay);
        if (!webOverlay)
            continue;

        auto actionContext = webOverlay->actionContextForResultAtPoint(locationInContentCoordinates);
        if (!actionContext)
            continue;

        auto view = actionContext->range.start.document().view();
        if (!view)
            continue;

        pageOverlayDidOverrideDataDetectors = true;
        immediateActionResult.detectedDataActionContext = actionContext->context.get();
        immediateActionResult.detectedDataBoundingBox = view->contentsToWindow(enclosingIntRect(unitedBoundingBoxes(RenderObject::absoluteTextQuads(actionContext->range))));
        immediateActionResult.detectedDataTextIndicator = TextIndicator::createWithRange(actionContext->range, indicatorOptions(actionContext->range), TextIndicatorPresentationTransition::FadeIn);
        immediateActionResult.detectedDataOriginatingPageOverlay = overlay->pageOverlayID();
        break;
    }

    // FIXME: Avoid scanning if we will just throw away the result (e.g. we're over a link).
    if (!pageOverlayDidOverrideDataDetectors && hitTestResult.innerNode() && (hitTestResult.innerNode()->isTextNode() || hitTestResult.isOverTextInsideFormControlElement())) {
        if (auto result = DataDetection::detectItemAroundHitTestResult(hitTestResult)) {
            immediateActionResult.detectedDataActionContext = WTFMove(result->actionContext);
            immediateActionResult.detectedDataBoundingBox = result->boundingBox;
            immediateActionResult.detectedDataTextIndicator = TextIndicator::createWithRange(result->range, indicatorOptions(result->range), TextIndicatorPresentationTransition::FadeIn);
        }
    }

#if ENABLE(PDFKIT_PLUGIN)
    if (is<HTMLPlugInImageElement>(element)) {
        if (auto* pluginView = static_cast<PluginView*>(downcast<HTMLPlugInImageElement>(*element).pluginWidget())) {
            // FIXME: We don't have API to identify images inside PDFs based on position.
            auto lookupResult = pluginView->lookupTextAtLocation(locationInViewCoordinates, immediateActionResult);
            if (auto lookupText = std::get<String>(lookupResult); !lookupText.isEmpty()) {
                // FIXME (144030): Focus does not seem to get set to the PDF when invoking the menu.
                auto& document = element->document();
                if (is<PluginDocument>(document))
                    downcast<PluginDocument>(document).setFocusedElement(element);

                auto selection = std::get<PDFSelection *>(lookupResult);
                auto options = std::get<NSDictionary *>(lookupResult);

                immediateActionResult.lookupText = lookupText;
                immediateActionResult.isTextNode = true;
                immediateActionResult.isSelected = true;
                immediateActionResult.dictionaryPopupInfo = dictionaryPopupInfoForSelectionInPDFPlugin(selection, *pluginView, options, TextIndicatorPresentationTransition::FadeIn);
            }
        }
    }
#endif

    RefPtr<API::Object> userData;
    injectedBundleContextMenuClient().prepareForImmediateAction(*this, hitTestResult, userData);

    send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(immediateActionResult, immediateActionHitTestPreventsDefault, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

std::optional<std::tuple<WebCore::SimpleRange, NSDictionary *>> WebPage::lookupTextAtLocation(FloatPoint locationInViewCoordinates)
{
    auto& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView())
        return std::nullopt;

    return DictionaryLookup::rangeAtHitTestResult(mainFrame.eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(roundedIntPoint(locationInViewCoordinates)), {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::DisallowUserAgentShadowContentExceptForImageOverlays,
        HitTestRequest::Type::AllowChildFrameContent,
    }));
}

void WebPage::immediateActionDidUpdate()
{
    m_page->mainFrame().eventHandler().setImmediateActionStage(ImmediateActionStage::ActionUpdated);
}

void WebPage::immediateActionDidCancel()
{
    ImmediateActionStage lastStage = m_page->mainFrame().eventHandler().immediateActionStage();
    if (lastStage == ImmediateActionStage::ActionUpdated)
        m_page->mainFrame().eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCancelledAfterUpdate);
    else
        m_page->mainFrame().eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCancelledWithoutUpdate);
}

void WebPage::immediateActionDidComplete()
{
    m_page->mainFrame().eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCompleted);
}

void WebPage::dataDetectorsDidPresentUI(PageOverlay::PageOverlayID overlayID)
{
    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (WebPageOverlay* webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidPresentUI();
            return;
        }
    }
}

void WebPage::dataDetectorsDidChangeUI(PageOverlay::PageOverlayID overlayID)
{
    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (WebPageOverlay* webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidChangeUI();
            return;
        }
    }
}

void WebPage::dataDetectorsDidHideUI(PageOverlay::PageOverlayID overlayID)
{
    auto& mainFrame = corePage()->mainFrame();

    // Dispatching a fake mouse event will allow clients to display any UI that is normally displayed on hover.
    mainFrame.eventHandler().dispatchFakeMouseMoveEventSoon();

    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (WebPageOverlay* webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidHideUI();
            return;
        }
    }
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo&, MonotonicTime)
{
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
void WebPage::playbackTargetSelected(PlaybackTargetClientContextIdentifier contextId, WebCore::MediaPlaybackTargetContext&& targetContext) const
{
    switch (targetContext.type()) {
    case MediaPlaybackTargetContext::Type::AVOutputContext:
    case MediaPlaybackTargetContext::Type::SerializedAVOutputContext:
        m_page->setPlaybackTarget(contextId, MediaPlaybackTargetCocoa::create(WTFMove(targetContext)));
        break;
    case MediaPlaybackTargetContext::Type::Mock:
        m_page->setPlaybackTarget(contextId, MediaPlaybackTargetMock::create(targetContext.deviceName(), targetContext.mockState()));
        break;
    case MediaPlaybackTargetContext::Type::None:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebPage::playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier contextId, bool changed)
{
    m_page->playbackTargetAvailabilityDidChange(contextId, changed);
}

void WebPage::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    m_page->setShouldPlayToPlaybackTarget(contextId, shouldPlay);
}

void WebPage::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    m_page->playbackTargetPickerWasDismissed(contextId);
}
#endif

void WebPage::didEndMagnificationGesture()
{
#if ENABLE(MAC_GESTURE_EVENTS)
    m_page->mainFrame().eventHandler().didEndMagnificationGesture();
#endif
}

bool WebPage::shouldAvoidComputingPostLayoutDataForEditorState() const
{
    if (m_needsFontAttributes) {
        // Font attribute information is propagated to the UI process through post-layout data on EditorState.
        return false;
    }

    if (!m_requiresUserActionForEditingControlsManager || m_hasEverFocusedElementDueToUserInteractionSincePageTransition) {
        // Text editing controls on the touch bar depend on having post-layout editor state data.
        return false;
    }

    return true;
}

#if HAVE(APP_ACCENT_COLORS)

void WebPage::setAccentColor(WebCore::Color color)
{
    [NSApp _setAccentColor:cocoaColorOrNil(color).get()];
}

#endif // HAVE(APP_ACCENT_COLORS)

#if ENABLE(UI_PROCESS_PDF_HUD)

void WebPage::zoomPDFIn(PDFPluginIdentifier identifier)
{
    auto pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return;
    pdfPlugin->zoomIn();
}

void WebPage::zoomPDFOut(PDFPluginIdentifier identifier)
{
    auto pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return;
    pdfPlugin->zoomOut();
}

void WebPage::savePDF(PDFPluginIdentifier identifier, CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&& completionHandler)
{
    auto pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return completionHandler({ }, { }, { });
    pdfPlugin->save(WTFMove(completionHandler));
}

void WebPage::openPDFWithPreview(PDFPluginIdentifier identifier, CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&& completionHandler)
{
    auto pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return completionHandler({ }, { }, { }, { });
    pdfPlugin->openWithPreview(WTFMove(completionHandler));
}

void WebPage::createPDFHUD(PDFPlugin& plugin, const IntRect& boundingBox)
{
    auto addResult = m_pdfPlugInsWithHUD.add(plugin.identifier(), plugin);
    if (addResult.isNewEntry)
        send(Messages::WebPageProxy::CreatePDFHUD(plugin.identifier(), boundingBox));
}

void WebPage::updatePDFHUDLocation(PDFPlugin& plugin, const IntRect& boundingBox)
{
    if (m_pdfPlugInsWithHUD.contains(plugin.identifier()))
        send(Messages::WebPageProxy::UpdatePDFHUDLocation(plugin.identifier(), boundingBox));
}

void WebPage::removePDFHUD(PDFPlugin& plugin)
{
    if (m_pdfPlugInsWithHUD.remove(plugin.identifier()))
        send(Messages::WebPageProxy::RemovePDFHUD(plugin.identifier()));
}

#endif // ENABLE(UI_PROCESS_PDF_HUD)

} // namespace WebKit

#endif // PLATFORM(MAC)
