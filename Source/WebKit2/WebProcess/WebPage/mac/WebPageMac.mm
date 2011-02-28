/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "AccessibilityWebPageObject.h"
#import "DataReference.h"
#import "PluginView.h"
#import "WebCoreArgumentCoders.h"
#import "WebEvent.h"
#import "WebFrame.h"
#import "WebPageProxyMessages.h"
#import "WebProcess.h"
#import <WebCore/AXObjectCache.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/Page.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/ScrollView.h>
#import <WebCore/TextIterator.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <WebKitSystemInterface.h>

using namespace WebCore;

namespace WebKit {

void WebPage::platformInitialize()
{
    m_page->addSchedulePair(SchedulePair::create([NSRunLoop currentRunLoop], kCFRunLoopCommonModes));

#if !defined(BUILDING_ON_SNOW_LEOPARD)
    AccessibilityWebPageObject* mockAccessibilityElement = [[[AccessibilityWebPageObject alloc] init] autorelease];

    // Get the pid for the starting process.
    pid_t pid = WebProcess::shared().presenterApplicationPid();    
    WKAXInitializeElementWithPresenterPid(mockAccessibilityElement, pid);
    [mockAccessibilityElement setWebPage:this];
    
    // send data back over
    NSData* remoteToken = (NSData *)WKAXRemoteTokenForElement(mockAccessibilityElement); 
    CoreIPC::DataReference dataToken = CoreIPC::DataReference(reinterpret_cast<const uint8_t*>([remoteToken bytes]), [remoteToken length]);
    send(Messages::WebPageProxy::RegisterWebProcessAccessibilityToken(dataToken));
    m_mockAccessibilityElement = mockAccessibilityElement;
#endif
}

void WebPage::platformPreferencesDidChange(const WebPreferencesStore&)
{
}

// FIXME: need to add support for input methods
    
bool WebPage::interceptEditingKeyboardEvent(KeyboardEvent* evt, bool shouldSaveCommand)
{
    Node* node = evt->target()->toNode();
    ASSERT(node);
    Frame* frame = node->document()->frame();
    ASSERT(frame);
    
    const PlatformKeyboardEvent* keyEvent = evt->keyEvent();
    if (!keyEvent)
        return false;
    const Vector<KeypressCommand>& commands = evt->keypressCommands();
    bool hasKeypressCommand = !commands.isEmpty();
    
    bool eventWasHandled = false;
    
    if (shouldSaveCommand || !hasKeypressCommand) {
        Vector<KeypressCommand> commandsList;  
        Vector<CompositionUnderline> underlines;
        unsigned start;
        unsigned end;
        if (!WebProcess::shared().connection()->sendSync(Messages::WebPageProxy::InterpretKeyEvent(keyEvent->type()), 
                                                         Messages::WebPageProxy::InterpretKeyEvent::Reply(commandsList, start, end, underlines),
                                                         m_pageID, CoreIPC::Connection::NoTimeout))
            return false;
        if (commandsList.isEmpty())
            return eventWasHandled;
        
        if (commandsList[0].commandName == "setMarkedText") {
            frame->editor()->setComposition(commandsList[0].text, underlines, start, end);
            eventWasHandled = true;
        } else if (commandsList[0].commandName == "insertText" && frame->editor()->hasComposition()) {
            frame->editor()->confirmComposition(commandsList[0].text);
            eventWasHandled = true;
        } else if (commandsList[0].commandName == "unmarkText") {
            frame->editor()->confirmComposition();
            eventWasHandled = true;
        } else {
            for (size_t i = 0; i < commandsList.size(); i++)
                evt->keypressCommands().append(commandsList[i]);
        }
    } else {
        size_t size = commands.size();
        // Are there commands that would just cause text insertion if executed via Editor?
        // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
        // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
        // (e.g. Tab that inserts a Tab character, or Enter).
        bool haveTextInsertionCommands = false;
        for (size_t i = 0; i < size; ++i) {
            if (frame->editor()->command(commands[i].commandName).isTextInsertion())
                haveTextInsertionCommands = true;
        }
        if (!haveTextInsertionCommands || keyEvent->type() == PlatformKeyboardEvent::Char) {
            for (size_t i = 0; i < size; ++i) {
                if (commands[i].commandName == "insertText") {
                    // Don't insert null or control characters as they can result in unexpected behaviour
                    if (evt->charCode() < ' ')
                        return false;
                    eventWasHandled = frame->editor()->insertText(commands[i].text, evt);
                } else
                    if (frame->editor()->command(commands[i].commandName).isSupported())
                        eventWasHandled = frame->editor()->command(commands[i].commandName).execute(evt);
            }
        }
    }
    return eventWasHandled;
}

void WebPage::sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput)
{
    for (HashSet<PluginView*>::const_iterator it = m_pluginViews.begin(), end = m_pluginViews.end(); it != end; ++it) {
        if ((*it)->sendComplexTextInput(pluginComplexTextInputIdentifier, textInput))
            break;
    }
}

