/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#import "AttributedString.h"
#import "ContextMenuContextData.h"
#import "DataReference.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "InjectedBundleHitTestResult.h"
#import "PDFKitImports.h"
#import "PDFPlugin.h"
#import "PageBanner.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "UserData.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebHitTestResultData.h"
#import "WebImage.h"
#import "WebInspector.h"
#import "WebPageOverlay.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPreferencesStore.h"
#import "WebProcess.h"
#import <Quartz/Quartz.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/Editing.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsContext3D.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HTMLPlugInImageElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PluginDocument.h>
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
#import <pal/spi/mac/NSAccessibilitySPI.h>
#import <wtf/SetForScope.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import <WebCore/MediaPlaybackTargetMac.h>
#import <WebCore/MediaPlaybackTargetMock.h>
#endif

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitialize()
{
    WKAccessibilityWebPageObject* mockAccessibilityElement = [[[WKAccessibilityWebPageObject alloc] init] autorelease];

    // Get the pid for the starting process.
    pid_t pid = WebCore::presentingApplicationPID();
    // FIXME: WKAccessibilityWebPageObject doesn't respond to -accessibilitySetPresenterProcessIdentifier:.
    // Either it needs to or this call should be removed.
    if ([mockAccessibilityElement respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)mockAccessibilityElement accessibilitySetPresenterProcessIdentifier:pid];
    [mockAccessibilityElement setWebPage:this];
    m_mockAccessibilityElement = mockAccessibilityElement;

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

void WebPage::platformEditorState(Frame& frame, EditorState& result, IncludePostLayoutDataHint shouldIncludePostLayoutData) const
{
    if (shouldIncludePostLayoutData == IncludePostLayoutDataHint::No || !frame.view() || frame.view()->needsLayout() || !result.isContentEditable) {
        result.isMissingPostLayoutData = true;
        return;
    }

    const VisibleSelection& selection = frame.selection().selection();
    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return;

    auto& postLayoutData = result.postLayoutData();
    VisiblePosition selectionStart = selection.visibleStart();
    VisiblePosition selectionEnd = selection.visibleEnd();
    VisiblePosition paragraphStart = startOfParagraph(selectionStart);
    VisiblePosition paragraphEnd = endOfParagraph(selectionEnd);

    postLayoutData.candidateRequestStartPosition = TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
    postLayoutData.selectedTextLength = TextIterator::rangeLength(makeRange(paragraphStart, selectionEnd).get()) - postLayoutData.candidateRequestStartPosition;
    postLayoutData.paragraphContextForCandidateRequest = plainText(frame.editor().contextRangeForCandidateRequest().get());
    postLayoutData.stringForCandidateRequest = frame.editor().stringForCandidateRequest();

    IntRect rectForSelectionCandidates;
    Vector<FloatQuad> quads;
    selectedRange->absoluteTextQuads(quads);
    if (!quads.isEmpty())
        postLayoutData.focusedElementRect = frame.view()->contentsToWindow(quads[0].enclosingBoundingBox());
    else {
        // Range::absoluteTextQuads() will be empty at the start of a paragraph.
        if (selection.isCaret())
            postLayoutData.focusedElementRect = frame.view()->contentsToWindow(frame.selection().absoluteCaretBounds());
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
    
    if (auto* pluginView = pluginViewForFrame(&m_page->mainFrame()))
        return pluginView->accessibilityObject();

    return nil;
}

bool WebPage::shouldUsePDFPlugin() const
{
    return pdfPluginEnabled() && classFromPDFKit(@"PDFLayerController");
}

typedef HashMap<String, String> SelectorNameMap;

// Map selectors into Editor command names.
// This is not needed for any selectors that have the same name as the Editor command.
static const SelectorNameMap* createSelectorExceptionMap()
{
    SelectorNameMap* map = new HashMap<String, String>;

    map->add("insertNewlineIgnoringFieldEditor:", "InsertNewline");
    map->add("insertParagraphSeparator:", "InsertNewline");
    map->add("insertTabIgnoringFieldEditor:", "InsertTab");
    map->add("pageDown:", "MovePageDown");
    map->add("pageDownAndModifySelection:", "MovePageDownAndModifySelection");
    map->add("pageUp:", "MovePageUp");
    map->add("pageUpAndModifySelection:", "MovePageUpAndModifySelection");

    return map;
}

static String commandNameForSelectorName(const String& selectorName)
{
    // Check the exception map first.
    static const SelectorNameMap* exceptionMap = createSelectorExceptionMap();
    SelectorNameMap::const_iterator it = exceptionMap->find(selectorName);
    if (it != exceptionMap->end())
        return it->value;

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
        if (commands[i].commandName == "insertText:") {
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
                bool commandWasHandledByUIProcess = false;
                WebProcess::singleton().parentProcessConnection()->sendSync(Messages::WebPageProxy::ExecuteSavedCommandBySelector(commands[i].commandName),
                    Messages::WebPageProxy::ExecuteSavedCommandBySelector::Reply(commandWasHandledByUIProcess), m_pageID);
                eventWasHandled |= commandWasHandledByUIProcess;
            }
        }
    }
    return eventWasHandled;
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent* event)
{
    Frame* frame = frameForEvent(event);
    
    auto* platformEvent = event->underlyingPlatformEvent();
    if (!platformEvent)
        return false;
    auto& commands = event->keypressCommands();

    ASSERT(!platformEvent->macEvent()); // Cannot have a native event in WebProcess.

    // Don't handle Esc while handling keydown event, we need to dispatch a keypress first.
    if (platformEvent->type() != PlatformEvent::Char && platformEvent->windowsVirtualKeyCode() == VK_ESCAPE && commands.size() == 1 && commandNameForSelectorName(commands[0].commandName) == "cancelOperation")
        return false;

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
        eventWasHandled = executeKeypressCommandsInternal(commands, event);
        commands.clear();
    }

    return eventWasHandled;
}

