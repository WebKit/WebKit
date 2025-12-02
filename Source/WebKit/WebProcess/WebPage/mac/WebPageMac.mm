/*
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#import "EditingRange.h"
#import "EditorState.h"
#import "FrameInfoData.h"
#import "InjectedBundleHitTestResult.h"
#import "InjectedBundlePageContextMenuClient.h"
#import "LaunchServicesDatabaseManager.h"
#import "Logging.h"
#import "PDFPluginBase.h"
#import "PageBanner.h"
#import "PlatformFontInfo.h"
#import "PluginView.h"
#import "PrintInfo.h"
#import "UserData.h"
#import "WKAccessibilityWebPageObjectMac.h"
#import "WebEventConversion.h"
#import "WebFrame.h"
#import "WebHitTestResultData.h"
#import "WebImage.h"
#import "WebInspectorBackend.h"
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
#import <WebCore/BoundaryPointInlines.h>
#import <WebCore/ColorMac.h>
#import <WebCore/DataDetection.h>
#import <WebCore/DictionaryLookup.h>
#import <WebCore/DocumentPage.h>
#import <WebCore/DocumentQuirks.h>
#import <WebCore/DocumentView.h>
#import <WebCore/Editing.h>
#import <WebCore/EditingHTMLConverter.h>
#import <WebCore/Editor.h>
#import <WebCore/EventHandler.h>
#import <WebCore/FocusController.h>
#import <WebCore/FrameDestructionObserverInlines.h>
#import <WebCore/FrameLoader.h>
#import <WebCore/FrameLoaderTypes.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/GraphicsLayer.h>
#import <WebCore/HTMLAttachmentElement.h>
#import <WebCore/HTMLImageElement.h>
#import <WebCore/HTMLPlugInElement.h>
#import <WebCore/HitTestResult.h>
#import <WebCore/ImageOverlay.h>
#import <WebCore/ImmediateActionStage.h>
#import <WebCore/KeyboardEvent.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/NetworkStorageSession.h>
#import <WebCore/NodeHTMLConverter.h>
#import <WebCore/NodeRenderStyle.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PlatformKeyboardEvent.h>
#import <WebCore/PluginDocument.h>
#import <WebCore/PointerCharacteristics.h>
#import <WebCore/Quirks.h>
#import <WebCore/RemoteFrameView.h>
#import <WebCore/RemoteUserInputEventData.h>
#import <WebCore/RenderElement.h>
#import <WebCore/RenderObject.h>
#import <WebCore/RenderStyle.h>
#import <WebCore/RenderTheme.h>
#import <WebCore/RenderView.h>
#import <WebCore/ScrollView.h>
#import <WebCore/StyleInheritedData.h>
#import <WebCore/TextIterator.h>
#import <WebCore/VisibleUnits.h>
#import <WebCore/WindowsKeyboardCodes.h>
#import <WebCore/markup.h>
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <pal/spi/cocoa/NSAccessibilitySPI.h>
#import <pal/spi/mac/NSApplicationSPI.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/SetForScope.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/cocoa/VectorCocoa.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#import "MediaPlaybackTargetContextSerialized.h"
#import "MediaPlaybackTargetSerialized.h"
#endif

#import "PDFKitSoftLink.h"

namespace WebKit {
using namespace WebCore;

void WebPage::platformInitializeAccessibility(ShouldInitializeNSAccessibility shouldInitializeNSAccessibility)
{
    RELEASE_LOG(Process, "WebPage::platformInitializeAccessibility shouldInitializeNSAccessibility = %d", shouldInitializeNSAccessibility == ShouldInitializeNSAccessibility::Yes);

    // For performance reasons, we should have received the LS database before initializing NSApplication.
    ASSERT(LaunchServicesDatabaseManager::singleton().hasReceivedLaunchServicesDatabase());

    // Need to initialize accessibility for VoiceOver to work when the WebContent process is using NSRunLoop.
    // Currently, it is also needed to allocate and initialize an NSApplication object.
    if (shouldInitializeNSAccessibility == ShouldInitializeNSAccessibility::Yes)
        [NSApplication _accessibilityInitialize];

    // Get the pid for the starting process.
    pid_t pid = legacyPresentingApplicationPID();
    createMockAccessibilityElement(pid);
    if (protectedCorePage()->localMainFrame())
        accessibilityTransferRemoteToken(accessibilityRemoteTokenData());

    // Close Mach connection to Launch Services.
    _LSSetApplicationLaunchServicesServerConnectionStatus(kLSServerConnectionStatusDoNotConnectToServerMask | kLSServerConnectionStatusReleaseNotificationsMask, nullptr);

    WebProcess::singleton().revokeLaunchServicesSandboxExtension();
}

void WebPage::createMockAccessibilityElement(pid_t pid)
{
    auto mockAccessibilityElement = adoptNS([[WKAccessibilityWebPageObject alloc] init]);

    if ([mockAccessibilityElement respondsToSelector:@selector(accessibilitySetPresenterProcessIdentifier:)])
        [(id)mockAccessibilityElement.get() accessibilitySetPresenterProcessIdentifier:pid];
    [mockAccessibilityElement setWebPage:this];
    m_mockAccessibilityElement = WTFMove(mockAccessibilityElement);
}

void WebPage::platformReinitializeAccessibilityToken()
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;
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

void WebPage::getPlatformEditorState(LocalFrame& frame, EditorState& result) const
{
    getPlatformEditorStateCommon(frame, result);

    result.canEnableAutomaticSpellingCorrection = result.isContentEditable && frame.protectedEditor()->canEnableAutomaticSpellingCorrection();
    RefPtr document = frame.document();
    result.inputMethodUsesCorrectKeyEventOrder = frame.settings().inputMethodUsesCorrectKeyEventOrder() || (document && document->quirks().inputMethodUsesCorrectKeyEventOrder());

    if (!result.hasPostLayoutAndVisualData())
        return;

    auto& selection = frame.selection().selection();
    auto selectedRange = selection.toNormalizedRange();
    if (!selectedRange)
        return;

    auto& postLayoutData = *result.postLayoutData;
    VisiblePosition selectionStart = selection.visibleStart();
    auto selectionStartBoundary = makeBoundaryPoint(selectionStart);
    auto selectionEnd = makeBoundaryPoint(selection.visibleEnd());
    auto paragraphStart = makeBoundaryPoint(startOfParagraph(selectionStart));

    if (!selectionStartBoundary || !selectionEnd || !paragraphStart)
        return;

    auto contextRangeForCandidateRequest = frame.protectedEditor()->contextRangeForCandidateRequest();

    postLayoutData.candidateRequestStartPosition = characterCount({ *paragraphStart, *selectionStartBoundary });
    postLayoutData.selectedTextLength = characterCount({ *selectionStartBoundary, *selectionEnd });
    postLayoutData.paragraphContextForCandidateRequest = contextRangeForCandidateRequest ? plainText(*contextRangeForCandidateRequest) : String();
    postLayoutData.stringForCandidateRequest = frame.protectedEditor()->stringForCandidateRequest();

    auto quads = RenderObject::absoluteTextQuads(*selectedRange);
    if (!quads.isEmpty())
        postLayoutData.selectionBoundingRect = frame.protectedView()->contentsToWindow(enclosingIntRect(unitedBoundingBoxes(quads)));
    else if (selection.isCaret()) {
        // Quads will be empty at the start of a paragraph.
        postLayoutData.selectionBoundingRect = frame.protectedView()->contentsToWindow(frame.checkedSelection()->absoluteCaretBounds());
    }
}

void WebPage::handleAcceptedCandidate(WebCore::TextCheckingResult acceptedCandidate)
{
    if (RefPtr frame = m_page->focusController().focusedLocalFrame())
        frame->protectedEditor()->handleAcceptedCandidate(acceptedCandidate);
}

static String commandNameForSelectorName(const String& selectorName)
{
    // Map selectors into Editor command names.
    // This is not needed for any selectors that have the same name as the Editor command.
    static constexpr std::pair<ComparableASCIILiteral, ASCIILiteral> selectorExceptions[] = {
        { "insertNewlineIgnoringFieldEditor:"_s, "InsertNewline"_s },
        { "insertParagraphSeparator:"_s, "InsertNewline"_s },
        { "insertTabIgnoringFieldEditor:"_s, "InsertTab"_s },
        { "pageDown:"_s, "MovePageDown"_s },
        { "pageDownAndModifySelection:"_s, "MovePageDownAndModifySelection"_s },
        { "pageUp:"_s, "MovePageUp"_s },
        { "pageUpAndModifySelection:"_s, "MovePageUpAndModifySelection"_s },
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

static LocalFrame* frameForEvent(KeyboardEvent* event)
{
    ASSERT(event->target());
    ASSERT(downcast<Node>(event->target())->document().frame());
    return downcast<Node>(event->target())->document().frame();
}

bool WebPage::executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>& commands, KeyboardEvent* event)
{
    RefPtr frame = event ? frameForEvent(event) : m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return false;
    ASSERT(frame->page() == corePage());

    Ref editor = frame->editor();
    bool eventWasHandled = false;
    for (size_t i = 0; i < commands.size(); ++i) {
        auto& currentCommand = commands[i];
        if (currentCommand.commandName == "insertText:"_s) {
            if (editor->hasComposition()) {
                eventWasHandled = true;
                editor->confirmComposition(currentCommand.text);
            } else {
                if (!editor->canEdit())
                    continue;

                // An insertText: might be handled by other responders in the chain if we don't handle it.
                // One example is space bar that results in scrolling down the page.
                eventWasHandled |= editor->insertText(currentCommand.text, event);
            }
        } else if (currentCommand.commandName == "setMarkedText:"_s) {
            setCompositionAsync(currentCommand.text, currentCommand.underlines, currentCommand.highlights, { },
                EditingRange { currentCommand.selectedRange }, EditingRange { currentCommand.replacementRange });
        } else {
            if (currentCommand.commandName == "scrollPageDown:"_s || currentCommand.commandName == "scrollPageUp:"_s)
                frame->eventHandler().setProcessingKeyRepeatForPotentialScroll(event && event->repeat());

            Editor::Command command = editor->command(commandNameForSelectorName(currentCommand.commandName));
            if (command.isSupported()) {
                bool commandExecutedByEditor = command.execute(event);
                eventWasHandled |= commandExecutedByEditor;
                if (!commandExecutedByEditor) {
                    bool performedNonEditingBehavior = event->underlyingPlatformEvent()->type() == PlatformEvent::Type::RawKeyDown && performNonEditingBehaviorForSelector(commands[i].commandName, event);
                    eventWasHandled |= performedNonEditingBehavior;
                }
            } else {
                auto sendResult = WebProcess::singleton().protectedParentProcessConnection()->sendSync(Messages::WebPageProxy::ExecuteSavedCommandBySelector(commands[i].commandName), m_identifier);
                auto [commandWasHandledByUIProcess] = sendResult.takeReplyOr(false);
                eventWasHandled |= commandWasHandledByUIProcess;
            }
        }
    }
    return eventWasHandled;
}

bool WebPage::handleEditingKeyboardEvent(KeyboardEvent& event)
{
    RefPtr frame = frameForEvent(&event);
    
    auto* platformEvent = event.underlyingPlatformEvent();
    if (!platformEvent)
        return false;
    auto& commands = event.keypressCommands();

    ASSERT(!platformEvent->macEvent()); // Cannot have a native event in WebProcess.

    // Don't handle Esc while handling keydown event, we need to dispatch a keypress first.
    if (platformEvent->type() != PlatformEvent::Type::Char && platformEvent->windowsVirtualKeyCode() == VK_ESCAPE && commands.size() == 1 && commandNameForSelectorName(commands[0].commandName) == "cancelOperation"_s)
        return false;

    if (handleKeyEventByRelinquishingFocusToChrome(event))
        return true;

    updateLastNodeBeforeWritingSuggestions(event);

    bool eventWasHandled = false;

    // Are there commands that could just cause text insertion if executed via Editor?
    // WebKit doesn't have enough information about mode to decide how they should be treated, so we leave it upon WebCore
    // to either handle them immediately (e.g. Tab that changes focus) or let a keypress event be generated
    // (e.g. Tab that inserts a Tab character, or Enter).
    bool haveTextInsertionCommands = false;
    Ref editor = frame->editor();
    for (auto& command : commands) {
        if (editor->command(commandNameForSelectorName(command.commandName)).isTextInsertion())
            haveTextInsertionCommands = true;
    }
    // If there are no text insertion commands, default keydown handler is the right time to execute the commands.
    // Keypress (Char event) handler is the latest opportunity to execute.
    if (!haveTextInsertionCommands || platformEvent->type() == PlatformEvent::Type::Char) {
        eventWasHandled = executeKeypressCommandsInternal(commands, &event);
        commands.clear();
    }

    return eventWasHandled;
}

void WebPage::attributedSubstringForCharacterRangeAsync(const EditingRange& editingRange, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&& completionHandler)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ }, { });

    const VisibleSelection& selection = frame->selection().selection();
    if (selection.isNone() || !selection.isContentEditable() || selection.isInPasswordField()) {
        completionHandler({ }, { });
        return;
    }

    auto range = EditingRange::toRange(*frame, editingRange);
    if (!range) {
        completionHandler({ }, { });
        return;
    }

    auto attributedString = editingAttributedString(*range, { }).nsAttributedString();

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
        completionHandler(WebCore::AttributedString::fromNSAttributedString(WTFMove(attributedString)), EditingRange());
        return;
    }

    completionHandler(WebCore::AttributedString::fromNSAttributedString(WTFMove(attributedString)), rangeToSend);
}

bool WebPage::performNonEditingBehaviorForSelector(const String& selector, KeyboardEvent* event)
{
    // First give accessibility a chance to handle the event.
    RefPtr frame = frameForEvent(event);
    frame->eventHandler().handleKeyboardSelectionMovementForAccessibility(*event);
    if (event->defaultHandled())
        return true;

    // FIXME: All these selectors have corresponding Editor commands, but the commands only work in editable content.
    // Should such non-editing behaviors be implemented in Editor or EventHandler::defaultArrowEventHandler() perhaps?
    
    bool didPerformAction = false;
    
    RefPtr page = m_page;
    if (!frame->eventHandler().shouldUseSmoothKeyboardScrollingForFocusedScrollableArea()) {
        if (selector == "moveUp:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollUp, ScrollGranularity::Line);
        else if (selector == "moveToBeginningOfParagraph:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollUp, ScrollGranularity::Page);
        else if (selector == "moveToBeginningOfDocument:"_s) {
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollUp, ScrollGranularity::Document);
            didPerformAction |= scroll(page.get(), ScrollDirection::ScrollLeft, ScrollGranularity::Document);
        } else if (selector == "moveDown:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollDown, ScrollGranularity::Line);
        else if (selector == "moveToEndOfParagraph:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollDown, ScrollGranularity::Page);
        else if (selector == "moveToEndOfDocument:"_s) {
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollDown, ScrollGranularity::Document);
            didPerformAction |= scroll(page.get(), ScrollDirection::ScrollLeft, ScrollGranularity::Document);
        } else if (selector == "moveLeft:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollLeft, ScrollGranularity::Line);
        else if (selector == "moveWordLeft:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollLeft, ScrollGranularity::Page);
        else if (selector == "moveRight:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollRight, ScrollGranularity::Line);
        else if (selector == "moveWordRight:"_s)
            didPerformAction = scroll(page.get(), ScrollDirection::ScrollRight, ScrollGranularity::Page);
    }

    if (selector == "moveToLeftEndOfLine:"_s)
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? page->checkedBackForward()->goBack() : page->checkedBackForward()->goForward();
    else if (selector == "moveToRightEndOfLine:"_s)
        didPerformAction = m_userInterfaceLayoutDirection == WebCore::UserInterfaceLayoutDirection::LTR ? page->checkedBackForward()->goForward() : page->checkedBackForward()->goBack();

    return didPerformAction;
}

void WebPage::updateRemotePageAccessibilityOffset(WebCore::FrameIdentifier frameID, WebCore::IntPoint offset)
{
    [protectedAccessibilityRemoteObject() setRemoteFrameOffset:offset];
}

void WebPage::registerRemoteFrameAccessibilityTokens(pid_t pid, std::span<const uint8_t> elementToken, WebCore::FrameIdentifier frameID)
{
    RetainPtr elementTokenData = toNSData(elementToken);
    auto remoteElement = [elementTokenData length] ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:elementTokenData.get()]) : nil;

    createMockAccessibilityElement(pid);
    RetainPtr accessibilityRemoteObject = this->accessibilityRemoteObject();
    [accessibilityRemoteObject setRemoteParent:remoteElement.get() token:elementTokenData.get()];
    [accessibilityRemoteObject setFrameIdentifier:frameID];
}

void WebPage::registerUIProcessAccessibilityTokens(std::span<const uint8_t> elementToken, std::span<const uint8_t> windowToken)
{
    RetainPtr elementTokenData = toNSData(elementToken);
    RetainPtr windowTokenData = toNSData(windowToken);
    RetainPtr remoteElement = [elementTokenData length] ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:elementTokenData.get()]) : nil;
    RetainPtr remoteWindow = [windowTokenData length] ? adoptNS([[NSAccessibilityRemoteUIElement alloc] initWithRemoteToken:windowTokenData.get()]) : nil;

    [remoteElement setWindowUIElement:remoteWindow.get()];
    [remoteElement setTopLevelUIElement:remoteWindow.get()];
    RetainPtr accessibilityRemoteObject = this->accessibilityRemoteObject();
    [accessibilityRemoteObject setWindow:remoteWindow.get()];
    [accessibilityRemoteObject setRemoteParent:remoteElement.get() token:elementTokenData.get()];
}

void WebPage::getStringSelectionForPasteboard(CompletionHandler<void(String&&)>&& completionHandler)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = focusedPluginViewForFrame(*frame)) {
        String selection = pluginView->selectionString();
        if (!selection.isNull())
            return completionHandler(WTFMove(selection));
    }
#endif

    if (frame->selection().isNone())
        return completionHandler({ });

    completionHandler(frame->protectedEditor()->stringSelectionForPasteboard());
}

void WebPage::getDataSelectionForPasteboard(const String pasteboardType, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });
    if (frame->selection().isNone())
        return completionHandler({ });

    auto buffer = frame->protectedEditor()->dataSelectionForPasteboard(pasteboardType);
    if (!buffer)
        return completionHandler({ });
    completionHandler(buffer.releaseNonNull());
}

WebCore::IntPoint WebPage::accessibilityRemoteFrameOffset()
{
    return [m_mockAccessibilityElement accessibilityRemoteFrameOffset];
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void WebPage::cacheAXPosition(const WebCore::FloatPoint& point)
{
    [m_mockAccessibilityElement setPosition:point];
}

void WebPage::cacheAXSize(const WebCore::IntSize& size)
{
    [m_mockAccessibilityElement setSize:size];
}

void WebPage::setIsolatedTree(Ref<WebCore::AXIsolatedTree>&& tree)
{
    [m_mockAccessibilityElement setIsolatedTree:WTFMove(tree)];
}
#endif

bool WebPage::platformCanHandleRequest(const WebCore::ResourceRequest& request)
{
    RetainPtr nsRequest = request.nsURLRequest(HTTPBodyUpdatePolicy::DoNotUpdateHTTPBody);
    if (!nsRequest.get().URL)
        return false;
    if ([NSURLConnection canHandleRequest:nsRequest.get()])
        return true;

    // FIXME: Return true if this scheme is any one WebKit2 knows how to handle.
#if ENABLE(SWIFT_DEMO_URI_SCHEME)
    return request.url().protocolIs("applewebdata"_s)
        || request.url().protocolIs("x-swift-demo"_s);
#else
    return request.url().protocolIs("applewebdata"_s);
#endif
}

void WebPage::shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent& event, CompletionHandler<void(bool)>&& completionHandler)
{
    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return completionHandler({ });

    bool result = false;
#if ENABLE(DRAG_SUPPORT)
    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitResult = frame->eventHandler().hitTestResultAtPoint(frame->protectedView()->windowToContents(flooredIntPoint(event.position())), hitType);
    if (hitResult.isSelected())
        result = frame->eventHandler().eventMayStartDrag(platform(event));
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

    RefPtr frame = m_page->focusController().focusedOrMainFrame();
    if (!frame)
        return;

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    HitTestResult hitResult = frame->eventHandler().hitTestResultAtPoint(frame->protectedView()->windowToContents(flooredIntPoint(event.position())), hitType);
    frame->eventHandler().setActivationEventNumber(eventNumber);
    bool result = false;
#if ENABLE(DRAG_SUPPORT)
    if (hitResult.isSelected())
        result = frame->eventHandler().eventMayStartDrag(platform(event));
    else
#endif
        result = !!hitResult.scrollbar();

    send(Messages::WebPageProxy::HandleAcceptsFirstMouse(result));
}

void WebPage::setTopOverhangImage(WebImage* image)
{
    RefPtr frameView = m_mainFrame->coreLocalFrame()->view();
    if (!frameView)
        return;

    RefPtr layer = frameView->setWantsLayerForTopOverhangImage(image);
    if (!layer)
        return;

    auto nativeImage = image->copyNativeImage();
    if (!nativeImage)
        return;

    layer->setSize(image->size());
    layer->setPosition(FloatPoint(0, -image->size().height()));
    layer->protectedPlatformLayer().get().contents = (__bridge id)nativeImage->platformImage().get();
}

void WebPage::setBottomOverhangImage(WebImage* image)
{
    RefPtr frameView = m_mainFrame->coreLocalFrame()->view();
    if (!frameView)
        return;

    RefPtr layer = frameView->setWantsLayerForBottomOverHangArea(image);
    if (!layer)
        return;

    auto nativeImage = image->copyNativeImage();
    if (!nativeImage)
        return;

    layer->setSize(image->size());
    layer->protectedPlatformLayer().get().contents = (__bridge id)nativeImage->platformImage().get();
}

void WebPage::setUseFormSemanticContext(bool useFormSemanticContext)
{
    RenderTheme::singleton().setUseFormSemanticContext(useFormSemanticContext);
}

void WebPage::semanticContextDidChange(bool useFormSemanticContext)
{
    setUseFormSemanticContext(useFormSemanticContext);
    protectedCorePage()->scheduleRenderingUpdate({ });
}

void WebPage::updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor)
{    
    if (RefPtr banner = m_headerBanner)
        banner->didChangeDeviceScaleFactor(scaleFactor);
    if (RefPtr banner = m_footerBanner)
        banner->didChangeDeviceScaleFactor(scaleFactor);
}

#if ENABLE(TELEPHONE_NUMBER_DETECTION)
void WebPage::handleTelephoneNumberClick(const String& number, const IntPoint& point, const IntRect& rect)
{
    send(Messages::WebPageProxy::ShowTelephoneNumberMenu(number, point, rect));
}
#endif

#if ENABLE(SERVICE_CONTROLS)

void WebPage::handleSelectionServiceClick(WebCore::FrameIdentifier frameID, FrameSelection& selection, const Vector<String>& phoneNumbers, const IntPoint& point)
{
    auto range = selection.selection().firstRange();
    if (!range)
        return;

    auto selectionString = attributedString(*range, IgnoreUserSelectNone::Yes);
    if (selectionString.isNull())
        return;

    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return;

    flushPendingEditorStateUpdate();
    send(Messages::WebPageProxy::ShowContextMenuFromFrame(webFrame->info(), ContextMenuContextData(point, WTFMove(selectionString), phoneNumbers, selection.selection().isContentEditable()), UserData()));
}

void WebPage::handleImageServiceClick(WebCore::FrameIdentifier frameID, const IntPoint& point, Image& image, HTMLImageElement& element)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return;

    send(Messages::WebPageProxy::ShowContextMenuFromFrame(webFrame->info(), ContextMenuContextData {
        point,
        image,
        element.isContentEditable(),
        element.checkedRenderBox()->absoluteContentQuad().enclosingBoundingBox(),
        HTMLAttachmentElement::getAttachmentIdentifier(element),
        contextForElement(element),
        image.mimeType()
    }, { }));
}

void WebPage::handlePDFServiceClick(WebCore::FrameIdentifier frameID, const IntPoint& point, HTMLAttachmentElement& element)
{
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame)
        return;

    send(Messages::WebPageProxy::ShowContextMenuFromFrame(webFrame->info(), ContextMenuContextData {
        point,
        element.isContentEditable(),
        element.checkedRenderBox()->absoluteContentQuad().enclosingBoundingBox(),
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

void WebPage::performImmediateActionHitTestAtLocation(WebCore::FrameIdentifier frameID, WebCore::FloatPoint locationInViewCoordinates)
{
    layoutIfNeeded();

    RefPtr currentFrame = WebProcess::singleton().webFrame(frameID);
    if (!currentFrame)
        return;
    RefPtr localCurrentFrame = currentFrame->coreLocalFrame();
    if (!localCurrentFrame)
        return;
    RefPtr currentFrameView = localCurrentFrame->view();

    if (!currentFrameView || !currentFrameView->renderView()) {
        send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(WebHitTestResultData(), false, UserData()));
        return;
    }

    auto locationInContentCoordinates = localCurrentFrame->protectedView()->rootViewToContents(roundedIntPoint(locationInViewCoordinates));
    auto hitTestResult = localCurrentFrame->eventHandler().hitTestResultAtPoint(locationInContentCoordinates, {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::DisallowUserAgentShadowContentExceptForImageOverlays,
        HitTestRequest::Type::AllowChildFrameContent,
    });

    bool immediateActionHitTestPreventsDefault = false;

    RefPtr element = hitTestResult.targetElement();

    localCurrentFrame->eventHandler().setImmediateActionStage(ImmediateActionStage::PerformedHitTest);
    if (element)
        immediateActionHitTestPreventsDefault = element->dispatchMouseForceWillBegin();

    WebHitTestResultData immediateActionResult(hitTestResult, { });

    auto subframe = EventHandler::subframeForTargetNode(hitTestResult.protectedTargetNode().get());
    if (RefPtr remoteFrame = dynamicDowncast<RemoteFrame>(subframe).get()) {
        if (RefPtr remoteFrameView = remoteFrame->view()) {
            immediateActionResult.remoteUserInputEventData = RemoteUserInputEventData {
                remoteFrame->frameID(),
                remoteFrameView->rootViewToContents(roundedIntPoint(locationInViewCoordinates))
            };
        }
    }

    RefPtr focusedOrMainFrame = corePage()->focusController().focusedOrMainFrame();
    if (!focusedOrMainFrame)
        return;
    auto selectionRange = focusedOrMainFrame->selection().selection().firstRange();

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

    if (auto lookupResult = lookupTextAtLocation(frameID, locationInViewCoordinates)) {
        auto lookupRange = WTFMove(*lookupResult);
        immediateActionResult.lookupText = plainText(lookupRange);
        if (RefPtr node = hitTestResult.innerNode()) {
            if (RefPtr frame = node->document().frame())
                immediateActionResult.dictionaryPopupInfo = dictionaryPopupInfoForRange(*frame, lookupRange, TextIndicatorPresentationTransition::FadeIn);
        }
    }

    bool pageOverlayDidOverrideDataDetectors = false;
    for (auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        RefPtr webOverlay = WebPageOverlay::fromCoreOverlay(*overlay);
        if (!webOverlay)
            continue;

        auto actionContext = webOverlay->actionContextForResultAtPoint(locationInContentCoordinates);
        if (!actionContext)
            continue;

        RefPtr view = actionContext->range.start.document().view();
        if (!view)
            continue;

        pageOverlayDidOverrideDataDetectors = true;
        if (RetainPtr detectedContext = actionContext->context.get())
            immediateActionResult.platformData.detectedDataActionContext = { { detectedContext.get() } };
        immediateActionResult.platformData.detectedDataBoundingBox = view->contentsToWindow(enclosingIntRect(unitedBoundingBoxes(RenderObject::absoluteTextQuads(actionContext->range))));
        immediateActionResult.platformData.detectedDataTextIndicator = TextIndicator::createWithRange(actionContext->range, indicatorOptions(actionContext->range), TextIndicatorPresentationTransition::FadeIn);
        immediateActionResult.platformData.detectedDataOriginatingPageOverlay = overlay->pageOverlayID();
        break;
    }

    // FIXME: Avoid scanning if we will just throw away the result (e.g. we're over a link).
    if (!pageOverlayDidOverrideDataDetectors && (is<Text>(hitTestResult.innerNode()) || hitTestResult.isOverTextInsideFormControlElement())) {
        if (auto result = DataDetection::detectItemAroundHitTestResult(hitTestResult)) {
            if (auto detectedContext = WTFMove(result->actionContext))
                immediateActionResult.platformData.detectedDataActionContext = { { WTFMove(detectedContext) } };
            immediateActionResult.platformData.detectedDataBoundingBox = result->boundingBox;
            immediateActionResult.platformData.detectedDataTextIndicator = TextIndicator::createWithRange(result->range, indicatorOptions(result->range), TextIndicatorPresentationTransition::FadeIn);
        }
    }

#if ENABLE(PDF_PLUGIN)
    if (RefPtr embedOrObject = dynamicDowncast<HTMLPlugInElement>(element)) {
        if (RefPtr pluginView = downcast<PluginView>(embedOrObject->pluginWidget())) {
            if (pluginView->performImmediateActionHitTestAtLocation(locationInViewCoordinates, immediateActionResult)) {
                // FIXME (144030): Focus does not seem to get set to the PDF when invoking the menu.
                RefPtr pluginDocument = dynamicDowncast<PluginDocument>(element->document());
                auto shouldFocusPluginDocument = [&pluginDocument, &immediateActionResult] {
                    if (!pluginDocument)
                        return false;
                    return !immediateActionResult.isActivePDFAnnotation;
                }();

                if (shouldFocusPluginDocument)
                    pluginDocument->setFocusedElement(element.get());
            }
        }
    }
#endif

    RefPtr<API::Object> userData;
    injectedBundleContextMenuClient().prepareForImmediateAction(*this, hitTestResult, userData);

    immediateActionResult.elementBoundingBox = immediateActionResult.elementBoundingBox.toRectWithExtentsClippedToNumericLimits();
    send(Messages::WebPageProxy::DidPerformImmediateActionHitTest(immediateActionResult, immediateActionHitTestPreventsDefault, UserData(WebProcess::singleton().transformObjectsToHandles(userData.get()).get())));
}

std::optional<WebCore::SimpleRange> WebPage::lookupTextAtLocation(FrameIdentifier frameID, FloatPoint locationInViewCoordinates)
{
    RefPtr currentFrame = WebProcess::singleton().webFrame(frameID);
    RefPtr localCurrentFrame = dynamicDowncast<LocalFrame>(currentFrame->coreFrame());
    if (!localCurrentFrame || !localCurrentFrame->view() || !localCurrentFrame->protectedView()->renderView())
        return std::nullopt;

    return DictionaryLookup::rangeAtHitTestResult(localCurrentFrame->eventHandler().hitTestResultAtPoint(localCurrentFrame->protectedView()->windowToContents(roundedIntPoint(locationInViewCoordinates)), {
        HitTestRequest::Type::ReadOnly,
        HitTestRequest::Type::Active,
        HitTestRequest::Type::DisallowUserAgentShadowContentExceptForImageOverlays,
        HitTestRequest::Type::AllowChildFrameContent,
    }));
}

void WebPage::immediateActionDidUpdate()
{
    if (RefPtr localMainFrame = protectedCorePage()->localMainFrame())
        localMainFrame->eventHandler().setImmediateActionStage(ImmediateActionStage::ActionUpdated);
}

void WebPage::immediateActionDidCancel()
{
    RefPtr localMainFrame = protectedCorePage()->localMainFrame();
    if (!localMainFrame)
        return;
    ImmediateActionStage lastStage = localMainFrame->eventHandler().immediateActionStage();
    if (lastStage == ImmediateActionStage::ActionUpdated)
        localMainFrame->eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCancelledAfterUpdate);
    else
        localMainFrame->eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCancelledWithoutUpdate);
}

void WebPage::immediateActionDidComplete()
{
    if (RefPtr localMainFrame = protectedCorePage()->localMainFrame())
        localMainFrame->eventHandler().setImmediateActionStage(ImmediateActionStage::ActionCompleted);
}

void WebPage::dataDetectorsDidPresentUI(PageOverlay::PageOverlayID overlayID)
{
    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (RefPtr webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidPresentUI();
            return;
        }
    }
}

void WebPage::dataDetectorsDidChangeUI(PageOverlay::PageOverlayID overlayID)
{
    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (RefPtr webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidChangeUI();
            return;
        }
    }
}

void WebPage::dataDetectorsDidHideUI(PageOverlay::PageOverlayID overlayID)
{
    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage()->mainFrame());
    if (!localMainFrame)
        return;
    // Dispatching a fake mouse event will allow clients to display any UI that is normally displayed on hover.
    localMainFrame->eventHandler().dispatchFakeMouseMoveEventSoon();

    for (const auto& overlay : corePage()->pageOverlayController().pageOverlays()) {
        if (overlay->pageOverlayID() == overlayID) {
            if (RefPtr webOverlay = WebPageOverlay::fromCoreOverlay(*overlay))
                webOverlay->dataDetectorsDidHideUI();
            return;
        }
    }
}

void WebPage::updateVisibleContentRects(const VisibleContentRectUpdateInfo&, MonotonicTime)
{
}

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
void WebPage::playbackTargetSelected(PlaybackTargetClientContextIdentifier contextId, MediaPlaybackTargetContextSerialized&& targetContext) const
{
    protectedCorePage()->setPlaybackTarget(contextId, MediaPlaybackTargetSerialized::create(WTFMove(targetContext)));
}

void WebPage::playbackTargetAvailabilityDidChange(PlaybackTargetClientContextIdentifier contextId, bool changed)
{
    protectedCorePage()->playbackTargetAvailabilityDidChange(contextId, changed);
}

void WebPage::setShouldPlayToPlaybackTarget(PlaybackTargetClientContextIdentifier contextId, bool shouldPlay)
{
    protectedCorePage()->setShouldPlayToPlaybackTarget(contextId, shouldPlay);
}

void WebPage::playbackTargetPickerWasDismissed(PlaybackTargetClientContextIdentifier contextId)
{
    protectedCorePage()->playbackTargetPickerWasDismissed(contextId);
}
#endif

void WebPage::didBeginMagnificationGesture()
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        pluginView->didBeginMagnificationGesture();
#endif
}

void WebPage::didEndMagnificationGesture()
{
#if ENABLE(MAC_GESTURE_EVENTS)
    if (RefPtr localMainFrame = protectedCorePage()->localMainFrame())
        localMainFrame->eventHandler().didEndMagnificationGesture();
#endif
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        pluginView->didEndMagnificationGesture();
#endif
}

bool WebPage::shouldAvoidComputingPostLayoutDataForEditorState() const
{
    if (m_needsFontAttributes) {
        // Font attribute information is propagated to the UI process through post-layout data on EditorState.
        return false;
    }

    if (!m_requiresUserActionForEditingControlsManager || !m_userInteractionsSincePageTransition.isEmpty()) {
        // Text editing controls on the touch bar depend on having post-layout editor state data.
        return false;
    }

    if (m_hasEverDisplayedContextMenu) {
        // Some context menu items (like Writing Tools) depend on having post-layout editor state data.
        return false;
    }

    return true;
}

#if HAVE(APP_ACCENT_COLORS)

void WebPage::setAccentColor(WebCore::Color color)
{
    [NSApp _setAccentColor:cocoaColorOrNil(color).get()];
}

#if PLATFORM(MAC)
void WebPage::setAppUsesCustomAccentColor(bool appUsesCustomAccentColor)
{
    protectedCorePage()->setAppUsesCustomAccentColor(appUsesCustomAccentColor);
}
#endif

#endif // HAVE(APP_ACCENT_COLORS)

#if ENABLE(PDF_PLUGIN)

void WebPage::zoomPDFIn(PDFPluginIdentifier identifier)
{
    RefPtr pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return;
    pdfPlugin->zoomIn();
}

void WebPage::zoomPDFOut(PDFPluginIdentifier identifier)
{
    RefPtr pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return;
    pdfPlugin->zoomOut();
}

void WebPage::savePDF(PDFPluginIdentifier identifier, CompletionHandler<void(const String&, const URL&, std::span<const uint8_t>)>&& completionHandler)
{
    RefPtr pdfPlugin = m_pdfPlugInsWithHUD.get(identifier);
    if (!pdfPlugin)
        return completionHandler({ }, { }, { });
    pdfPlugin->save(WTFMove(completionHandler));
}

void WebPage::openPDFWithPreview(PDFPluginIdentifier identifier, CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)>&& completionHandler)
{
    for (Ref pluginView : m_pluginViews) {
        if (pluginView->pdfPluginIdentifier() == identifier)
            return pluginView->openWithPreview(WTFMove(completionHandler));
    }

    completionHandler({ }, { }, { });
}

void WebPage::createPDFHUD(PDFPluginBase& plugin, WebCore::FrameIdentifier frameID, const IntRect& boundingBox)
{
    auto addResult = m_pdfPlugInsWithHUD.add(plugin.identifier(), plugin);
    if (addResult.isNewEntry)
        send(Messages::WebPageProxy::CreatePDFHUD(plugin.identifier(), frameID, boundingBox));
}

void WebPage::updatePDFHUDLocation(PDFPluginBase& plugin, const IntRect& boundingBox)
{
    if (m_pdfPlugInsWithHUD.contains(plugin.identifier()))
        send(Messages::WebPageProxy::UpdatePDFHUDLocation(plugin.identifier(), boundingBox));
}

void WebPage::removePDFHUD(PDFPluginBase& plugin)
{
    if (m_pdfPlugInsWithHUD.remove(plugin.identifier()))
        send(Messages::WebPageProxy::RemovePDFHUD(plugin.identifier()));
}

#endif // ENABLE(PDF_PLUGIN)

} // namespace WebKit

#endif // PLATFORM(MAC)