void WebPage::getMarkedRange(uint64_t& location, uint64_t& length)
{
    location = NSNotFound;
    length = 0;
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    if (!frame)
        return;
    
    getLocationAndLengthFromRange(frame->editor()->compositionRange().get(), location, length);
}

static PassRefPtr<Range> characterRangeAtPoint(Frame* frame, const IntPoint& point)
{
    VisiblePosition position = frame->visiblePositionForPoint(point);
    if (position.isNull())
        return 0;
    
    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        RefPtr<Range> previousCharacterRange = makeRange(previous, position);
        IntRect rect = frame->editor()->firstRectForRange(previousCharacterRange.get());
        if (rect.contains(point))
            return previousCharacterRange.release();
    }
    
    VisiblePosition next = position.next();
    if (next.isNotNull()) {
        RefPtr<Range> nextCharacterRange = makeRange(position, next);
        IntRect rect = frame->editor()->firstRectForRange(nextCharacterRange.get());
        if (rect.contains(point))
            return nextCharacterRange.release();
    }
    
    return 0;
}
    
void WebPage::characterIndexForPoint(IntPoint point, uint64_t& index)
{
    index = NSNotFound;
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return;

    HitTestResult result = frame->eventHandler()->hitTestResultAtPoint(point, false);
    frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document()->frame() : m_page->focusController()->focusedOrMainFrame();
    
    RefPtr<Range> range = characterRangeAtPoint(frame, result.point());
    if (!range)
        return;

    uint64_t length;
    getLocationAndLengthFromRange(range.get(), index, length);
}

static PassRefPtr<Range> convertToRange(Frame* frame, NSRange nsrange)
{
    if (nsrange.location > INT_MAX)
        return 0;
    if (nsrange.length > INT_MAX || nsrange.location + nsrange.length > INT_MAX)
        nsrange.length = INT_MAX - nsrange.location;
        
    // our critical assumption is that we are only called by input methods that
    // concentrate on a given area containing the selection
    // We have to do this because of text fields and textareas. The DOM for those is not
    // directly in the document DOM, so serialization is problematic. Our solution is
    // to use the root editable element of the selection start as the positional base.
    // That fits with AppKit's idea of an input context.
    Element* selectionRoot = frame->selection()->rootEditableElement();
    Element* scope = selectionRoot ? selectionRoot : frame->document()->documentElement();
    return TextIterator::rangeFromLocationAndLength(scope, nsrange.location, nsrange.length);
}
    
void WebPage::firstRectForCharacterRange(uint64_t location, uint64_t length, WebCore::IntRect& resultRect)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    resultRect.setLocation(IntPoint(0, 0));
    resultRect.setSize(IntSize(0, 0));
    
    RefPtr<Range> range = convertToRange(frame, NSMakeRange(location, length));
    if (!range)
        return;
    
    ASSERT(range->startContainer());
    ASSERT(range->endContainer());
     
    IntRect rect = frame->editor()->firstRectForRange(range.get());
    resultRect = frame->view()->contentsToWindow(rect);
}