void WebPage::sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    for (auto* pluginView : m_pluginViews) {
        if (pluginView->sendComplexTextInput(pluginComplexTextInputIdentifier, textInput))
            break;
    }
}

void WebPage::insertDictatedTextAsync(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool registerUndoGroup)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    Ref<Frame> protector(frame);

    if (replacementEditingRange.location != notFound) {
        RefPtr<Range> replacementRange = EditingRange::toRange(frame, replacementEditingRange);
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(*replacementRange, SEL_DEFAULT_AFFINITY));
    }

    if (registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());
    
    ASSERT(!frame.editor().hasComposition());
    frame.editor().insertDictatedText(text, dictationAlternativeLocations, nullptr);
}

void WebPage::attributedSubstringForCharacterRangeAsync(const EditingRange& editingRange, CallbackID callbackID)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone() || !selection.isContentEditable() || selection.isInPasswordField()) {
        send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback({ }, EditingRange(), callbackID));
        return;
    }

    RefPtr<Range> range = EditingRange::toRange(frame, editingRange);
    if (!range) {
        send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback({ }, EditingRange(), callbackID));
        return;
    }

    NSAttributedString *attributedString = editingAttributedStringFromRange(*range, IncludeImagesInAttributedString::No);
    
    // WebCore::editingAttributedStringFromRange() insists on inserting a trailing
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([attributedString length] > editingRange.length) {
        ASSERT([attributedString length] == editingRange.length + 1);
        ASSERT([[attributedString string] characterAtIndex:editingRange.length] == '\n' || [[attributedString string] characterAtIndex:editingRange.length] == ' ');
        attributedString = [attributedString attributedSubstringFromRange:NSMakeRange(0, editingRange.length)];
    }

    EditingRange rangeToSend(editingRange.location, attributedString.length);
    ASSERT(rangeToSend.isValid());
    if (!rangeToSend.isValid()) {
        // Send an empty EditingRange as a last resort for <rdar://problem/27078089>.
        send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback(attributedString, EditingRange(), callbackID));
        return;
    }

    send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback(attributedString, rangeToSend, callbackID));
}

void WebPage::fontAtSelection(CallbackID callbackID)
{
    String fontName;
    double fontSize = 0;
    bool selectionHasMultipleFonts = false;
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    
    if (!frame.selection().selection().isNone()) {
        if (auto* font = frame.editor().fontForSelection(selectionHasMultipleFonts)) {
            if (auto ctFont = font->getCTFont()) {
                fontName = adoptCF(CTFontCopyPostScriptName(ctFont)).get();
                fontSize = CTFontGetSize(ctFont);
            }
        }
    }
    send(Messages::WebPageProxy::FontAtSelectionCallback(fontName, fontSize, selectionHasMultipleFonts, callbackID));
}
    


