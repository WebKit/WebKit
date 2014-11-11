/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#import "ActionMenuHitTestResult.h"
#import "AttributedString.h"
#import "DataReference.h"
#import "DictionaryPopupInfo.h"
#import "EditingRange.h"
#import "EditorState.h"
#import "InjectedBundleHitTestResult.h"
#import "InjectedBundleUserMessageCoders.h"
#import "PDFKitImports.h"
#import "PageBanner.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebImage.h"
#import "WebInspector.h"
#import "WebPageOverlay.h"
#import "WebPageProxyMessages.h"
#import "WebPasteboardOverrides.h"
#import "WebPreferencesStore.h"
#import "WebProcess.h"
#import <PDFKit/PDFKit.h>
#import <QuartzCore/QuartzCore.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/BackForwardController.h>
#import <WebCore/DataDetectorsSPI.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/HTMLConverter.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/MainFrame.h>
#import <WebCore/NetworkingContext.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderStyle.h>
#import <WebCore/RenderView.h>
#import <WebCore/ResourceHandle.h>
#import <WebCore/ScrollView.h>
#import <WebCore/StyleInheritedData.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <WebCore/htmlediting.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
#if USE(CFNETWORK)
    m_page->addSchedulePair(SchedulePair::create([[NSRunLoop currentRunLoop] getCFRunLoop], kCFRunLoopCommonModes));
#else
    m_page->addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));
#endif

    WKAccessibilityWebPageObject* mockAccessibilityElement = [[[WKAccessibilityWebPageObject alloc] init] autorelease];

    // Get the pid for the starting process.
    pid_t pid = WebProcess::shared().presenterApplicationPid();    
    WKAXInitializeElementWithPresenterPid(mockAccessibilityElement, pid);
    [mockAccessibilityElement setWebPage:this];
    
    // send data back over
    NSData* remoteToken = (NSData *)WKAXRemoteTokenForElement(mockAccessibilityElement); 
    IPC::DataReference dataToken = IPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
    m_mockAccessibilityElement = mockAccessibilityElement;
}

NSObject *WebPage::accessibilityObjectForMainFramePlugin()
{
    if (!m_page)
        return 0;
    
    if (PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame()))
        return pluginView->accessibilityObject();

    return 0;
}

void WebPage::platformPreferencesDidChange(const WebPreferencesStore& store)
{
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
    Node* node = event->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document().frame();
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
                    bool performedNonEditingBehavior = event->keyEvent()->type() == PlatformEvent::RawKeyDown && performNonEditingBehaviorForSelector(commands[i].commandName, event);
                    eventWasHandled |= performedNonEditingBehavior;
                }
            } else {
                bool commandWasHandledByUIProcess = false;
                WebProcess::shared().parentProcessConnection()->sendSync(Messages::WebPageProxy::ExecuteSavedCommandBySelector(commands[i].commandName), 
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
    
    const PlatformKeyboardEvent* platformEvent = event->keyEvent();
    if (!platformEvent)
        return false;
    const Vector<KeypressCommand>& commands = event->keypressCommands();

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
        eventWasHandled = executeKeypressCommandsInternal(event->keypressCommands(), event);
        event->keypressCommands().clear();
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

#if !USE(ASYNC_NSTEXTINPUTCLIENT)

void WebPage::setComposition(const String& text, Vector<CompositionUnderline> underlines, const EditingRange& selectionRange, const EditingRange& replacementEditingRange, EditorState& newState)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (frame.selection().selection().isContentEditable()) {
        RefPtr<Range> replacementRange;
        if (replacementEditingRange.location != notFound) {
            replacementRange = rangeFromEditingRange(frame, replacementEditingRange);
            frame.selection().setSelection(VisibleSelection(replacementRange.get(), SEL_DEFAULT_AFFINITY));
        }

        frame.editor().setComposition(text, underlines, selectionRange.location, selectionRange.location + selectionRange.length);
    }

    newState = editorState();
}

void WebPage::confirmComposition(EditorState& newState)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().confirmComposition();
    newState = editorState();
}