void WebPage::performDictionaryLookupAtLocation(const FloatPoint& floatPoint)
{
    Frame* frame = m_page->mainFrame();
    if (!frame)
        return;

    // Find the frame the point is over.
    IntPoint point = roundedIntPoint(floatPoint);

    HitTestResult result = frame->eventHandler()->hitTestResultAtPoint(point, false);
    frame = result.innerNonSharedNode() ? result.innerNonSharedNode()->document()->frame() : m_page->focusController()->focusedOrMainFrame();

    // Figure out if there are any characters under the point.
    RefPtr<Range> characterRange = characterRangeAtPoint(frame, frame->view()->windowToContents(point));
    if (!characterRange)
        return;

    // Grab the currently selected text.
    RefPtr<Range> selectedRange = m_page->focusController()->focusedOrMainFrame()->selection()->selection().toNormalizedRange();

    // Use the selected text if the point was anywhere in it. Assertain this by seeing if either character range
    // the mouse is over is contained by the selection range.
    if (characterRange && selectedRange) {
        ExceptionCode ec = 0;
        selectedRange->compareBoundaryPoints(Range::START_TO_START, characterRange.get(), ec);
        if (!ec) {
            if (selectedRange->isPointInRange(characterRange->startContainer(), characterRange->startOffset(), ec)) {
                if (!ec)
                    characterRange = selectedRange;
            }
        }
    }

    if (!characterRange)
        return;

    // Ensure we have whole words.
    VisibleSelection selection(characterRange.get());
    selection.expandUsingGranularity(WordGranularity);

    RefPtr<Range> finalRange = selection.toNormalizedRange();
    if (!finalRange)
        return;

    performDictionaryLookupForRange(DictionaryPopupInfo::HotKey, frame, finalRange.get());
}

void WebPage::performDictionaryLookupForRange(DictionaryPopupInfo::Type type, Frame* frame, Range* range)
{
    String rangeText = range->text();
    if (rangeText.stripWhiteSpace().isEmpty())
        return;
    
    RenderObject* renderer = range->startContainer()->renderer();
    RenderStyle* style = renderer->style();
    NSFont *font = style->font().primaryFont()->getNSFont();
    if (!font)
        return;
    
    CFDictionaryRef fontDescriptorAttributes = (CFDictionaryRef)[[font fontDescriptor] fontAttributes];
    if (!fontDescriptorAttributes)
        return;

    Vector<FloatQuad> quads;
    range->textQuads(quads);
    if (quads.isEmpty())
        return;
    
    IntRect rangeRect = frame->view()->contentsToWindow(quads[0].enclosingBoundingBox());
    
    DictionaryPopupInfo dictionaryPopupInfo;
    dictionaryPopupInfo.type = type;
    dictionaryPopupInfo.origin = FloatPoint(rangeRect.x(), rangeRect.y());
    dictionaryPopupInfo.fontInfo.fontAttributeDictionary = fontDescriptorAttributes;

    send(Messages::WebPageProxy::DidPerformDictionaryLookup(rangeText, dictionaryPopupInfo));
}

static inline void scroll(Page* page, ScrollDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->scrollRecursively(direction, granularity);
}

static inline void logicalScroll(Page* page, ScrollLogicalDirection direction, ScrollGranularity granularity)
{
    page->focusController()->focusedOrMainFrame()->eventHandler()->logicalScrollRecursively(direction, granularity);
}