#if ENABLE(PDFKIT_PLUGIN)

DictionaryPopupInfo WebPage::dictionaryPopupInfoForSelectionInPDFPlugin(PDFSelection *selection, PDFPlugin& pdfPlugin, NSDictionary *options, WebCore::TextIndicatorPresentationTransition presentationTransition)
{
    DictionaryPopupInfo dictionaryPopupInfo;
    if (!selection.string.length)
        return dictionaryPopupInfo;

    NSRect rangeRect = pdfPlugin.rectForSelectionInRootView(selection);

    NSAttributedString *nsAttributedString = selection.attributedString;
    
    RetainPtr<NSMutableAttributedString> scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);
    
    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    CGFloat scaleFactor = pdfPlugin.scaleFactor();

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
    dictionaryPopupInfo.options = options;
    dictionaryPopupInfo.textIndicator = dataForSelection;
    dictionaryPopupInfo.attributedString = scaledNSAttributedString;
    
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

    if (selector == "moveUp:")
        didPerformAction = scroll(m_page.get(), ScrollUp, ScrollByLine);
    else if (selector == "moveToBeginningOfParagraph:")
        didPerformAction = scroll(m_page.get(), ScrollUp, ScrollByPage);
    else if (selector == "moveToBeginningOfDocument:") {
        didPerformAction = scroll(m_page.get(), ScrollUp, ScrollByDocument);
        didPerformAction |= scroll(m_page.get(), ScrollLeft, ScrollByDocument);
    } else if (selector == "moveDown:")
        didPerformAction = scroll(m_page.get(), ScrollDown, ScrollByLine);
    else if (selector == "moveToEndOfParagraph:")
        didPerformAction = scroll(m_page.get(), ScrollDown, ScrollByPage);
    else if (selector == "moveToEndOfDocument:") {
        didPerformAction = scroll(m_page.get(), ScrollDown, ScrollByDocument);
        didPerformAction |= scroll(m_page.get(), ScrollLeft, ScrollByDocument);
    } else if (selector == "moveLeft:")
        didPerformAction = scroll(m_page.get(), ScrollLeft, ScrollByLine);
    else if (selector == "moveWordLeft:")
        didPerformAction = scroll(m_page.get(), ScrollLeft, ScrollByPage);
    else if (selector == "moveToLeftEndOfLine:")
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? m_page->backForward().goBack() : m_page->backForward().goForward();
    else if (selector == "moveRight:")
        didPerformAction = scroll(m_page.get(), ScrollRight, ScrollByLine);
    else if (selector == "moveWordRight:")
        didPerformAction = scroll(m_page.get(), ScrollRight, ScrollByPage);
    else if (selector == "moveToRightEndOfLine:")
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? m_page->backForward().goForward() : m_page->backForward().goBack();

    return didPerformAction;
}

#if ENABLE(SERVICE_CONTROLS)
static String& replaceSelectionPasteboardName()
{
    static NeverDestroyed<String> string("ReplaceSelectionPasteboard");
    return string;
}

void WebPage::replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference& data)
{
    for (auto& type : types)
        WebPasteboardOverrides::sharedPasteboardOverrides().addOverride(replaceSelectionPasteboardName(), type, data.vector());

    bool result;
    readSelectionFromPasteboard(replaceSelectionPasteboardName(), result);

    for (auto& type : types)
        WebPasteboardOverrides::sharedPasteboardOverrides().removeOverride(replaceSelectionPasteboardName(), type);
}
#endif

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

void WebPage::readSelectionFromPasteboard(const String& pasteboardName, bool& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone()) {
        result = false;
        return;
    }
    frame.editor().readSelectionFromPasteboard(pasteboardName);
    result = true;
}

void WebPage::getStringSelectionForPasteboard(String& stringValue)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (PluginView* pluginView = focusedPluginViewForFrame(frame)) {
        String selection = pluginView->getSelectionString();
        if (!selection.isNull()) {
            stringValue = selection;
            return;
        }
    }

    if (frame.selection().isNone())
        return;

    stringValue = frame.editor().stringSelectionForPasteboard();
}