void WebPage::insertText(const String& text, const EditingRange& replacementEditingRange, bool& handled, EditorState& newState)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (replacementEditingRange.location != notFound) {
        RefPtr<Range> replacementRange = rangeFromEditingRange(frame, replacementEditingRange);
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(replacementRange.get(), SEL_DEFAULT_AFFINITY));
    }

    if (!frame.editor().hasComposition()) {
        // An insertText: might be handled by other responders in the chain if we don't handle it.
        // One example is space bar that results in scrolling down the page.
        handled = frame.editor().insertText(text, nullptr);
    } else {
        handled = true;
        frame.editor().confirmComposition(text);
    }

    newState = editorState();
}

void WebPage::insertDictatedText(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool& handled, EditorState& newState)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (replacementEditingRange.location != notFound) {
        RefPtr<Range> replacementRange = rangeFromEditingRange(frame, replacementEditingRange);
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(replacementRange.get(), SEL_DEFAULT_AFFINITY));
    }

    ASSERT(!frame.editor().hasComposition());
    handled = frame.editor().insertDictatedText(text, dictationAlternativeLocations, nullptr);
    newState = editorState();
}

void WebPage::getMarkedRange(EditingRange& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    RefPtr<Range> range = frame.editor().compositionRange();
    size_t locationSize;
    size_t lengthSize;
    if (range && TextIterator::getLocationAndLengthFromRange(frame.selection().rootEditableElementOrDocumentElement(), range.get(), locationSize, lengthSize))
        result = EditingRange(static_cast<uint64_t>(locationSize), static_cast<uint64_t>(lengthSize));
    else
        result = EditingRange();
}

void WebPage::getSelectedRange(EditingRange& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    size_t locationSize;
    size_t lengthSize;
    RefPtr<Range> range = frame.selection().toNormalizedRange();
    if (range && TextIterator::getLocationAndLengthFromRange(frame.selection().rootEditableElementOrDocumentElement(), range.get(), locationSize, lengthSize))
        result = EditingRange(static_cast<uint64_t>(locationSize), static_cast<uint64_t>(lengthSize));
    else
        result = EditingRange();
}

void WebPage::getAttributedSubstringFromRange(const EditingRange& editingRange, AttributedString& result)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone() || !selection.isContentEditable() || selection.isInPasswordField())
        return;

    RefPtr<Range> range = rangeFromEditingRange(frame, editingRange);
    if (!range)
        return;

    result.string = editingAttributedStringFromRange(*range);
    NSAttributedString* attributedString = result.string.get();
    
    // WebCore::editingAttributedStringFromRange() insists on inserting a trailing
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([attributedString length] > editingRange.length) {
        ASSERT([attributedString length] == editingRange.length + 1);
        ASSERT([[attributedString string] characterAtIndex:editingRange.length] == '\n' || [[attributedString string] characterAtIndex:editingRange.length] == ' ');
        result.string = [attributedString attributedSubstringFromRange:NSMakeRange(0, editingRange.length)];
    }
}

void WebPage::characterIndexForPoint(IntPoint point, uint64_t& index)
{
    index = notFound;

    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(point);
    Frame* frame = result.innerNonSharedNode() ? result.innerNodeFrame() : &m_page->focusController().focusedOrMainFrame();
    
    RefPtr<Range> range = frame->rangeForPoint(result.roundedPointInInnerNodeFrame());
    if (!range)
        return;

    size_t location;
    size_t length;
    if (TextIterator::getLocationAndLengthFromRange(frame->selection().rootEditableElementOrDocumentElement(), range.get(), location, length))
        index = static_cast<uint64_t>(location);
}
    