bool WebPage::performDefaultBehaviorForKeyEvent(const WebKeyboardEvent& keyboardEvent)
{
    if (keyboardEvent.type() != WebEvent::KeyDown)
        return false;

    // FIXME: This should be in WebCore.

    switch (keyboardEvent.windowsVirtualKeyCode()) {
    case VK_BACK:
        if (keyboardEvent.shiftKey())
            m_page->goForward();
        else
            m_page->goBack();
        break;
    case VK_SPACE:
        if (keyboardEvent.shiftKey())
            logicalScroll(m_page.get(), ScrollBlockDirectionBackward, ScrollByPage);
        else
            logicalScroll(m_page.get(), ScrollBlockDirectionForward, ScrollByPage);
        break;
    case VK_PRIOR:
        logicalScroll(m_page.get(), ScrollBlockDirectionBackward, ScrollByPage);
        break;
    case VK_NEXT:
        logicalScroll(m_page.get(), ScrollBlockDirectionForward, ScrollByPage);
        break;
    case VK_HOME:
        logicalScroll(m_page.get(), ScrollBlockDirectionBackward, ScrollByDocument);
        break;
    case VK_END:
        logicalScroll(m_page.get(), ScrollBlockDirectionForward, ScrollByDocument);
        break;
    case VK_UP:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollUp, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollUp, ScrollByPage);
        else
            scroll(m_page.get(), ScrollUp, ScrollByLine);
        break;
    case VK_DOWN:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey()) {
            scroll(m_page.get(), ScrollDown, ScrollByDocument);
            scroll(m_page.get(), ScrollLeft, ScrollByDocument);
        } else if (keyboardEvent.altKey() || keyboardEvent.controlKey())
            scroll(m_page.get(), ScrollDown, ScrollByPage);
        else
            scroll(m_page.get(), ScrollDown, ScrollByLine);
        break;
    case VK_LEFT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goBack();
        else {
            if (keyboardEvent.altKey()  | keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollLeft, ScrollByPage);
            else
                scroll(m_page.get(), ScrollLeft, ScrollByLine);
        }
        break;
    case VK_RIGHT:
        if (keyboardEvent.shiftKey())
            return false;
        if (keyboardEvent.metaKey())
            m_page->goForward();
        else {
            if (keyboardEvent.altKey() || keyboardEvent.controlKey())
                scroll(m_page.get(), ScrollRight, ScrollByPage);
            else
                scroll(m_page.get(), ScrollRight, ScrollByLine);
        }
        break;
    default:
        return false;
    }

    return true;
}

void WebPage::registerUIProcessAccessibilityTokens(const CoreIPC::DataReference& elementToken, const CoreIPC::DataReference& windowToken)
{
#if !defined(BUILDING_ON_SNOW_LEOPARD)
    NSData* elementTokenData = [NSData dataWithBytes:elementToken.data() length:elementToken.size()];
    NSData* windowTokenData = [NSData dataWithBytes:windowToken.data() length:windowToken.size()];
    id remoteElement = WKAXRemoteElementForToken(elementTokenData);
    id remoteWindow = WKAXRemoteElementForToken(windowTokenData);
    WKAXSetWindowForRemoteElement(remoteWindow, remoteElement);
    
    [accessibilityRemoteObject() setRemoteParent:remoteElement];
#endif
}

void WebPage::writeSelectionToPasteboard(const String& pasteboardName, const Vector<String>& pasteboardTypes, bool& result)
{
    Frame* frame = m_page->focusController()->focusedOrMainFrame();
    frame->editor()->writeSelectionToPasteboard(pasteboardName, pasteboardTypes);
    result = true;
}

AccessibilityWebPageObject* WebPage::accessibilityRemoteObject()
{
    return m_mockAccessibilityElement.get();
}
         
bool WebPage::platformHasLocalDataForURL(const WebCore::KURL& url)
{
    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
    [request setValue:(NSString*)userAgent() forHTTPHeaderField:@"User-Agent"];
    NSCachedURLResponse *cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    
    return cachedResponse;
}

String WebPage::cachedResponseMIMETypeForURL(const WebCore::KURL& url)
{
    NSMutableURLRequest* request = [[NSMutableURLRequest alloc] initWithURL:url];
    [request setValue:(NSString*)userAgent() forHTTPHeaderField:@"User-Agent"];
    NSCachedURLResponse *cachedResponse = [[NSURLCache sharedURLCache] cachedResponseForRequest:request];
    [request release];
    
    return [[cachedResponse response] MIMEType];
}

bool WebPage::canHandleRequest(const WebCore::ResourceRequest& request)
{
    if ([NSURLConnection canHandleRequest:request.nsURLRequest()])
        return YES;

    // FIXME: Return true if this scheme is any one WebKit2 knows how to handle.
    return request.url().protocolIs("applewebdata");
}

} // namespace WebKit