void WebPage::getDataSelectionForPasteboard(const String pasteboardType, SharedMemory::Handle& handle, uint64_t& size)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    if (frame.selection().isNone())
        return;

    RefPtr<SharedBuffer> buffer = frame.editor().dataSelectionForPasteboard(pasteboardType);
    if (!buffer) {
        size = 0;
        return;
    }
    size = buffer->size();
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::allocate(size);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    sharedMemoryBuffer->createHandle(handle, SharedMemory::Protection::ReadOnly);
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
    return request.url().protocolIs("applewebdata");
}

void WebPage::shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent& event, bool& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

#if ENABLE(DRAG_SUPPORT)
    HitTestResult hitResult = frame.eventHandler().hitTestResultAtPoint(frame.view()->windowToContents(event.position()), HitTestRequest::ReadOnly | HitTestRequest::Active);
    if (hitResult.isSelected())
        result = frame.eventHandler().eventMayStartDrag(platform(event));
    else
#endif
        result = false;
}

void WebPage::acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent& event, bool& result)
{
    result = false;

    if (WebProcess::singleton().parentProcessConnection()->inSendSync()) {
        // In case we're already inside a sendSync message, it's possible that the page is in a
        // transitionary state, so any hit-testing could cause crashes  so we just return early in that case.
        return;
    }

    Frame& frame = m_page->focusController().focusedOrMainFrame();

    HitTestResult hitResult = frame.eventHandler().hitTestResultAtPoint(frame.view()->windowToContents(event.position()), HitTestRequest::ReadOnly | HitTestRequest::Active);
    frame.eventHandler().setActivationEventNumber(eventNumber);
#if ENABLE(DRAG_SUPPORT)
    if (hitResult.isSelected())
        result = frame.eventHandler().eventMayStartDrag(platform(event));
    else
#endif
        result = !!hitResult.scrollbar();
}

void WebPage::setTopOverhangImage(WebImage* image)
{
    auto* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    auto* layer = frameView->setWantsLayerForTopOverHangArea(image);
    if (!layer)
        return;

    layer->setSize(image->size());
    layer->setPosition(FloatPoint(0, -image->size().height()));
    layer->platformLayer().contents = (__bridge id)image->bitmap().makeCGImageCopy().get();
}

void WebPage::setBottomOverhangImage(WebImage* image)
{
    auto* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    auto* layer = frameView->setWantsLayerForBottomOverHangArea(image);
    if (!layer)
        return;

    layer->setSize(image->size());
    layer->platformLayer().contents = (__bridge id)image->bitmap().makeCGImageCopy().get();
}

void WebPage::updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor)
{    
    if (m_headerBanner)
        m_headerBanner->didChangeDeviceScaleFactor(scaleFactor);
    if (m_footerBanner)
        m_footerBanner->didChangeDeviceScaleFactor(scaleFactor);
}

void WebPage::computePagesForPrintingPDFDocument(uint64_t frameID, const PrintInfo& printInfo, Vector<IntRect>& resultPageRects)
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
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO]];
    [pdfPage drawWithBox:kPDFDisplayBoxCropBox];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [NSGraphicsContext restoreGraphicsState];

    CGAffineTransform transform = CGContextGetCTM(context);

    for (PDFAnnotation *annotation in [pdfPage annotations]) {
        if (![annotation isKindOfClass:pdfAnnotationLinkClass()])
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
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame* frame, const URL& url)
{
    uint32_t policyResult = 0;

    if (sendSync(Messages::WebPageProxy::WebGLPolicyForURL(url), Messages::WebPageProxy::WebGLPolicyForURL::Reply(policyResult)))
        return static_cast<WebGLLoadPolicy>(policyResult);

    return WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame* frame, const URL& url)
{
    uint32_t policyResult = 0;

    if (sendSync(Messages::WebPageProxy::ResolveWebGLPolicyForURL(url), Messages::WebPageProxy::ResolveWebGLPolicyForURL::Reply(policyResult)))
        return static_cast<WebGLLoadPolicy>(policyResult);

    return WebGLAllowCreation;
}
#endif // ENABLE(WEBGL)

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPage::handleTelephoneNumberClick(const String& number, const IntPoint& point)
{
    send(Messages::WebPageProxy::ShowTelephoneNumberMenu(number, point));
}
#endif