void WebPage::firstRectForCharacterRange(const EditingRange& editingRange, WebCore::IntRect& resultRect)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    resultRect.setLocation(IntPoint(0, 0));
    resultRect.setSize(IntSize(0, 0));
    
    RefPtr<Range> range = rangeFromEditingRange(frame, editingRange);
    if (!range)
        return;
    
    ASSERT(range->startContainer());
    ASSERT(range->endContainer());
     
    IntRect rect = frame.editor().firstRectForRange(range.get());
    resultRect = frame.view()->contentsToWindow(rect);
}

void WebPage::executeKeypressCommands(const Vector<WebCore::KeypressCommand>& commands, bool& handled, EditorState& newState)
{
    handled = executeKeypressCommandsInternal(commands, nullptr);
    newState = editorState();
}

void WebPage::cancelComposition(EditorState& newState)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();
    frame.editor().cancelComposition();
    newState = editorState();
}

#endif // !USE(ASYNC_NSTEXTINPUTCLIENT)

void WebPage::insertDictatedTextAsync(const String& text, const EditingRange& replacementEditingRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, bool registerUndoGroup)
{
    Frame& frame = m_page->focusController().focusedOrMainFrame();

    if (replacementEditingRange.location != notFound) {
        RefPtr<Range> replacementRange = rangeFromEditingRange(frame, replacementEditingRange);
        if (replacementRange)
            frame.selection().setSelection(VisibleSelection(replacementRange.get(), SEL_DEFAULT_AFFINITY));
    }

    if (registerUndoGroup)
        send(Messages::WebPageProxy::RegisterInsertionUndoGrouping());
    
    ASSERT(!frame.editor().hasComposition());
    frame.editor().insertDictatedText(text, dictationAlternativeLocations, nullptr);
}

void WebPage::attributedSubstringForCharacterRangeAsync(const EditingRange& editingRange, uint64_t callbackID)
{
    AttributedString result;

    Frame& frame = m_page->focusController().focusedOrMainFrame();

    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNone() || !selection.isContentEditable() || selection.isInPasswordField()) {
        send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback(result, EditingRange(), callbackID));
        return;
    }

    RefPtr<Range> range = rangeFromEditingRange(frame, editingRange);
    if (!range) {
        send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback(result, EditingRange(), callbackID));
        return;
    }

    result.string = editingAttributedStringFromRange(*range);
    NSAttributedString* attributedString = result.string.get();
    
    // WebCore::editingAttributedStringFromRange() insists on inserting a trailing
    // whitespace at the end of the string which breaks the ATOK input method.  <rdar://problem/5400551>
    // To work around this we truncate the resultant string to the correct length.
    if ([attributedString length] > editingRange.length) {
        ASSERT([attributedString length] == editingRange.length + 1);
        ASSERT([[attributedString string] characterAtIndex:editingRange.length] == '\n' || [[attributedString string] characterAtIndex:editingRange.length] == ' ');
        result.string = [attributedString attributedSubstringFromRange:NSMakeRange(0, editingRange.length)];
    }

    send(Messages::WebPageProxy::AttributedStringForCharacterRangeCallback(result, EditingRange(editingRange.location, [result.string length]), callbackID));
}

static bool isPositionInRange(const VisiblePosition& position, Range* range)
{
    RefPtr<Range> positionRange = makeRange(position, position);

    ExceptionCode ec = 0;
    range->compareBoundaryPoints(Range::START_TO_START, positionRange.get(), ec);
    if (ec)
        return false;

    if (!range->isPointInRange(positionRange->startContainer(), positionRange->startOffset(), ec))
        return false;
    if (ec)
        return false;

    return true;
}

static bool shouldUseSelection(const VisiblePosition& position, const VisibleSelection& selection)
{
    if (!selection.isRange())
        return false;

    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return false;

    return isPositionInRange(position, selectedRange.get());
}

static PassRefPtr<Range> rangeExpandedAroundPositionByCharacters(const VisiblePosition& position, int numberOfCharactersToExpand)
{
    Position start = position.deepEquivalent();
    Position end = position.deepEquivalent();
    for (int i = 0; i < numberOfCharactersToExpand; ++i) {
        if (directionOfEnclosingBlock(start) == LTR)
            start = start.previous(Character);
        else
            start = start.next(Character);

        if (directionOfEnclosingBlock(end) == LTR)
            end = end.next(Character);
        else
            end = end.previous(Character);
    }

    return makeRange(start, end);
}