#if ENABLE(SERVICE_CONTROLS)
void WebPage::handleSelectionServiceClick(FrameSelection& selection, const Vector<String>& phoneNumbers, const IntPoint& point)
{
    RefPtr<Range> range = selection.selection().firstRange();
    if (!range)
        return;

    NSAttributedString *attributedSelection = attributedStringFromRange(*range);
    if (!attributedSelection)
        return;

    NSData *selectionData = [attributedSelection RTFDFromRange:NSMakeRange(0, attributedSelection.length) documentAttributes:@{ }];

    Vector<uint8_t> selectionDataVector;
    selectionDataVector.append(reinterpret_cast<const uint8_t*>(selectionData.bytes), selectionData.length);

    send(Messages::WebPageProxy::ShowContextMenu(ContextMenuContextData(point, selectionDataVector, phoneNumbers, selection.selection().isContentEditable()), UserData()));
}
#endif

String WebPage::platformUserAgent(const URL&) const
{
    return String();
}

void WebPage::performImmediateActionHitTestAtLocation(WebCore::FloatPoint locationInViewCoordinates)
{
    layoutIfNeeded();

    auto& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView()) {
        send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(WebHitTestResultData(), false, UserData()));
        return;
    }

    IntPoint locationInContentCoordinates = mainFrame.view()->rootViewToContents(roundedIntPoint(locationInViewCoordinates));
    HitTestResult hitTestResult = mainFrame.eventHandler().hitTestResultAtPoint(locationInContentCoordinates);

    bool immediateActionHitTestPreventsDefault = false;
    Element* element = hitTestResult.targetElement();

    mainFrame.eventHandler().setImmediateActionStage(ImmediateActionStage::PerformedHitTest);
    if (element)
        immediateActionHitTestPreventsDefault = element->dispatchMouseForceWillBegin();

    WebHitTestResultData immediateActionResult(hitTestResult);

    RefPtr<Range> selectionRange = corePage()->focusController().focusedOrMainFrame().selection().selection().firstRange();

    URL absoluteLinkURL = hitTestResult.absoluteLinkURL();
    Element* URLElement = hitTestResult.URLElement();
    if (!absoluteLinkURL.isEmpty() && URLElement)
        immediateActionResult.linkTextIndicator = TextIndicator::createWithRange(rangeOfContents(*URLElement), TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges, TextIndicatorPresentationTransition::FadeIn);

    auto lookupResult = lookupTextAtLocation(locationInViewCoordinates);
    if (auto* lookupRange = std::get<RefPtr<Range>>(lookupResult).get()) {
        immediateActionResult.lookupText = lookupRange->text();
        if (auto* node = hitTestResult.innerNode()) {
            if (auto* frame = node->document().frame()) {
                auto options = std::get<NSDictionary *>(lookupResult);
                immediateActionResult.dictionaryPopupInfo = dictionaryPopupInfoForRange(*frame, *lookupRange, options, TextIndicatorPresentationTransition::FadeIn);
            }
        }
    }

    bool pageOverlayDidOverrideDataDetectors = false;
    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        WebPageOverlay* webOverlay = WebPageOverlay::fromCoreOverlay(*overlay);
        if (!webOverlay)
            continue;

        RefPtr<Range> mainResultRange;
        DDActionContext *actionContext = webOverlay->actionContextForResultAtPoint(locationInContentCoordinates, mainResultRange);
        if (!actionContext || !mainResultRange)
            continue;

        pageOverlayDidOverrideDataDetectors = true;
        immediateActionResult.detectedDataActionContext = actionContext;

        Vector<FloatQuad> quads;
        mainResultRange->absoluteTextQuads(quads);
        FloatRect detectedDataBoundingBox;
        FrameView* frameView = mainResultRange->ownerDocument().view();
        for (const auto& quad : quads)
            detectedDataBoundingBox.unite(frameView->contentsToWindow(quad.enclosingBoundingBox()));

        immediateActionResult.detectedDataBoundingBox = detectedDataBoundingBox;
        immediateActionResult.detectedDataTextIndicator = TextIndicator::createWithRange(*mainResultRange, TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges, TextIndicatorPresentationTransition::FadeIn);
        immediateActionResult.detectedDataOriginatingPageOverlay = overlay->pageOverlayID();

        break;
    }

    // FIXME: Avoid scanning if we will just throw away the result (e.g. we're over a link).
    if (!pageOverlayDidOverrideDataDetectors && hitTestResult.innerNode() && (hitTestResult.innerNode()->isTextNode() || hitTestResult.isOverTextInsideFormControlElement())) {
        FloatRect detectedDataBoundingBox;
        RefPtr<Range> detectedDataRange;
        immediateActionResult.detectedDataActionContext = DataDetection::detectItemAroundHitTestResult(hitTestResult, detectedDataBoundingBox, detectedDataRange);
        if (immediateActionResult.detectedDataActionContext && detectedDataRange) {
            immediateActionResult.detectedDataBoundingBox = detectedDataBoundingBox;
            immediateActionResult.detectedDataTextIndicator = TextIndicator::createWithRange(*detectedDataRange, TextIndicatorOptionUseBoundingRectAndPaintAllContentForComplexRanges, TextIndicatorPresentationTransition::FadeIn);
        }
    }

#if ENABLE(PDFKIT_PLUGIN)
    if (is<HTMLPlugInImageElement>(element)) {
        if (auto* pluginView = static_cast<PluginView*>(downcast<HTMLPlugInImageElement>(*element).pluginWidget())) {
            if (is<PDFPlugin>(pluginView->plugin())) {
                // FIXME: We don't have API to identify images inside PDFs based on position.
                auto& plugIn = downcast<PDFPlugin>(*pluginView->plugin());
                auto lookupResult = plugIn.lookupTextAtLocation(locationInViewCoordinates, immediateActionResult);
                auto lookupText = std::get<String>(lookupResult);
                if (!lookupText.isEmpty()) {
                    // FIXME (144030): Focus does not seem to get set to the PDF when invoking the menu.
                    auto& document = element->document();
                    if (is<PluginDocument>(document))
                        downcast<PluginDocument>(document).setFocusedElement(element);

                    auto selection = std::get<PDFSelection *>(lookupResult);
                    auto options = std::get<NSDictionary *>(lookupResult);

                    immediateActionResult.lookupText = lookupText;
                    immediateActionResult.isTextNode = true;
                    immediateActionResult.isSelected = true;
                    immediateActionResult.dictionaryPopupInfo = dictionaryPopupInfoForSelectionInPDFPlugin(selection, plugIn, options, TextIndicatorPresentationTransition::FadeIn);
                }
            }
        }
    }
#endif

    RefPtr<API::Object> userData;
    injectedBundleContextMenuClient().prepareForImmediateAction(*this, hitTestResult, userData);

    send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(immediateActionResult, immediateActionHitTestPreventsDefault, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

std::tuple<RefPtr<WebCore::Range>, NSDictionary *> WebPage::lookupTextAtLocation(FloatPoint locationInViewCoordinates)
{
    auto& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView())
        return { nullptr, nil };

    auto point = roundedIntPoint(locationInViewCoordinates);
    auto result = mainFrame.eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(point));
    return DictionaryLookup::rangeAtHitTestResult(result);
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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
void WebPage::playbackTargetSelected(uint64_t contextId, const WebCore::MediaPlaybackTargetContext& targetContext) const
{
    switch (targetContext.type()) {
    case MediaPlaybackTargetContext::AVOutputContextType:
        m_page->setPlaybackTarget(contextId, WebCore::MediaPlaybackTargetMac::create(targetContext.avOutputContext()));
        break;
    case MediaPlaybackTargetContext::MockType:
        m_page->setPlaybackTarget(contextId, WebCore::MediaPlaybackTargetMock::create(targetContext.mockDeviceName(), targetContext.mockState()));
        break;
    case MediaPlaybackTargetContext::None:
        ASSERT_NOT_REACHED();
        break;
    }
}

void WebPage::playbackTargetAvailabilityDidChange(uint64_t contextId, bool changed)
{
    m_page->playbackTargetAvailabilityDidChange(contextId, changed);
}

void WebPage::setShouldPlayToPlaybackTarget(uint64_t contextId, bool shouldPlay)
{
    m_page->setShouldPlayToPlaybackTarget(contextId, shouldPlay);
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC)