static PassRefPtr<Range> rangeForDictionaryLookupForSelection(const VisibleSelection& selection, NSDictionary **options)
{
    RefPtr<Range> selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return nullptr;

    VisiblePosition selectionStart = selection.visibleStart();
    VisiblePosition selectionEnd = selection.visibleEnd();

    // As context, we are going to use the surrounding paragraphs of text.
    VisiblePosition paragraphStart = startOfParagraph(selectionStart);
    VisiblePosition paragraphEnd = endOfParagraph(selectionEnd);

    int lengthToSelectionStart = TextIterator::rangeLength(makeRange(paragraphStart, selectionStart).get());
    int lengthToSelectionEnd = TextIterator::rangeLength(makeRange(paragraphStart, selectionEnd).get());
    NSRange rangeToPass = NSMakeRange(lengthToSelectionStart, lengthToSelectionEnd - lengthToSelectionStart);

    String fullPlainTextString = plainText(makeRange(paragraphStart, paragraphEnd).get());

    // Since we already have the range we want, we just need to grab the returned options.
    WKExtractWordDefinitionTokenRangeFromContextualString(fullPlainTextString, rangeToPass, options);
    
    return selectedRange.release();
}

static PassRefPtr<Range> rangeForDictionaryLookupAtHitTestResult(const HitTestResult& hitTestResult, NSDictionary **options)
{
    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return nullptr;

    auto renderer = node->renderer();
    if (!renderer)
        return nullptr;

    Frame* frame = node->document().frame();
    if (!frame)
        return nullptr;

    // Don't do anything if there is no character at the point.
    if (!frame->rangeForPoint(hitTestResult.roundedPointInInnerNodeFrame()))
        return nullptr;

    VisiblePosition position = renderer->positionForPoint(hitTestResult.localPoint(), nullptr);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    VisibleSelection selection = frame->page()->focusController().focusedOrMainFrame().selection().selection();
    if (shouldUseSelection(position, selection))
        return rangeForDictionaryLookupForSelection(selection, options);

    // As context, we are going to use 250 characters of text before and after the point.
    RefPtr<Range> fullCharacterRange = rangeExpandedAroundPositionByCharacters(position, 250);
    if (!fullCharacterRange)
        return nullptr;

    NSRange rangeToPass = NSMakeRange(TextIterator::rangeLength(makeRange(fullCharacterRange->startPosition(), position).get()), 0);

    String fullPlainTextString = plainText(fullCharacterRange.get());

    NSRange extractedRange = WKExtractWordDefinitionTokenRangeFromContextualString(fullPlainTextString, rangeToPass, options);

    // This function sometimes returns {NSNotFound, 0} if it was unable to determine a good string.
    if (extractedRange.location == NSNotFound)
        return nullptr;

    return TextIterator::subrange(fullCharacterRange.get(), extractedRange.location, extractedRange.length);
}

void WebPage::performDictionaryLookupAtLocation(const FloatPoint& floatPoint)
{
    if (PluginView* pluginView = pluginViewForFrame(&m_page->mainFrame())) {
        if (pluginView->performDictionaryLookupAtLocation(floatPoint))
            return;
    }

    // Find the frame the point is over.
    IntPoint point = roundedIntPoint(floatPoint);
    HitTestResult result = m_page->mainFrame().eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(point));
    Frame* frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document().frame() : &m_page->focusController().focusedOrMainFrame();
    NSDictionary *options = nil;
    RefPtr<Range> range = rangeForDictionaryLookupAtHitTestResult(result, &options);
    if (!range)
        return;

    performDictionaryLookupForRange(frame, *range, options);
}

void WebPage::performDictionaryLookupForSelection(Frame* frame, const VisibleSelection& selection)
{
    NSDictionary *options = nil;
    RefPtr<Range> selectedRange = rangeForDictionaryLookupForSelection(selection, &options);
    performDictionaryLookupForRange(frame, *selectedRange, options);
}

void WebPage::performDictionaryLookupOfCurrentSelection()
{
    Frame* frame = &m_page->focusController().focusedOrMainFrame();
    performDictionaryLookupForSelection(frame, frame->selection().selection());
}

void WebPage::performDictionaryLookupForRange(Frame* frame, Range& range, NSDictionary *options)
{
    if (range.text().stripWhiteSpace().isEmpty())
        return;
    
    RenderObject* renderer = range.startContainer()->renderer();
    const RenderStyle& style = renderer->style();

    Vector<FloatQuad> quads;
    range.textQuads(quads);
    if (quads.isEmpty())
        return;

    IntRect rangeRect = frame->view()->contentsToWindow(quads[0].enclosingBoundingBox());
    
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y() + (style.fontMetrics().ascent() * pageScaleFactor()));
    dictionaryPopupInfo.options = (CFDictionaryRef)options;

    NSAttributedString *nsAttributedString = editingAttributedStringFromRange(range);

    RetainPtr<NSMutableAttributedString> scaledNSAttributedString = adoptNS([[NSMutableAttributedString alloc] initWithString:[nsAttributedString string]]);

    NSFontManager *fontManager = [NSFontManager sharedFontManager];

    [nsAttributedString enumerateAttributesInRange:NSMakeRange(0, [nsAttributedString length]) options:0 usingBlock:^(NSDictionary *attributes, NSRange range, BOOL *stop) {
        RetainPtr<NSMutableDictionary> scaledAttributes = adoptNS([attributes mutableCopy]);

        NSFont *font = [scaledAttributes objectForKey:NSFontAttributeName];
        if (font) {
            font = [fontManager convertFont:font toSize:[font pointSize] * pageScaleFactor()];
            [scaledAttributes setObject:font forKey:NSFontAttributeName];
        }

        [scaledNSAttributedString addAttributes:scaledAttributes.get() range:range];
    }];

    AttributedString attributedString;
    attributedString.string = scaledNSAttributedString;

    send(Messages::WebPageProxy::DidPerformDictionaryLookup(attributedString, dictionaryPopupInfo));
}

bool WebPage::performNonEditingBehaviorForSelector(const String& selector, KeyboardEvent* event)
{
    // First give accessibility a chance to handle the event.
    Frame* frame = frameForEvent(event);
    frame->eventHandler().handleKeyboardSelectionMovementForAccessibility(event);
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
        didPerformAction = m_page->backForward().goBack();
    else if (selector == "moveRight:")
        didPerformAction = scroll(m_page.get(), ScrollRight, ScrollByLine);
    else if (selector == "moveWordRight:")
        didPerformAction = scroll(m_page.get(), ScrollRight, ScrollByPage);
    else if (selector == "moveToRightEndOfLine:")
        didPerformAction = m_page->backForward().goForward();

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
    NSData* elementTokenData = [NSData dataWithBytes:elementToken.data() length:elementToken.size()];
    NSData* windowTokenData = [NSData dataWithBytes:windowToken.data() length:windowToken.size()];
    id remoteElement = WKAXRemoteElementForToken(elementTokenData);
    id remoteWindow = WKAXRemoteElementForToken(windowTokenData);
    WKAXSetWindowForRemoteElement(remoteWindow, remoteElement);
    
    [accessibilityRemoteObject() setRemoteParent:remoteElement];
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
    RefPtr<SharedMemory> sharedMemoryBuffer = SharedMemory::create(size);
    memcpy(sharedMemoryBuffer->data(), buffer->data(), size);
    sharedMemoryBuffer->createHandle(handle, SharedMemory::ReadOnly);
}

WKAccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    return m_mockAccessibilityElement.get();
}
         
bool WebPage::platformHasLocalDataForURL(const WebCore::URL& url)
{
    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
    [request setValue:(NSString*)userAgent(url) forHTTPHeaderField:@"User-Agent"];
    NSCachedURLResponse *cachedResponse;
    if (CFURLStorageSessionRef storageSession = corePage()->mainFrame().loader().networkingContext()->storageSession().platformSession())
        cachedResponse = WKCachedResponseForRequest(storageSession, request);
    else
        cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    
    return cachedResponse;
}

static NSCachedURLResponse *cachedResponseForURL(WebPage* webPage, const URL& url)
{
    RetainPtr<NSMutableURLRequest> request = adoptNS([[NSMutableURLRequest alloc] initWithURL:url]);
    [request setValue:(NSString *)webPage->userAgent(url) forHTTPHeaderField:@"User-Agent"];

    if (CFURLStorageSessionRef storageSession = webPage->corePage()->mainFrame().loader().networkingContext()->storageSession().platformSession())
        return WKCachedResponseForRequest(storageSession, request.get());

    return [[NSURLCache sharedURLCache] cachedResponseForRequest:request.get()];
}

String WebPage::cachedSuggestedFilenameForURL(const URL& url)
{
    return [[cachedResponseForURL(this, url) response] suggestedFilename];
}

String WebPage::cachedResponseMIMETypeForURL(const URL& url)
{
    return [[cachedResponseForURL(this, url) response] MIMEType];
}

PassRefPtr<SharedBuffer> WebPage::cachedResponseDataForURL(const URL& url)
{
    return SharedBuffer::wrapNSData([cachedResponseForURL(this, url) data]);
}

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest& request)
{
    if ([NSURLConnection canHandleRequest:request.nsURLRequest(DoNotUpdateHTTPBody)])
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

void WebPage::setTopOverhangImage(PassRefPtr<WebImage> image)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    GraphicsLayer* layer = frameView->setWantsLayerForTopOverHangArea(image.get());
    if (!layer)
        return;

    layer->setSize(image->size());
    layer->setPosition(FloatPoint(0, -image->size().height()));

    RetainPtr<CGImageRef> cgImage = image->bitmap()->makeCGImageCopy();
    layer->platformLayer().contents = (id)cgImage.get();
}

void WebPage::setBottomOverhangImage(PassRefPtr<WebImage> image)
{
    FrameView* frameView = m_mainFrame->coreFrame()->view();
    if (!frameView)
        return;

    GraphicsLayer* layer = frameView->setWantsLayerForBottomOverHangArea(image.get());
    if (!layer)
        return;

    layer->setSize(image->size());
    
    RetainPtr<CGImageRef> cgImage = image->bitmap()->makeCGImageCopy();
    layer->platformLayer().contents = (id)cgImage.get();
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
    WebFrame* frame = WebProcess::shared().webFrame(frameID);
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
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:context flipped:NO]];
    [pdfPage drawWithBox:kPDFDisplayBoxCropBox];
    [NSGraphicsContext restoreGraphicsState];

    CGAffineTransform transform = CGContextGetCTM(context);

    for (PDFAnnotation *annotation in [pdfPage annotations]) {
        if (![annotation isKindOfClass:pdfAnnotationLinkClass()])
            continue;

        PDFAnnotationLink *linkAnnotation = (PDFAnnotationLink *)annotation;
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
WebCore::WebGLLoadPolicy WebPage::webGLPolicyForURL(WebFrame* frame, const String& url)
{
    uint32_t policyResult = 0;

    if (sendSync(Messages::WebPageProxy::WebGLPolicyForURL(url), Messages::WebPageProxy::WebGLPolicyForURL::Reply(policyResult)))
        return static_cast<WebGLLoadPolicy>(policyResult);

    return WebGLAllowCreation;
}

WebCore::WebGLLoadPolicy WebPage::resolveWebGLPolicyForURL(WebFrame* frame, const String& url)
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

    NSData *selectionData = [attributedSelection RTFDFromRange:NSMakeRange(0, [attributedSelection length]) documentAttributes:nil];
    IPC::DataReference data = IPC::DataReference(reinterpret_cast<const uint8_t*>([selectionData bytes]), [selectionData length]);
    bool isEditable = selection.selection().isContentEditable();

    send(Messages::WebPageProxy::ShowSelectionServiceMenu(data, phoneNumbers, isEditable, point));
}
#endif

String WebPage::platformUserAgent(const URL&) const
{
    return String();
}

static RetainPtr<DDActionContext> scanForDataDetectedItems(const HitTestResult& hitTestResult, FloatRect& detectedDataBoundingBox, RefPtr<Range>& detectedDataRange)
{
    Node* node = hitTestResult.innerNonSharedNode();
    if (!node)
        return nullptr;
    auto renderer = node->renderer();
    if (!renderer)
        return nullptr;
    VisiblePosition position = renderer->positionForPoint(hitTestResult.localPoint(), nullptr);
    if (position.isNull())
        position = firstPositionInOrBeforeNode(node);

    RefPtr<Range> contextRange = rangeExpandedAroundPositionByCharacters(position, 250);
    if (!contextRange)
        return nullptr;

    String fullPlainTextString = plainText(contextRange.get());
    int hitLocation = TextIterator::rangeLength(makeRange(contextRange->startPosition(), position).get());

    RetainPtr<DDScannerRef> scanner = adoptCF(DDScannerCreate(DDScannerTypeStandard, 0, nullptr));
    RetainPtr<DDScanQueryRef> scanQuery = adoptCF(DDScanQueryCreateFromString(kCFAllocatorDefault, fullPlainTextString.createCFString().get(), CFRangeMake(0, fullPlainTextString.length())));

    if (!DDScannerScanQuery(scanner.get(), scanQuery.get()))
        return nullptr;

    RetainPtr<CFArrayRef> results = adoptCF(DDScannerCopyResultsWithOptions(scanner.get(), DDScannerCopyResultsOptionsNoOverlap));

    // Find the DDResultRef that intersects the hitTestResult's VisiblePosition.
    DDResultRef mainResult = nullptr;
    RefPtr<Range> mainResultRange;
    CFIndex resultCount = CFArrayGetCount(results.get());
    for (CFIndex i = 0; i < resultCount; i++) {
        DDResultRef result = (DDResultRef)CFArrayGetValueAtIndex(results.get(), i);
        CFRange resultRangeInContext = DDResultGetRange(result);
        if (hitLocation >= resultRangeInContext.location && (hitLocation - resultRangeInContext.location) < resultRangeInContext.length) {
            mainResult = result;
            mainResultRange = TextIterator::subrange(contextRange.get(), resultRangeInContext.location, resultRangeInContext.length);
            break;
        }
    }

    if (!mainResult)
        return nullptr;

    RetainPtr<DDActionContext> actionContext = adoptNS([[getDDActionContextClass() alloc] init]);
    [actionContext setAllResults:@[ (id)mainResult ]];
    [actionContext setMainResult:mainResult];

    Vector<FloatQuad> quads;
    mainResultRange->textQuads(quads);
    if (!quads.isEmpty())
        detectedDataBoundingBox = mainResultRange->ownerDocument().view()->contentsToWindow(quads[0].enclosingBoundingBox());

    detectedDataRange = mainResultRange;

    return actionContext;
}

static PassRefPtr<TextIndicator> textIndicatorForRange(Range* range)
{
    if (!range)
        return nullptr;

    Frame* frame = range->startContainer()->document().frame();

    if (!frame)
        return nullptr;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(range);

    RefPtr<TextIndicator> indicator = TextIndicator::createWithSelectionInFrame(*WebFrame::fromCoreFrame(*frame));

    frame->selection().setSelection(oldSelection);

    return indicator.release();
}

void WebPage::performActionMenuHitTestAtLocation(WebCore::FloatPoint locationInViewCooordinates)
{
    layoutIfNeeded();

    MainFrame& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView()) {
        send(Messages::WebPageProxy::DidPerformActionMenuHitTest(ActionMenuHitTestResult(), InjectedBundleUserMessageEncoder(nullptr)));
        return;
    }

    RenderView& mainRenderView = *mainFrame.view()->renderView();

    HitTestRequest request(HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnoreClipping);

    IntPoint locationInContentCoordinates = mainFrame.view()->rootViewToContents(roundedIntPoint(locationInViewCooordinates));
    HitTestResult hitTestResult(locationInContentCoordinates);
    mainRenderView.hitTest(request, hitTestResult);

    ActionMenuHitTestResult actionMenuResult;
    actionMenuResult.hitTestLocationInViewCooordinates = locationInViewCooordinates;
    actionMenuResult.hitTestResult = WebHitTestResult::Data(hitTestResult);

    RefPtr<WebCore::Range> lookupRange = lookupTextAtLocation(locationInViewCooordinates);
    actionMenuResult.lookupText = lookupRange ? lookupRange->text() : String();
    m_lastActionMenuRangeForSelection = lookupRange;

    if (Image* image = hitTestResult.image()) {
        actionMenuResult.image = ShareableBitmap::createShareable(IntSize(image->size()), ShareableBitmap::SupportsAlpha);
        if (actionMenuResult.image)
            actionMenuResult.image->createGraphicsContext()->drawImage(image, ColorSpaceDeviceRGB, IntPoint());
    }

    // FIXME: Avoid scanning if we will just throw away the result (e.g. we're over a link).
    if (hitTestResult.innerNode() && hitTestResult.innerNode()->isTextNode()) {
        FloatRect detectedDataBoundingBox;
        RefPtr<Range> detectedDataRange;
        actionMenuResult.actionContext = scanForDataDetectedItems(hitTestResult, detectedDataBoundingBox, detectedDataRange);
        if (actionMenuResult.actionContext) {
            actionMenuResult.detectedDataBoundingBox = detectedDataBoundingBox;
            actionMenuResult.detectedDataTextIndicator = textIndicatorForRange(detectedDataRange.get());
            m_lastActionMenuRangeForSelection = detectedDataRange;
        }
    }

    RefPtr<API::Object> userData;

    bool handled = false;
    for (const auto& overlay : mainFrame.pageOverlayController().pageOverlays()) {
        WebPageOverlay* webOverlay = WebPageOverlay::fromCoreOverlay(*overlay);
        if (!webOverlay)
            continue;
        if (webOverlay->prepareForActionMenu(userData)) {
            handled = true;
            break;
        }
    }

    if (!handled) {
        RefPtr<InjectedBundleHitTestResult> injectedBundleHitTestResult = InjectedBundleHitTestResult::create(hitTestResult);
        injectedBundleContextMenuClient().prepareForActionMenu(this, injectedBundleHitTestResult.get(), userData);
    }

    send(Messages::WebPageProxy::DidPerformActionMenuHitTest(actionMenuResult, InjectedBundleUserMessageEncoder(userData.get())));
}

PassRefPtr<WebCore::Range> WebPage::lookupTextAtLocation(FloatPoint locationInViewCooordinates)
{
    MainFrame& mainFrame = corePage()->mainFrame();
    if (!mainFrame.view() || !mainFrame.view()->renderView())
        return nullptr;

    IntPoint point = roundedIntPoint(locationInViewCooordinates);
    HitTestResult result = mainFrame.eventHandler().hitTestResultAtPoint(m_page->mainFrame().view()->windowToContents(point), HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::AllowChildFrameContent | HitTestRequest::IgnoreClipping);
    NSDictionary *options = nil;
    return rangeForDictionaryLookupAtHitTestResult(result, &options);
}

void WebPage::selectLastActionMenuRange()
{
    if (m_lastActionMenuRangeForSelection)
        corePage()->mainFrame().selection().setSelectedRange(m_lastActionMenuRangeForSelection.get(), DOWNSTREAM, true);
}

} // namespace WebKit

#endif // PLATFORM(MAC)
