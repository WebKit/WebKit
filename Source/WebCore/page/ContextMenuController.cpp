/*
 * Copyright (C) 2006-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Igalia S.L
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ContextMenuController.h"

#if ENABLE(CONTEXT_MENUS)

#include "BackForwardController.h"
#include "BitmapImage.h"
#include "Chrome.h"
#include "ContextMenu.h"
#include "ContextMenuClient.h"
#include "ContextMenuItem.h"
#include "ContextMenuProvider.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "Editor.h"
#include "EditorClient.h"
#include "ElementAncestorIteratorInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "FormState.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameSnapshotting.h"
#include "HTMLCanvasElement.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLImageElement.h"
#include "HTMLTableElement.h"
#include "HandleUserInputEventResult.h"
#include "HitTestResult.h"
#include "ImageBuffer.h"
#include "ImageOverlay.h"
#include "InspectorController.h"
#include "LocalFrame.h"
#include "LocalFrameLoaderClient.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NavigationAction.h"
#include "Node.h"
#include "Page.h"
#include "PlatformEvent.h"
#include "PlatformMouseEvent.h"
#include "RenderImage.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "SVGElementTypeHelpers.h"
#include "SVGSVGElement.h"
#include "Settings.h"
#include "TextIterator.h"
#include "TranslationContextMenuInfo.h"
#include "TypingCommand.h"
#include "UserTypingGestureIndicator.h"
#include "WindowFeatures.h"
#include "markup.h"
#include <wtf/SetForScope.h>
#include <wtf/WallTime.h>
#include <wtf/unicode/CharacterNames.h>

#if ENABLE(PDFJS)
#include "PDFDocument.h"
#endif

#if ENABLE(SERVICE_CONTROLS)
#include "ImageControlsMac.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

using namespace WTF::Unicode;

ContextMenuController::ContextMenuController(Page& page, UniqueRef<ContextMenuClient>&& client)
    : m_page(page)
    , m_client(WTFMove(client))
{
}

ContextMenuController::~ContextMenuController()
{
}

Page& ContextMenuController::page()
{
    return m_page.get();
}

void ContextMenuController::clearContextMenu()
{
    m_contextMenu = nullptr;
    if (RefPtr menuProvider = std::exchange(m_menuProvider, nullptr))
        menuProvider->contextMenuCleared();
}

void ContextMenuController::handleContextMenuEvent(Event& event)
{
    if (m_isHandlingContextMenuEvent)
        return;

    SetForScope isHandlingContextMenuEventForScope(m_isHandlingContextMenuEvent, true);

    constexpr OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::DisallowUserAgentShadowContent, HitTestRequest::Type::AllowChildFrameContent };
    m_contextMenu = maybeCreateContextMenu(event, hitType, ContextMenuContext::Type::ContextMenu);
    if (!m_contextMenu)
        return;

    populate();

    showContextMenu(event);
}

static std::unique_ptr<ContextMenuItem> separatorItem()
{
    return std::unique_ptr<ContextMenuItem>(new ContextMenuItem(ContextMenuItemType::Separator, ContextMenuItemTagNoAction, String()));
}

void ContextMenuController::showContextMenu(Event& event, ContextMenuProvider& provider)
{
    m_menuProvider = &provider;

    auto contextType = provider.contextMenuContextType();

    OptionSet<HitTestRequest::Type> hitType { HitTestRequest::Type::ReadOnly, HitTestRequest::Type::Active, HitTestRequest::Type::AllowChildFrameContent };
    if (contextType == ContextMenuContext::Type::ContextMenu)
        hitType.add(HitTestRequest::Type::DisallowUserAgentShadowContent);

    m_contextMenu = maybeCreateContextMenu(event, WTFMove(hitType), contextType);
    if (!m_contextMenu) {
        clearContextMenu();
        return;
    }

    provider.populateContextMenu(m_contextMenu.get());
    if (m_context.hitTestResult().isSelected()) {
        appendItem(*separatorItem(), m_contextMenu.get());
        populate();
    }
    showContextMenu(event);
}

#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)

static void prepareContextForQRCode(ContextMenuContext& context)
{
    auto& result = context.hitTestResult();

    RefPtr node = result.innerNonSharedNode();
    if (!node || !node->document().settings().contextMenuQRCodeDetectionEnabled())
        return;

    if (result.image() || !result.absoluteLinkURL().isEmpty())
        return;

    RefPtr<Element> element;
    RefPtr nodeElement = dynamicDowncast<Element>(*node);
    for (auto& lineage : lineageOfType<Element>(nodeElement ? *nodeElement : *node->protectedParentElement())) {
        if (is<HTMLTableElement>(lineage) || is<HTMLCanvasElement>(lineage) || is<HTMLImageElement>(lineage) || is<SVGSVGElement>(lineage)) {
            element = &lineage;
            break;
        }
    }

    if (!element || !element->renderer())
        return;

    auto elementRect = element->renderer()->absoluteBoundingBoxRect();

    // Constant chosen to be larger than we think any QR code would be, matching the original Safari implementation.
    constexpr auto maxQRCodeContainerDimension = 800;
    if (elementRect.width() > maxQRCodeContainerDimension || elementRect.height() > maxQRCodeContainerDimension)
        return;

    RefPtr frame = element->document().frame();
    if (!frame)
        return;

    auto nodeSnapshotImageBuffer = snapshotNode(*frame, *element, { { }, ImageBufferPixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    RefPtr nodeSnapshotImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTFMove(nodeSnapshotImageBuffer)));
    context.setPotentialQRCodeNodeSnapshotImage(nodeSnapshotImage.get());

    // FIXME: Node snapshotting does not take transforms into account, making it unreliable for QR code detection.
    // As a fallback, also take a viewport-level snapshot. A node snapshot is still required to capture partially
    // obscured elements. This workaround can be removed once rdar://87204215 is fixed.
    auto viewportSnapshotImageBuffer = snapshotFrameRect(*frame, elementRect, { { }, ImageBufferPixelFormat::BGRA8, DestinationColorSpace::SRGB() });
    RefPtr viewportSnapshotImage = BitmapImage::create(ImageBuffer::sinkIntoNativeImage(WTFMove(viewportSnapshotImageBuffer)));
    context.setPotentialQRCodeViewportSnapshotImage(viewportSnapshotImage.get());
}

#endif

std::unique_ptr<ContextMenu> ContextMenuController::maybeCreateContextMenu(Event& event, OptionSet<HitTestRequest::Type> hitType, ContextMenuContext::Type contextType)
{
    RefPtr mouseEvent = dynamicDowncast<MouseEvent>(event);
    if (!mouseEvent)
        return nullptr;

    RefPtr node = dynamicDowncast<Node>(mouseEvent->target());
    if (!node)
        return nullptr;
    RefPtr frame = node->document().frame();
    if (!frame)
        return nullptr;

    auto result = frame->checkedEventHandler()->hitTestResultAtPoint(mouseEvent->absoluteLocation(), WTFMove(hitType));
    if (!result.innerNonSharedNode())
        return nullptr;

    m_context = { contextType, result, &event };
#if ENABLE(CONTEXT_MENU_QR_CODE_DETECTION)
    prepareContextForQRCode(m_context);
#endif
    
    return makeUnique<ContextMenu>();
}

void ContextMenuController::showContextMenu(Event& event)
{
    if ((!m_menuProvider || m_menuProvider->contextMenuContextType() == ContextMenuContext::Type::ContextMenu) && m_page->settings().developerExtrasEnabled())
        addDebuggingItems();

    event.setDefaultHandled();
}

void ContextMenuController::didDismissContextMenu()
{
    if (RefPtr menuProvider = m_menuProvider)
        menuProvider->didDismissContextMenu();
}

static void openNewWindow(const URL& urlToLoad, LocalFrame& frame, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    RefPtr oldPage = frame.page();
    if (!oldPage)
        return;

    FrameLoadRequest frameLoadRequest { frame.protectedDocument().releaseNonNull(), frame.document()->protectedSecurityOrigin(), ResourceRequest(urlToLoad, frame.loader().outgoingReferrer()), { }, InitiatedByMainFrame::Unknown };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);

    RefPtr newPage = oldPage->chrome().createWindow(frame, { }, { *frame.protectedDocument(), frameLoadRequest.resourceRequest(), frameLoadRequest.initiatedByMainFrame(), frameLoadRequest.isRequestFromClientOrUserInput() });
    if (!newPage)
        return;
    newPage->chrome().show();
    if (RefPtr localMainFrame = dynamicDowncast<LocalFrame>(newPage->mainFrame()))
        localMainFrame->checkedLoader()->loadFrameRequest(WTFMove(frameLoadRequest), event, { });
}

#if PLATFORM(GTK)

static void insertUnicodeCharacter(UChar character, LocalFrame& frame)
{
    String text(span(character));
    if (!frame.checkedEditor()->shouldInsertText(text, frame.selection().selection().toNormalizedRange(), EditorInsertAction::Typed))
        return;

    ASSERT(frame.document());
    TypingCommand::insertText(*frame.protectedDocument(), text, { }, TypingCommand::TextCompositionType::None);
}

#endif

void ContextMenuController::contextMenuItemSelected(ContextMenuAction action, const String& title)
{
    if (action >= ContextMenuItemBaseCustomTag) {
        ASSERT(m_menuProvider);
        m_menuProvider->contextMenuItemSelected(action, title);
        return;
    }

    Ref document = m_context.hitTestResult().innerNonSharedNode()->document();

    RefPtr frame = document->frame();
    if (!frame)
        return;

    RefPtr eventForLoadRequests = [&]() -> Event* {
#if PLATFORM(COCOA)
        if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ContextMenuTriggersLinkActivationNavigationType))
            return nullptr;
#endif
        return m_context.event();
    }();

    switch (action) {
    case ContextMenuItemTagOpenLinkInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteLinkURL(), *frame, eventForLoadRequests.get(), ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks);
        break;
    case ContextMenuItemTagDownloadLinkToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client->downloadURL(m_context.hitTestResult().absoluteLinkURL());
        break;
    case ContextMenuItemTagCopyLinkToClipboard:
        frame->checkedEditor()->copyURL(m_context.hitTestResult().absoluteLinkURL(), m_context.hitTestResult().textContent());
        break;
    case ContextMenuItemTagOpenImageInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteImageURL(), *frame, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    case ContextMenuItemTagDownloadImageToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client->downloadURL(m_context.hitTestResult().absoluteImageURL());
        break;
    case ContextMenuItemTagCopyImageToClipboard:
        // FIXME: The Pasteboard class is not written yet
        // For now, call into the client. This is temporary!
        frame->checkedEditor()->copyImage(m_context.hitTestResult());
        break;
#if PLATFORM(GTK)
    case ContextMenuItemTagCopyImageURLToClipboard:
        frame->checkedEditor()->copyURL(m_context.hitTestResult().absoluteImageURL(), m_context.hitTestResult().textContent());
        break;
#endif
    case ContextMenuItemTagOpenMediaInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteMediaURL(), *frame, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    case ContextMenuItemTagDownloadMediaToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client->downloadURL(m_context.hitTestResult().absoluteMediaURL());
        break;
    case ContextMenuItemTagCopyMediaLinkToClipboard:
        frame->checkedEditor()->copyURL(m_context.hitTestResult().absoluteMediaURL(), m_context.hitTestResult().textContent());
        break;
    case ContextMenuItemTagToggleMediaControls:
        m_context.hitTestResult().toggleMediaControlsDisplay();
        break;
    case ContextMenuItemTagToggleMediaLoop:
        m_context.hitTestResult().toggleMediaLoopPlayback();
        break;
    case ContextMenuItemTagShowMediaStats:
        m_context.hitTestResult().toggleShowMediaStats();
        break;
    case ContextMenuItemTagToggleVideoFullscreen:
        m_context.hitTestResult().toggleMediaFullscreenState();
        break;
    case ContextMenuItemTagEnterVideoFullscreen:
        m_context.hitTestResult().enterFullscreenForVideo();
        break;
    case ContextMenuItemTagMediaPlayPause:
        m_context.hitTestResult().toggleMediaPlayState();
        break;
    case ContextMenuItemTagMediaMute:
        m_context.hitTestResult().toggleMediaMuteState();
        break;
    case ContextMenuItemTagToggleVideoEnhancedFullscreen:
        m_context.hitTestResult().toggleEnhancedFullscreenForVideo();
        break;
    case ContextMenuItemTagToggleVideoViewer:
        m_context.hitTestResult().toggleVideoViewer();
        break;
    case ContextMenuItemTagOpenFrameInNewWindow: {
        RefPtr loader = frame->loader().documentLoader();
        if (!loader->unreachableURL().isEmpty())
            openNewWindow(loader->unreachableURL(), *frame, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        else
            openNewWindow(loader->url(), *frame, nullptr, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    }
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    case ContextMenuItemTagPlayAllAnimations: {
        if (RefPtr page = frame->page())
            page->setImageAnimationEnabled(true);
        break;
    }
    case ContextMenuItemTagPauseAllAnimations: {
        if (RefPtr page = frame->page())
            page->setImageAnimationEnabled(false);
        break;
    }
    case ContextMenuItemTagPlayAnimation:
        m_context.hitTestResult().playAnimation();
        break;
    case ContextMenuItemTagPauseAnimation:
        m_context.hitTestResult().pauseAnimation();
        break;
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    case ContextMenuItemTagCopy:
        frame->checkedEditor()->copy();
        break;
    case ContextMenuItemTagCopyLinkToHighlight:
        if (Page* page = frame->page()) {
            auto url = page->fragmentDirectiveURLForSelectedText();
            if (url.isValid())
                frame->editor().copyURL(url, { });
        }
        break;
    case ContextMenuItemTagGoBack:
        if (RefPtr page = frame->page())
            page->checkedBackForward()->goBackOrForward(-1);
        break;
    case ContextMenuItemTagGoForward:
        if (RefPtr page = frame->page())
            page->checkedBackForward()->goBackOrForward(1);
        break;
    case ContextMenuItemTagStop:
        frame->checkedLoader()->stop();
        break;
    case ContextMenuItemTagReload:
        frame->checkedLoader()->reload();
        break;
    case ContextMenuItemTagCut:
        frame->editor().command("Cut"_s).execute();
        break;
    case ContextMenuItemTagPaste:
        frame->editor().command("Paste"_s).execute();
        break;
#if PLATFORM(GTK)
    case ContextMenuItemTagPasteAsPlainText:
        frame->editor().command("PasteAsPlainText"_s).execute();
        break;
    case ContextMenuItemTagDelete:
        frame->checkedEditor()->performDelete();
        break;
    case ContextMenuItemTagUnicodeInsertLRMMark:
        insertUnicodeCharacter(leftToRightMark, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertRLMMark:
        insertUnicodeCharacter(rightToLeftMark, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertLREMark:
        insertUnicodeCharacter(leftToRightEmbed, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertRLEMark:
        insertUnicodeCharacter(rightToLeftEmbed, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertLROMark:
        insertUnicodeCharacter(leftToRightOverride, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertRLOMark:
        insertUnicodeCharacter(rightToLeftOverride, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertPDFMark:
        insertUnicodeCharacter(popDirectionalFormatting, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertZWSMark:
        insertUnicodeCharacter(zeroWidthSpace, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertZWJMark:
        insertUnicodeCharacter(zeroWidthJoiner, *frame);
        break;
    case ContextMenuItemTagUnicodeInsertZWNJMark:
        insertUnicodeCharacter(zeroWidthNonJoiner, *frame);
        break;
    case ContextMenuItemTagSelectAll:
        frame->editor().command("SelectAll"_s).execute();
        break;
    case ContextMenuItemTagInsertEmoji:
        m_client->insertEmoji(*frame);
        break;
#endif
    case ContextMenuItemTagSpellingGuess: {
        VisibleSelection selection = frame->selection().selection();
        if (frame->checkedEditor()->shouldInsertText(title, selection.toNormalizedRange(), EditorInsertAction::Pasted)) {
            OptionSet<ReplaceSelectionCommand::CommandOption> replaceOptions { ReplaceSelectionCommand::MatchStyle, ReplaceSelectionCommand::PreventNesting };

            if (frame->editor().behavior().shouldAllowSpellingSuggestionsWithoutSelection()) {
                ASSERT(selection.isCaretOrRange());
                VisibleSelection wordSelection(selection.base());
                wordSelection.expandUsingGranularity(TextGranularity::WordGranularity);
                frame->checkedSelection()->setSelection(wordSelection);
            } else {
                ASSERT(frame->editor().selectedText().length());
                replaceOptions.add(ReplaceSelectionCommand::SelectReplacement);
            }

            RefPtr document = frame->document();
            ASSERT(document);
            Ref command = ReplaceSelectionCommand::create(*document, createFragmentFromMarkup(*document, title, emptyString()), replaceOptions);
            command->apply();
            frame->checkedSelection()->revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded);
        }
        break;
    }
    case ContextMenuItemTagIgnoreSpelling:
        frame->checkedEditor()->ignoreSpelling();
        break;
    case ContextMenuItemTagLearnSpelling:
        frame->checkedEditor()->learnSpelling();
        break;
    case ContextMenuItemTagSearchWeb:
        m_client->searchWithGoogle(frame.get());
        break;
    case ContextMenuItemTagLookUpInDictionary:
        // FIXME: Some day we may be able to do this from within WebCore.
        m_client->lookUpInDictionary(frame.get());
        break;
    case ContextMenuItemTagOpenLink:
        if (RefPtr targetFrame = m_context.hitTestResult().targetFrame()) {
            ResourceRequest resourceRequest { m_context.hitTestResult().absoluteLinkURL(), frame->loader().outgoingReferrer() };
            FrameLoadRequest frameLoadRequest { frame->protectedDocument().releaseNonNull(), frame->document()->securityOrigin(), WTFMove(resourceRequest), { }, InitiatedByMainFrame::Unknown };
            frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);
            if (targetFrame->isMainFrame())
                frameLoadRequest.setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy::ShouldAllow);
            targetFrame->loader().loadFrameRequest(WTFMove(frameLoadRequest), eventForLoadRequests.get(), { });
        } else
            openNewWindow(m_context.hitTestResult().absoluteLinkURL(), *frame, eventForLoadRequests.get(), ShouldOpenExternalURLsPolicy::ShouldAllow);
        break;
    case ContextMenuItemTagBold:
        frame->editor().command("ToggleBold"_s).execute();
        break;
    case ContextMenuItemTagItalic:
        frame->editor().command("ToggleItalic"_s).execute();
        break;
    case ContextMenuItemTagUnderline:
        frame->checkedEditor()->toggleUnderline();
        break;
    case ContextMenuItemTagOutline:
        // We actually never enable this because CSS does not have a way to specify an outline font,
        // which may make this difficult to implement. Maybe a special case of text-shadow?
        break;
    case ContextMenuItemTagStartSpeaking: {
        auto selectedRange = frame->selection().selection().toNormalizedRange();
        if (!selectedRange || selectedRange->collapsed())
            selectedRange = makeRangeSelectingNodeContents(document);
        m_client->speak(plainText(*selectedRange));
        break;
    }
    case ContextMenuItemTagStopSpeaking:
        m_client->stopSpeaking();
        break;
    case ContextMenuItemTagDefaultDirection:
        frame->checkedEditor()->setBaseWritingDirection(WritingDirection::Natural);
        break;
    case ContextMenuItemTagLeftToRight:
        frame->checkedEditor()->setBaseWritingDirection(WritingDirection::LeftToRight);
        break;
    case ContextMenuItemTagRightToLeft:
        frame->checkedEditor()->setBaseWritingDirection(WritingDirection::RightToLeft);
        break;
    case ContextMenuItemTagTextDirectionDefault:
        frame->editor().command("MakeTextWritingDirectionNatural"_s).execute();
        break;
    case ContextMenuItemTagTextDirectionLeftToRight:
        frame->editor().command("MakeTextWritingDirectionLeftToRight"_s).execute();
        break;
    case ContextMenuItemTagTextDirectionRightToLeft:
        frame->editor().command("MakeTextWritingDirectionRightToLeft"_s).execute();
        break;
    case ContextMenuItemTagShowSpellingPanel:
        frame->checkedEditor()->showSpellingGuessPanel();
        break;
    case ContextMenuItemTagCheckSpelling:
        frame->checkedEditor()->advanceToNextMisspelling();
        break;
    case ContextMenuItemTagCheckSpellingWhileTyping:
        frame->checkedEditor()->toggleContinuousSpellChecking();
        break;
    case ContextMenuItemTagCheckGrammarWithSpelling:
        frame->checkedEditor()->toggleGrammarChecking();
        break;
#if USE(APPKIT)
    case ContextMenuItemTagMakeUpperCase:
        frame->checkedEditor()->uppercaseWord();
        break;
    case ContextMenuItemTagMakeLowerCase:
        frame->checkedEditor()->lowercaseWord();
        break;
    case ContextMenuItemTagCapitalize:
        frame->checkedEditor()->capitalizeWord();
        break;
#endif
#if PLATFORM(COCOA)
    case ContextMenuItemTagChangeBack:
        frame->checkedEditor()->changeBackToReplacedString(m_context.hitTestResult().replacedString());
        break;
#endif
#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    case ContextMenuItemTagShowSubstitutions:
        frame->checkedEditor()->showSubstitutionsPanel();
        break;
    case ContextMenuItemTagSmartCopyPaste:
        frame->checkedEditor()->toggleSmartInsertDelete();
        break;
    case ContextMenuItemTagSmartQuotes:
        frame->checkedEditor()->toggleAutomaticQuoteSubstitution();
        break;
    case ContextMenuItemTagSmartDashes:
        frame->checkedEditor()->toggleAutomaticDashSubstitution();
        break;
    case ContextMenuItemTagSmartLinks:
        frame->checkedEditor()->toggleAutomaticLinkDetection();
        break;
    case ContextMenuItemTagTextReplacement:
        frame->checkedEditor()->toggleAutomaticTextReplacement();
        break;
    case ContextMenuItemTagCorrectSpellingAutomatically:
        frame->checkedEditor()->toggleAutomaticSpellingCorrection();
        break;
#endif
    case ContextMenuItemTagInspectElement:
        if (RefPtr page = frame->page())
            page->inspectorController().inspect(m_context.hitTestResult().innerNonSharedNode());
        break;
    case ContextMenuItemTagDictationAlternative:
        frame->checkedEditor()->applyDictationAlternative(title);
        break;
#if PLATFORM(MAC)
    case ContextMenuItemTagShowFonts:
    case ContextMenuItemTagStyles:
    case ContextMenuItemTagShowColors:
#endif
    case ContextMenuItemTagCopySubject:
    case ContextMenuItemTagLookUpImage:
        // These should be handled at the client layer.
        ASSERT_NOT_REACHED();
        break;
    case ContextMenuItemTagTranslate:
#if HAVE(TRANSLATION_UI_SERVICES)
        if (RefPtr view = frame->view()) {
            m_client->handleTranslation({
                m_context.hitTestResult().selectedText(),
                view->contentsToRootView(enclosingIntRect(frame->selection().selectionBounds())),
                view->contentsToRootView(m_context.hitTestResult().roundedPointInInnerNodeFrame()),
                m_context.hitTestResult().isContentEditable() ? TranslationContextMenuMode::Editable : TranslationContextMenuMode::NonEditable,
                ImageOverlay::isInsideOverlay(frame->selection().selection()) ? TranslationContextMenuSource::Image : TranslationContextMenuSource::Unspecified,
            });
        }
#endif
        break;

    case ContextMenuItemTagWritingTools:
        // The Writing Tools context menu item action is handled in the client layer.
        RELEASE_ASSERT_NOT_REACHED();
        break;

#if ENABLE(PDFJS)
    case ContextMenuItemPDFAutoSize:
        performPDFJSAction(*frame, "context-menu-auto-size"_s);
        break;
    case ContextMenuItemPDFZoomIn:
        performPDFJSAction(*frame, "context-menu-zoom-in"_s);
        break;
    case ContextMenuItemPDFZoomOut:
        performPDFJSAction(*frame, "context-menu-zoom-out"_s);
        break;
    case ContextMenuItemPDFActualSize:
        performPDFJSAction(*frame, "context-menu-actual-size"_s);
        break;
    case ContextMenuItemPDFSinglePage:
        performPDFJSAction(*frame, "context-menu-single-page"_s);
        break;
    case ContextMenuItemPDFSinglePageContinuous:
        performPDFJSAction(*frame, "context-menu-single-page-continuous"_s);
        break;
    case ContextMenuItemPDFTwoPages:
        performPDFJSAction(*frame, "context-menu-two-pages"_s);
        break;
    case ContextMenuItemPDFTwoPagesContinuous:
        performPDFJSAction(*frame, "context-menu-two-pages-continuous"_s);
        break;
    case ContextMenuItemPDFNextPage:
        performPDFJSAction(*frame, "context-menu-next-page"_s);
        break;
    case ContextMenuItemPDFPreviousPage:
        performPDFJSAction(*frame, "context-menu-previous-page"_s);
        break;
#endif
    default:
        break;
    }
}

void ContextMenuController::appendItem(ContextMenuItem& menuItem, ContextMenu* parentMenu)
{
    checkOrEnableIfNeeded(menuItem);
    if (parentMenu)
        parentMenu->appendItem(menuItem);
}

void ContextMenuController::createAndAppendFontSubMenu(ContextMenuItem& fontMenuItem)
{
    ContextMenu fontMenu;

#if PLATFORM(COCOA)
    ContextMenuItem showFonts(ContextMenuItemType::Action, ContextMenuItemTagShowFonts, contextMenuItemTagShowFonts());
#endif
    ContextMenuItem bold(ContextMenuItemType::CheckableAction, ContextMenuItemTagBold, contextMenuItemTagBold());
    ContextMenuItem italic(ContextMenuItemType::CheckableAction, ContextMenuItemTagItalic, contextMenuItemTagItalic());
    ContextMenuItem underline(ContextMenuItemType::CheckableAction, ContextMenuItemTagUnderline, contextMenuItemTagUnderline());
    ContextMenuItem outline(ContextMenuItemType::Action, ContextMenuItemTagOutline, contextMenuItemTagOutline());
#if PLATFORM(COCOA)
    ContextMenuItem styles(ContextMenuItemType::Action, ContextMenuItemTagStyles, contextMenuItemTagStyles());
    ContextMenuItem showColors(ContextMenuItemType::Action, ContextMenuItemTagShowColors, contextMenuItemTagShowColors());
#endif

#if PLATFORM(COCOA)
    appendItem(showFonts, &fontMenu);
#endif
    appendItem(bold, &fontMenu);
    appendItem(italic, &fontMenu);
    appendItem(underline, &fontMenu);
    appendItem(outline, &fontMenu);
#if PLATFORM(COCOA)
    appendItem(styles, &fontMenu);
    appendItem(*separatorItem(), &fontMenu);
    appendItem(showColors, &fontMenu);
#endif

    fontMenuItem.setSubMenu(&fontMenu);
}


#if !PLATFORM(GTK)

void ContextMenuController::createAndAppendSpellingAndGrammarSubMenu(ContextMenuItem& spellingAndGrammarMenuItem)
{
    ContextMenu spellingAndGrammarMenu;

    ContextMenuItem showSpellingPanel(ContextMenuItemType::Action, ContextMenuItemTagShowSpellingPanel,
        contextMenuItemTagShowSpellingPanel(true));
    ContextMenuItem checkSpelling(ContextMenuItemType::Action, ContextMenuItemTagCheckSpelling,
        contextMenuItemTagCheckSpelling());
    ContextMenuItem checkAsYouType(ContextMenuItemType::CheckableAction, ContextMenuItemTagCheckSpellingWhileTyping,
        contextMenuItemTagCheckSpellingWhileTyping());
    ContextMenuItem grammarWithSpelling(ContextMenuItemType::CheckableAction, ContextMenuItemTagCheckGrammarWithSpelling,
        contextMenuItemTagCheckGrammarWithSpelling());
#if PLATFORM(COCOA)
    ContextMenuItem correctSpelling(ContextMenuItemType::CheckableAction, ContextMenuItemTagCorrectSpellingAutomatically,
        contextMenuItemTagCorrectSpellingAutomatically());
#endif

    appendItem(showSpellingPanel, &spellingAndGrammarMenu);
    appendItem(checkSpelling, &spellingAndGrammarMenu);
#if PLATFORM(COCOA)
    appendItem(*separatorItem(), &spellingAndGrammarMenu);
#endif
    appendItem(checkAsYouType, &spellingAndGrammarMenu);
    appendItem(grammarWithSpelling, &spellingAndGrammarMenu);
#if PLATFORM(COCOA)
    appendItem(correctSpelling, &spellingAndGrammarMenu);
#endif

    spellingAndGrammarMenuItem.setSubMenu(&spellingAndGrammarMenu);
}

#endif // !PLATFORM(GTK)


#if PLATFORM(COCOA)

void ContextMenuController::createAndAppendSpeechSubMenu(ContextMenuItem& speechMenuItem)
{
    ContextMenu speechMenu;

    ContextMenuItem start(ContextMenuItemType::Action, ContextMenuItemTagStartSpeaking, contextMenuItemTagStartSpeaking());
    ContextMenuItem stop(ContextMenuItemType::Action, ContextMenuItemTagStopSpeaking, contextMenuItemTagStopSpeaking());

    appendItem(start, &speechMenu);
    appendItem(stop, &speechMenu);

    speechMenuItem.setSubMenu(&speechMenu);
}

#endif
 
#if PLATFORM(GTK)

void ContextMenuController::createAndAppendUnicodeSubMenu(ContextMenuItem& unicodeMenuItem)
{
    ContextMenu unicodeMenu;

    ContextMenuItem leftToRightMarkMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertLRMMark, contextMenuItemTagUnicodeInsertLRMMark());
    ContextMenuItem rightToLeftMarkMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertRLMMark, contextMenuItemTagUnicodeInsertRLMMark());
    ContextMenuItem leftToRightEmbedMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertLREMark, contextMenuItemTagUnicodeInsertLREMark());
    ContextMenuItem rightToLeftEmbedMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertRLEMark, contextMenuItemTagUnicodeInsertRLEMark());
    ContextMenuItem leftToRightOverrideMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertLROMark, contextMenuItemTagUnicodeInsertLROMark());
    ContextMenuItem rightToLeftOverrideMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertRLOMark, contextMenuItemTagUnicodeInsertRLOMark());
    ContextMenuItem popDirectionalFormattingMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertPDFMark, contextMenuItemTagUnicodeInsertPDFMark());
    ContextMenuItem zeroWidthSpaceMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertZWSMark, contextMenuItemTagUnicodeInsertZWSMark());
    ContextMenuItem zeroWidthJoinerMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertZWJMark, contextMenuItemTagUnicodeInsertZWJMark());
    ContextMenuItem zeroWidthNonJoinerMenuItem(ContextMenuItemType::Action, ContextMenuItemTagUnicodeInsertZWNJMark, contextMenuItemTagUnicodeInsertZWNJMark());

    appendItem(leftToRightMarkMenuItem, &unicodeMenu);
    appendItem(rightToLeftMarkMenuItem, &unicodeMenu);
    appendItem(leftToRightEmbedMenuItem, &unicodeMenu);
    appendItem(rightToLeftEmbedMenuItem, &unicodeMenu);
    appendItem(leftToRightOverrideMenuItem, &unicodeMenu);
    appendItem(rightToLeftOverrideMenuItem, &unicodeMenu);
    appendItem(popDirectionalFormattingMenuItem, &unicodeMenu);
    appendItem(zeroWidthSpaceMenuItem, &unicodeMenu);
    appendItem(zeroWidthJoinerMenuItem, &unicodeMenu);
    appendItem(zeroWidthNonJoinerMenuItem, &unicodeMenu);

    unicodeMenuItem.setSubMenu(&unicodeMenu);
}

#else

void ContextMenuController::createAndAppendWritingDirectionSubMenu(ContextMenuItem& writingDirectionMenuItem)
{
    ContextMenu writingDirectionMenu;

    ContextMenuItem defaultItem(ContextMenuItemType::Action, ContextMenuItemTagDefaultDirection,
        contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(ContextMenuItemType::CheckableAction, ContextMenuItemTagLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(ContextMenuItemType::CheckableAction, ContextMenuItemTagRightToLeft, contextMenuItemTagRightToLeft());

    appendItem(defaultItem, &writingDirectionMenu);
    appendItem(ltr, &writingDirectionMenu);
    appendItem(rtl, &writingDirectionMenu);

    writingDirectionMenuItem.setSubMenu(&writingDirectionMenu);
}

void ContextMenuController::createAndAppendTextDirectionSubMenu(ContextMenuItem& textDirectionMenuItem)
{
    ContextMenu textDirectionMenu;

    ContextMenuItem defaultItem(ContextMenuItemType::Action, ContextMenuItemTagTextDirectionDefault, contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(ContextMenuItemType::CheckableAction, ContextMenuItemTagTextDirectionLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(ContextMenuItemType::CheckableAction, ContextMenuItemTagTextDirectionRightToLeft, contextMenuItemTagRightToLeft());

    appendItem(defaultItem, &textDirectionMenu);
    appendItem(ltr, &textDirectionMenu);
    appendItem(rtl, &textDirectionMenu);

    textDirectionMenuItem.setSubMenu(&textDirectionMenu);
}

#endif

#if PLATFORM(COCOA)

void ContextMenuController::createAndAppendSubstitutionsSubMenu(ContextMenuItem& substitutionsMenuItem)
{
    ContextMenu substitutionsMenu;

    ContextMenuItem showSubstitutions(ContextMenuItemType::Action, ContextMenuItemTagShowSubstitutions, contextMenuItemTagShowSubstitutions(true));
    ContextMenuItem smartCopyPaste(ContextMenuItemType::CheckableAction, ContextMenuItemTagSmartCopyPaste, contextMenuItemTagSmartCopyPaste());
    ContextMenuItem smartQuotes(ContextMenuItemType::CheckableAction, ContextMenuItemTagSmartQuotes, contextMenuItemTagSmartQuotes());
    ContextMenuItem smartDashes(ContextMenuItemType::CheckableAction, ContextMenuItemTagSmartDashes, contextMenuItemTagSmartDashes());
    ContextMenuItem smartLinks(ContextMenuItemType::CheckableAction, ContextMenuItemTagSmartLinks, contextMenuItemTagSmartLinks());
    ContextMenuItem textReplacement(ContextMenuItemType::CheckableAction, ContextMenuItemTagTextReplacement, contextMenuItemTagTextReplacement());

    appendItem(showSubstitutions, &substitutionsMenu);
    appendItem(*separatorItem(), &substitutionsMenu);
    appendItem(smartCopyPaste, &substitutionsMenu);
    appendItem(smartQuotes, &substitutionsMenu);
    appendItem(smartDashes, &substitutionsMenu);
    appendItem(smartLinks, &substitutionsMenu);
    appendItem(textReplacement, &substitutionsMenu);

    substitutionsMenuItem.setSubMenu(&substitutionsMenu);
}

void ContextMenuController::createAndAppendTransformationsSubMenu(ContextMenuItem& transformationsMenuItem)
{
    ContextMenu transformationsMenu;

    ContextMenuItem makeUpperCase(ContextMenuItemType::Action, ContextMenuItemTagMakeUpperCase, contextMenuItemTagMakeUpperCase());
    ContextMenuItem makeLowerCase(ContextMenuItemType::Action, ContextMenuItemTagMakeLowerCase, contextMenuItemTagMakeLowerCase());
    ContextMenuItem capitalize(ContextMenuItemType::Action, ContextMenuItemTagCapitalize, contextMenuItemTagCapitalize());

    appendItem(makeUpperCase, &transformationsMenu);
    appendItem(makeLowerCase, &transformationsMenu);
    appendItem(capitalize, &transformationsMenu);

    transformationsMenuItem.setSubMenu(&transformationsMenu);
}

#endif

#if PLATFORM(COCOA)
#define SUPPORTS_TOGGLE_VIDEO_FULLSCREEN 1
#else
#define SUPPORTS_TOGGLE_VIDEO_FULLSCREEN 0
#endif

#if PLATFORM(COCOA)
#define SUPPORTS_TOGGLE_SHOW_HIDE_MEDIA_CONTROLS 1
#else
#define SUPPORTS_TOGGLE_SHOW_HIDE_MEDIA_CONTROLS 0
#endif

void ContextMenuController::populate()
{
    ContextMenuItem OpenLinkItem(ContextMenuItemType::Action, ContextMenuItemTagOpenLink, contextMenuItemTagOpenLink());
    ContextMenuItem OpenLinkInNewWindowItem(ContextMenuItemType::Action, ContextMenuItemTagOpenLinkInNewWindow,
        contextMenuItemTagOpenLinkInNewWindow());
    ContextMenuItem DownloadFileItem(ContextMenuItemType::Action, ContextMenuItemTagDownloadLinkToDisk,
        contextMenuItemTagDownloadLinkToDisk());
    ContextMenuItem CopyLinkItem(ContextMenuItemType::Action, ContextMenuItemTagCopyLinkToClipboard,
        contextMenuItemTagCopyLinkToClipboard());
    ContextMenuItem OpenImageInNewWindowItem(ContextMenuItemType::Action, ContextMenuItemTagOpenImageInNewWindow,
        contextMenuItemTagOpenImageInNewWindow());
    ContextMenuItem DownloadImageItem(ContextMenuItemType::Action, ContextMenuItemTagDownloadImageToDisk,
        contextMenuItemTagDownloadImageToDisk());
    ContextMenuItem CopyImageItem(ContextMenuItemType::Action, ContextMenuItemTagCopyImageToClipboard,
        contextMenuItemTagCopyImageToClipboard());
#if PLATFORM(GTK)
    ContextMenuItem CopyImageURLItem(ContextMenuItemType::Action, ContextMenuItemTagCopyImageURLToClipboard,
        contextMenuItemTagCopyImageURLToClipboard());
#endif
    ContextMenuItem OpenMediaInNewWindowItem(ContextMenuItemType::Action, ContextMenuItemTagOpenMediaInNewWindow, String());
    ContextMenuItem DownloadMediaItem(ContextMenuItemType::Action, ContextMenuItemTagDownloadMediaToDisk, String());
    ContextMenuItem CopyMediaLinkItem(ContextMenuItemType::Action, ContextMenuItemTagCopyMediaLinkToClipboard, String());
    ContextMenuItem MediaPlayPause(ContextMenuItemType::Action, ContextMenuItemTagMediaPlayPause,
        contextMenuItemTagMediaPlay());
    ContextMenuItem MediaMute(ContextMenuItemType::Action, ContextMenuItemTagMediaMute,
        contextMenuItemTagMediaMute());
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    ContextMenuItem PlayAllAnimations(ContextMenuItemType::Action, ContextMenuItemTagPlayAllAnimations, contextMenuItemTagPlayAllAnimations());
    ContextMenuItem PauseAllAnimations(ContextMenuItemType::Action, ContextMenuItemTagPauseAllAnimations, contextMenuItemTagPauseAllAnimations());
    ContextMenuItem PlayAnimation(ContextMenuItemType::Action, ContextMenuItemTagPlayAnimation, contextMenuItemTagPlayAnimation());
    ContextMenuItem PauseAnimation(ContextMenuItemType::Action, ContextMenuItemTagPauseAnimation, contextMenuItemTagPauseAnimation());
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
#if SUPPORTS_TOGGLE_SHOW_HIDE_MEDIA_CONTROLS
    ContextMenuItem ToggleMediaControls(ContextMenuItemType::Action, ContextMenuItemTagToggleMediaControls,
        contextMenuItemTagHideMediaControls());
#else
    ContextMenuItem ToggleMediaControls(ContextMenuItemType::CheckableAction, ContextMenuItemTagToggleMediaControls,
        contextMenuItemTagToggleMediaControls());
#endif
    ContextMenuItem ToggleMediaLoop(ContextMenuItemType::CheckableAction, ContextMenuItemTagToggleMediaLoop,
        contextMenuItemTagToggleMediaLoop());
    ContextMenuItem EnterVideoFullscreen(ContextMenuItemType::Action, ContextMenuItemTagEnterVideoFullscreen,
        contextMenuItemTagEnterVideoFullscreen());
    ContextMenuItem ToggleVideoFullscreen(ContextMenuItemType::Action, ContextMenuItemTagToggleVideoFullscreen,
        contextMenuItemTagEnterVideoFullscreen());
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    ContextMenuItem ToggleVideoEnhancedFullscreen(ContextMenuItemType::Action, ContextMenuItemTagToggleVideoEnhancedFullscreen, contextMenuItemTagEnterVideoEnhancedFullscreen());
    ContextMenuItem ToggleVideoViewer(ContextMenuItemType::Action, ContextMenuItemTagToggleVideoViewer, contextMenuItemTagEnterVideoViewer());
#endif

#if ENABLE(PDFJS)
    ContextMenuItem PDFAutoSizeItem(ContextMenuItemType::Action, ContextMenuItemPDFAutoSize, contextMenuItemPDFAutoSize());
    ContextMenuItem PDFZoomInItem(ContextMenuItemType::Action, ContextMenuItemPDFZoomIn, contextMenuItemPDFZoomIn());
    ContextMenuItem PDFZoomOutItem(ContextMenuItemType::Action, ContextMenuItemPDFZoomOut, contextMenuItemPDFZoomOut());
    ContextMenuItem PDFActualSizeItem(ContextMenuItemType::Action, ContextMenuItemPDFActualSize, contextMenuItemPDFActualSize());

    ContextMenuItem PDFSinglePageItem(ContextMenuItemType::Action, ContextMenuItemPDFSinglePage, contextMenuItemPDFSinglePage());
    ContextMenuItem PDFSinglePageContinuousItem(ContextMenuItemType::Action, ContextMenuItemPDFSinglePageContinuous, contextMenuItemPDFSinglePageContinuous());
    ContextMenuItem PDFTwoPagesItem(ContextMenuItemType::Action, ContextMenuItemPDFTwoPages, contextMenuItemPDFTwoPages());
    ContextMenuItem PDFTwoPagesContinuousItem(ContextMenuItemType::Action, ContextMenuItemPDFTwoPagesContinuous, contextMenuItemPDFTwoPagesContinuous());

    ContextMenuItem PDFNextPageItem(ContextMenuItemType::Action, ContextMenuItemPDFNextPage, contextMenuItemPDFNextPage());
    ContextMenuItem PDFPreviousPageItem(ContextMenuItemType::Action, ContextMenuItemPDFPreviousPage, contextMenuItemPDFPreviousPage());
#endif
    
#if ENABLE(APP_HIGHLIGHTS)
    ContextMenuItem AddHighlightItem(ContextMenuItemType::Action, ContextMenuItemTagAddHighlightToCurrentQuickNote, contextMenuItemTagAddHighlightToCurrentQuickNote());
    ContextMenuItem AddHighlightToNewQuickNoteItem(ContextMenuItemType::Action, ContextMenuItemTagAddHighlightToNewQuickNote, contextMenuItemTagAddHighlightToNewQuickNote());
#endif
    ContextMenuItem CopyLinkToHighlightItem(ContextMenuItemType::Action, ContextMenuItemTagCopyLinkToHighlight, contextMenuItemTagCopyLinkToHighlight());
#if !PLATFORM(GTK)
    ContextMenuItem SearchWebItem(ContextMenuItemType::Action, ContextMenuItemTagSearchWeb, contextMenuItemTagSearchWeb());
#endif
    ContextMenuItem CopyItem(ContextMenuItemType::Action, ContextMenuItemTagCopy, contextMenuItemTagCopy());
    ContextMenuItem BackItem(ContextMenuItemType::Action, ContextMenuItemTagGoBack, contextMenuItemTagGoBack());
    ContextMenuItem ForwardItem(ContextMenuItemType::Action, ContextMenuItemTagGoForward,  contextMenuItemTagGoForward());
    ContextMenuItem StopItem(ContextMenuItemType::Action, ContextMenuItemTagStop, contextMenuItemTagStop());
    ContextMenuItem ReloadItem(ContextMenuItemType::Action, ContextMenuItemTagReload, contextMenuItemTagReload());
    ContextMenuItem OpenFrameItem(ContextMenuItemType::Action, ContextMenuItemTagOpenFrameInNewWindow,
        contextMenuItemTagOpenFrameInNewWindow());
    ContextMenuItem NoGuessesItem(ContextMenuItemType::Action, ContextMenuItemTagNoGuessesFound,
        contextMenuItemTagNoGuessesFound());
    ContextMenuItem IgnoreSpellingItem(ContextMenuItemType::Action, ContextMenuItemTagIgnoreSpelling,
        contextMenuItemTagIgnoreSpelling());
    ContextMenuItem LearnSpellingItem(ContextMenuItemType::Action, ContextMenuItemTagLearnSpelling,
        contextMenuItemTagLearnSpelling());
    ContextMenuItem IgnoreGrammarItem(ContextMenuItemType::Action, ContextMenuItemTagIgnoreGrammar,
        contextMenuItemTagIgnoreGrammar());
    ContextMenuItem CutItem(ContextMenuItemType::Action, ContextMenuItemTagCut, contextMenuItemTagCut());
    ContextMenuItem PasteItem(ContextMenuItemType::Action, ContextMenuItemTagPaste, contextMenuItemTagPaste());
#if PLATFORM(GTK)
    ContextMenuItem PasteAsPlainTextItem(ContextMenuItemType::Action, ContextMenuItemTagPasteAsPlainText, contextMenuItemTagPasteAsPlainText());
    ContextMenuItem DeleteItem(ContextMenuItemType::Action, ContextMenuItemTagDelete, contextMenuItemTagDelete());
    ContextMenuItem SelectAllItem(ContextMenuItemType::Action, ContextMenuItemTagSelectAll, contextMenuItemTagSelectAll());
    ContextMenuItem InsertEmojiItem(ContextMenuItemType::Action, ContextMenuItemTagInsertEmoji, contextMenuItemTagInsertEmoji());
#endif
#if ENABLE(IMAGE_ANALYSIS)
    ContextMenuItem LookUpImageItem(ContextMenuItemType::Action, ContextMenuItemTagLookUpImage, contextMenuItemTagLookUpImage());
#endif

#if PLATFORM(GTK) || PLATFORM(WIN)
    ContextMenuItem ShareMenuItem;
#else
    ContextMenuItem ShareMenuItem(ContextMenuItemType::Action, ContextMenuItemTagShareMenu, emptyString());
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    ContextMenuItem copySubjectItem { ContextMenuItemType::Action, ContextMenuItemTagCopySubject, contextMenuItemTagCopySubject() };
#endif

    RefPtr node = m_context.hitTestResult().innerNonSharedNode();
    if (!node)
        return;
#if PLATFORM(GTK)
    if (!m_context.hitTestResult().isContentEditable() && is<HTMLFormControlElement>(*node))
        return;
#endif
    RefPtr frame = node->document().frame();
    if (!frame)
        return;

#if ENABLE(SERVICE_CONTROLS)
    // The default image control menu gets populated solely by the platform.
    if (m_context.controlledImage())
        return;
#endif

    auto addSelectedTextActionsIfNeeded = [&] (const String& selectedText) {
        if (selectedText.isEmpty())
            return;

#if PLATFORM(COCOA)
        ContextMenuItem lookUpInDictionaryItem(ContextMenuItemType::Action, ContextMenuItemTagLookUpInDictionary, contextMenuItemTagLookUpInDictionary(selectedText));
        appendItem(lookUpInDictionaryItem, m_contextMenu.get());
#endif

#if HAVE(TRANSLATION_UI_SERVICES)
        ContextMenuItem translateItem(ContextMenuItemType::Action, ContextMenuItemTagTranslate, contextMenuItemTagTranslate(selectedText));
        appendItem(translateItem, m_contextMenu.get());
#endif

#if !PLATFORM(GTK)
        appendItem(SearchWebItem, m_contextMenu.get());
        appendItem(*separatorItem(), m_contextMenu.get());
#endif
    };

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    auto canAddAnimationControls = [&] () -> bool {
        if (!frame->page() || !frame->page()->settings().imageAnimationControlEnabled())
            return false;

        return Image::systemAllowsAnimationControls() || frame->page()->settings().allowAnimationControlsOverride();
    };

    bool shouldAddPlayAllPauseAllAnimationsItem = canAddAnimationControls();
    auto addPlayAllPauseAllAnimationsItem = [&] () {
        if (!shouldAddPlayAllPauseAllAnimationsItem)
            return;
        // Only add this item once.
        shouldAddPlayAllPauseAllAnimationsItem = false;

        if (frame->page()->imageAnimationEnabled())
            appendItem(PauseAllAnimations, m_contextMenu.get());
        else
            appendItem(PlayAllAnimations, m_contextMenu.get());
    };
#endif

    auto selectedText = m_context.hitTestResult().selectedText();
    m_context.setSelectedText(selectedText);

    if (!m_context.hitTestResult().isContentEditable()) {
        CheckedRef loader = frame->loader();
        URL linkURL = m_context.hitTestResult().absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->client().canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem, m_contextMenu.get());
                appendItem(OpenLinkInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadFileItem, m_contextMenu.get());
            }
            appendItem(CopyLinkItem, m_contextMenu.get());
        }

        URL imageURL = m_context.hitTestResult().absoluteImageURL();
        if (!imageURL.isEmpty()) {
            if (!linkURL.isEmpty())
                appendItem(*separatorItem(), m_contextMenu.get());

            appendItem(OpenImageInNewWindowItem, m_contextMenu.get());
            appendItem(DownloadImageItem, m_contextMenu.get());

            RefPtr image = m_context.hitTestResult().image();
            if (imageURL.protocolIsFile() || image) {
                appendItem(CopyImageItem, m_contextMenu.get());

                if (image && !image->isAnimated()) {
#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
                    if (m_client->supportsCopySubject())
                        appendItem(copySubjectItem, m_contextMenu.get());
#endif
#if ENABLE(IMAGE_ANALYSIS)
                    if (m_client->supportsLookUpInImages())
                        appendItem(LookUpImageItem, m_contextMenu.get());
#endif
                }

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
                if (image && image->isAnimated() && canAddAnimationControls()) {
                    appendItem(*separatorItem(), m_contextMenu.get());
                    if (m_context.hitTestResult().isAnimating())
                        appendItem(PauseAnimation, m_contextMenu.get());
                    else
                        appendItem(PlayAnimation, m_contextMenu.get());
                    // If the individual animation control action is available, group the Pause All Animations / Play All Animations action with it.
                    addPlayAllPauseAllAnimationsItem();
                }
#endif // ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
            }
#if PLATFORM(GTK)
            appendItem(CopyImageURLItem, m_contextMenu.get());
#endif
        }

        URL mediaURL = m_context.hitTestResult().absoluteMediaURL();
        if (!mediaURL.isEmpty()) {
            if (!linkURL.isEmpty() || !imageURL.isEmpty())
                appendItem(*separatorItem(), m_contextMenu.get());

            appendItem(MediaPlayPause, m_contextMenu.get());
            appendItem(MediaMute, m_contextMenu.get());
            appendItem(ToggleMediaControls, m_contextMenu.get());
            appendItem(ToggleMediaLoop, m_contextMenu.get());
#if SUPPORTS_TOGGLE_VIDEO_FULLSCREEN
            if (!m_context.hitTestResult().mediaIsInVideoViewer())
                appendItem(ToggleVideoFullscreen, m_contextMenu.get());
#else
            appendItem(EnterVideoFullscreen, m_contextMenu.get());
#endif
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
            appendItem(ToggleVideoEnhancedFullscreen, m_contextMenu.get());
            appendItem(ToggleVideoViewer, m_contextMenu.get());
#endif
            if (m_context.hitTestResult().isDownloadableMedia() && loader->client().canHandleRequest(ResourceRequest(mediaURL))) {
                appendItem(*separatorItem(), m_contextMenu.get());
                appendItem(CopyMediaLinkItem, m_contextMenu.get());
                appendItem(OpenMediaInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadMediaItem, m_contextMenu.get());
            }
        }

        auto selectedRange = frame->selection().selection().range();
        bool selectionIsInsideImageOverlay = selectedRange && ImageOverlay::isInsideOverlay(*selectedRange);
        if (selectionIsInsideImageOverlay || (linkURL.isEmpty() && mediaURL.isEmpty() && imageURL.isEmpty())) {
            if (!imageURL.isEmpty())
                appendItem(*separatorItem(), m_contextMenu.get());
            
            RefPtr page = frame->page();
            RefPtr ownerElement = frame->ownerElement();
            bool isPDFDocument = ownerElement && ownerElement->document().isPDFDocument();
            bool isMainFrame = frame->isMainFrame();

            if (m_context.hitTestResult().isSelected()) {
                addSelectedTextActionsIfNeeded(selectedText);

                appendItem(CopyItem, m_contextMenu.get());
                if (!selectionIsInsideImageOverlay && isMainFrame && page && page->settings().scrollToTextFragmentGenerationEnabled())
                    appendItem(CopyLinkToHighlightItem, m_contextMenu.get());
#if PLATFORM(COCOA)
                appendItem(*separatorItem(), m_contextMenu.get());

#if ENABLE(APP_HIGHLIGHTS)
                if (page && page->settings().appHighlightsEnabled() && !selectionIsInsideImageOverlay) {
                    appendItem(AddHighlightToNewQuickNoteItem, m_contextMenu.get());
                    appendItem(AddHighlightItem, m_contextMenu.get());
                    appendItem(*separatorItem(), m_contextMenu.get());
                }
#endif

                appendItem(ShareMenuItem, m_contextMenu.get());
                appendItem(*separatorItem(), m_contextMenu.get());

#if ENABLE(WRITING_TOOLS)
                appendItem(*separatorItem(), m_contextMenu.get());
                ContextMenuItem writingToolsItem(ContextMenuItemType::Action, ContextMenuItemTagWritingTools, contextMenuItemTagWritingTools());
                appendItem(writingToolsItem, m_contextMenu.get());
#endif

                ContextMenuItem SpeechMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagSpeechMenu, contextMenuItemTagSpeechMenu());
                createAndAppendSpeechSubMenu(SpeechMenuItem);
                appendItem(SpeechMenuItem, m_contextMenu.get());
#endif                
            } else {
                if (!(page && (page->inspectorController().inspectionLevel() > 0 || page->inspectorController().hasRemoteFrontend()))) {

                // In GTK+ unavailable items are not hidden but insensitive.
#if PLATFORM(GTK)
                appendItem(BackItem, m_contextMenu.get());
                appendItem(ForwardItem, m_contextMenu.get());
                appendItem(StopItem, m_contextMenu.get());
                appendItem(ReloadItem, m_contextMenu.get());
#else

                if (isMainFrame) {
                    if (page && page->backForward().canGoBackOrForward(-1))
                        appendItem(BackItem, m_contextMenu.get());

                    if (page && page->backForward().canGoBackOrForward(1))
                        appendItem(ForwardItem, m_contextMenu.get());

                    // Here we use isLoadingInAPISense rather than isLoading because Stop/Reload are
                    // intended to match WebKit's API, not WebCore's internal notion of loading status.
                    if (loader->documentLoader()->isLoadingInAPISense())
                        appendItem(StopItem, m_contextMenu.get());
                    else
                        appendItem(ReloadItem, m_contextMenu.get());
                }
#endif
                }

                if (page && !isMainFrame && !isPDFDocument) 
                    appendItem(OpenFrameItem, m_contextMenu.get());
                if (!ShareMenuItem.isNull()) {
                    appendItem(*separatorItem(), m_contextMenu.get());
                    appendItem(ShareMenuItem, m_contextMenu.get());
                }
            }
#if ENABLE(PDFJS)
            if (isPDFDocument) {
                if (m_contextMenu && !m_contextMenu->items().isEmpty())
                    appendItem(*separatorItem(), m_contextMenu.get());
                appendItem(PDFAutoSizeItem, m_contextMenu.get());
                appendItem(PDFZoomInItem, m_contextMenu.get());
                appendItem(PDFZoomOutItem, m_contextMenu.get());
                appendItem(PDFActualSizeItem, m_contextMenu.get());
                appendItem(*separatorItem(), m_contextMenu.get());

                appendItem(PDFSinglePageItem, m_contextMenu.get());
                appendItem(PDFSinglePageContinuousItem, m_contextMenu.get());
                appendItem(PDFTwoPagesItem, m_contextMenu.get());
                appendItem(PDFTwoPagesContinuousItem, m_contextMenu.get());
                appendItem(*separatorItem(), m_contextMenu.get());

                appendItem(PDFNextPageItem, m_contextMenu.get());
                appendItem(PDFPreviousPageItem, m_contextMenu.get());
            }
#endif
        } else if (!ShareMenuItem.isNull()) {
            appendItem(*separatorItem(), m_contextMenu.get());
            appendItem(ShareMenuItem, m_contextMenu.get());
        }
    } else { // Make an editing context menu
        bool inPasswordField = frame->selection().selection().isInPasswordField();
        if (!inPasswordField) {
            bool haveContextMenuItemsForMisspellingOrGrammer = false;
            bool spellCheckingEnabled = frame->editor().isSpellCheckingEnabledFor(node.get());
            if (spellCheckingEnabled) {
                // Consider adding spelling-related or grammar-related context menu items (never both, since a single selected range
                // is never considered a misspelling and bad grammar at the same time)
                auto [guesses, misspelling, badGrammar] = frame->editor().guessesForMisspelledOrUngrammatical();
                if (misspelling || badGrammar) {
                    if (guesses.isEmpty()) {
                        // If there's bad grammar but no suggestions (e.g., repeated word), just leave off the suggestions
                        // list and trailing separator rather than adding a "No Guesses Found" item (matches AppKit)
                        if (misspelling) {
                            appendItem(NoGuessesItem, m_contextMenu.get());
                            appendItem(*separatorItem(), m_contextMenu.get());
                        }
                    } else {
                        for (const auto& guess : guesses) {
                            if (!guess.isEmpty()) {
                                ContextMenuItem item(ContextMenuItemType::Action, ContextMenuItemTagSpellingGuess, guess);
                                appendItem(item, m_contextMenu.get());
                            }
                        }
                        appendItem(*separatorItem(), m_contextMenu.get());
                    }
                    if (misspelling) {
                        appendItem(IgnoreSpellingItem, m_contextMenu.get());
                        appendItem(LearnSpellingItem, m_contextMenu.get());
                    } else
                        appendItem(IgnoreGrammarItem, m_contextMenu.get());
                    appendItem(*separatorItem(), m_contextMenu.get());
                    haveContextMenuItemsForMisspellingOrGrammer = true;
#if PLATFORM(COCOA)
                } else {
                    // If the string was autocorrected, generate a contextual menu item allowing it to be changed back.
                    String replacedString = m_context.hitTestResult().replacedString();
                    if (!replacedString.isEmpty()) {
                        ContextMenuItem item(ContextMenuItemType::Action, ContextMenuItemTagChangeBack, contextMenuItemTagChangeBack(replacedString));
                        appendItem(item, m_contextMenu.get());
                        appendItem(*separatorItem(), m_contextMenu.get());
                        haveContextMenuItemsForMisspellingOrGrammer = true;
                    }
#endif
                }
            }

            if (!haveContextMenuItemsForMisspellingOrGrammer) {
                // Spelling and grammar checking is mutually exclusive with dictation alternatives.
                Vector<String> dictationAlternatives = m_context.hitTestResult().dictationAlternatives();
                if (!dictationAlternatives.isEmpty()) {
                    for (auto& alternative : dictationAlternatives) {
                        ContextMenuItem item(ContextMenuItemType::Action, ContextMenuItemTagDictationAlternative, alternative);
                        appendItem(item, m_contextMenu.get());
                    }
                    appendItem(*separatorItem(), m_contextMenu.get());
                }
            }
        }

        CheckedRef loader = frame->loader();
        URL linkURL = m_context.hitTestResult().absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader->client().canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem, m_contextMenu.get());
                appendItem(OpenLinkInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadFileItem, m_contextMenu.get());
            }
            appendItem(CopyLinkItem, m_contextMenu.get());
            appendItem(*separatorItem(), m_contextMenu.get());
        }

        if (m_context.hitTestResult().isSelected() && !inPasswordField)
            addSelectedTextActionsIfNeeded(selectedText);

        appendItem(CutItem, m_contextMenu.get());
        appendItem(CopyItem, m_contextMenu.get());
        appendItem(PasteItem, m_contextMenu.get());
#if PLATFORM(GTK)
        if (frame->editor().canEditRichly())
            appendItem(PasteAsPlainTextItem, m_contextMenu.get());
        appendItem(DeleteItem, m_contextMenu.get());
        appendItem(*separatorItem(), m_contextMenu.get());
        appendItem(SelectAllItem, m_contextMenu.get());
        appendItem(InsertEmojiItem, m_contextMenu.get());
#endif

        if (!inPasswordField) {
#if ENABLE(WRITING_TOOLS)
            appendItem(*separatorItem(), m_contextMenu.get());
            ContextMenuItem writingToolsItem(ContextMenuItemType::Action, ContextMenuItemTagWritingTools, contextMenuItemTagWritingTools());
            appendItem(writingToolsItem, m_contextMenu.get());
#endif

#if !PLATFORM(GTK)
#if !ENABLE(WRITING_TOOLS)
            appendItem(*separatorItem(), m_contextMenu.get());
#endif
            ContextMenuItem SpellingAndGrammarMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagSpellingMenu,
                contextMenuItemTagSpellingMenu());
            createAndAppendSpellingAndGrammarSubMenu(SpellingAndGrammarMenuItem);
            appendItem(SpellingAndGrammarMenuItem, m_contextMenu.get());
#endif
#if PLATFORM(COCOA)
            ContextMenuItem substitutionsMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagSubstitutionsMenu,
                contextMenuItemTagSubstitutionsMenu());
            createAndAppendSubstitutionsSubMenu(substitutionsMenuItem);
            appendItem(substitutionsMenuItem, m_contextMenu.get());
            ContextMenuItem transformationsMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagTransformationsMenu,
                contextMenuItemTagTransformationsMenu());
            createAndAppendTransformationsSubMenu(transformationsMenuItem);
            appendItem(transformationsMenuItem, m_contextMenu.get());
#endif
#if PLATFORM(GTK)
            bool shouldShowFontMenu = frame->editor().canEditRichly();
#else
            bool shouldShowFontMenu = true;
#endif
            if (shouldShowFontMenu) {
                ContextMenuItem FontMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagFontMenu,
                    contextMenuItemTagFontMenu());
                createAndAppendFontSubMenu(FontMenuItem);
                appendItem(FontMenuItem, m_contextMenu.get());
            }
#if PLATFORM(COCOA)
            ContextMenuItem SpeechMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagSpeechMenu, contextMenuItemTagSpeechMenu());
            createAndAppendSpeechSubMenu(SpeechMenuItem);
            appendItem(SpeechMenuItem, m_contextMenu.get());
#endif
#if PLATFORM(GTK)
            EditorClient* client = frame->editor().client();
            if (client && client->shouldShowUnicodeMenu()) {
                ContextMenuItem UnicodeMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagUnicode, contextMenuItemTagUnicode());
                createAndAppendUnicodeSubMenu(UnicodeMenuItem);
                appendItem(*separatorItem(), m_contextMenu.get());
                appendItem(UnicodeMenuItem, m_contextMenu.get());
            }
#else
            ContextMenuItem WritingDirectionMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagWritingDirectionMenu,
                contextMenuItemTagWritingDirectionMenu());
            createAndAppendWritingDirectionSubMenu(WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem, m_contextMenu.get());
            if (RefPtr page = frame->page()) {
                bool includeTextDirectionSubmenu = page->settings().textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuInclusionBehavior::AlwaysIncluded
                    || (page->settings().textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuInclusionBehavior::AutomaticallyIncluded && frame->editor().hasBidiSelection());
                if (includeTextDirectionSubmenu) {
                    ContextMenuItem TextDirectionMenuItem(ContextMenuItemType::Submenu, ContextMenuItemTagTextDirectionMenu, contextMenuItemTagTextDirectionMenu());
                    createAndAppendTextDirectionSubMenu(TextDirectionMenuItem);
                    appendItem(TextDirectionMenuItem, m_contextMenu.get());
                }
            }
#endif
        }

        if (!ShareMenuItem.isNull()) {
            appendItem(*separatorItem(), m_contextMenu.get());
            appendItem(ShareMenuItem, m_contextMenu.get());
        }
    }

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    if (shouldAddPlayAllPauseAllAnimationsItem) {
        appendItem(*separatorItem(), m_contextMenu.get());
        addPlayAllPauseAllAnimationsItem();
        appendItem(*separatorItem(), m_contextMenu.get());
    }
#endif
}

void ContextMenuController::addDebuggingItems()
{
    RefPtr node = m_context.hitTestResult().innerNonSharedNode();
    if (!node)
        return;

    RefPtr frame = node->document().frame();
    if (!frame)
        return;

    RefPtr page = frame->page();
    if (!page)
        return;
    ASSERT(page->inspectorController().enabled());

#if ENABLE(PDFJS)
    if (RefPtr ownerElement = frame->ownerElement(); ownerElement && ownerElement->document().isPDFDocument())
        return;
#endif

    if (m_contextMenu && !m_contextMenu->items().isEmpty())
        appendItem(*separatorItem(), m_contextMenu.get());

    ContextMenuItem InspectElementItem(ContextMenuItemType::Action, ContextMenuItemTagInspectElement, contextMenuItemTagInspectElement());
    appendItem(InspectElementItem, m_contextMenu.get());

#if ENABLE(VIDEO)
    if (page->settings().showMediaStatsContextMenuItemEnabled() && !m_context.hitTestResult().absoluteMediaURL().isEmpty()) {
        ContextMenuItem ShowMediaStats(ContextMenuItemType::CheckableAction, ContextMenuItemTagShowMediaStats, contextMenuItemTagShowMediaStats());
        appendItem(ShowMediaStats, m_contextMenu.get());
    }
#endif // ENABLE(VIDEO)
}

bool ContextMenuController::shouldEnableCopyLinkToHighlight() const
{
    RefPtr frame = m_context.hitTestResult().innerNonSharedNode()->document().frame();
    if (!frame)
        return false;

    auto selectedRange = frame->selection().selection().range();
    bool selectionIsInsideImageOverlay = selectedRange && ImageOverlay::isInsideOverlay(*selectedRange);
    if (frame->page() && frame->page()->settings().scrollToTextFragmentGenerationEnabled() && !selectionIsInsideImageOverlay)
        return frame->selection().isRange();
    return false;
}

void ContextMenuController::checkOrEnableIfNeeded(ContextMenuItem& item) const
{
    if (item.type() == ContextMenuItemType::Separator)
        return;
    
    RefPtr frame = m_context.hitTestResult().innerNonSharedNode()->document().frame();
    if (!frame)
        return;

    // Custom items already have proper checked and enabled values.
    if (ContextMenuItemBaseCustomTag <= item.action() && item.action() <= ContextMenuItemLastCustomTag)
        return;

    bool shouldEnable = true;
    bool shouldCheck = false; 

    switch (item.action()) {
        case ContextMenuItemTagCheckSpelling:
            shouldEnable = frame->editor().canEdit();
            break;
        case ContextMenuItemTagDefaultDirection:
            shouldCheck = false;
            shouldEnable = false;
            break;
        case ContextMenuItemTagLeftToRight:
        case ContextMenuItemTagRightToLeft: {
            String direction = item.action() == ContextMenuItemTagLeftToRight ? "ltr"_s : "rtl"_s;
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyDirection, direction) != TriState::False;
            shouldEnable = true;
            break;
        }
        case ContextMenuItemTagTextDirectionDefault: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionNatural"_s);
            shouldCheck = command.state() == TriState::True;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionLeftToRight: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionLeftToRight"_s);
            shouldCheck = command.state() == TriState::True;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionRightToLeft: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionRightToLeft"_s);
            shouldCheck = command.state() == TriState::True;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagCopy:
            shouldEnable = frame->editor().canDHTMLCopy() || frame->editor().canCopy();
            break;
        case ContextMenuItemTagCut:
            shouldEnable = frame->editor().canDHTMLCut() || frame->editor().canCut();
            break;
        case ContextMenuItemTagIgnoreSpelling:
        case ContextMenuItemTagLearnSpelling:
            shouldEnable = frame->selection().isRange();
            break;
        case ContextMenuItemTagPaste:
            shouldEnable = frame->editor().canDHTMLPaste() || frame->editor().canEdit();
            break;
        case ContextMenuItemTagCopyLinkToHighlight:
            shouldEnable = shouldEnableCopyLinkToHighlight();
            break;
#if PLATFORM(GTK)
        case ContextMenuItemTagPasteAsPlainText:
            shouldEnable = frame->editor().canDHTMLPaste() || frame->editor().canEdit();
            break;
        case ContextMenuItemTagDelete:
            shouldEnable = frame->editor().canDelete();
            break;
        case ContextMenuItemTagInsertEmoji:
            shouldEnable = frame->editor().canEdit();
            break;
        case ContextMenuItemTagSelectAll:
        case ContextMenuItemTagInputMethods:
        case ContextMenuItemTagUnicode:
        case ContextMenuItemTagUnicodeInsertLRMMark:
        case ContextMenuItemTagUnicodeInsertRLMMark:
        case ContextMenuItemTagUnicodeInsertLREMark:
        case ContextMenuItemTagUnicodeInsertRLEMark:
        case ContextMenuItemTagUnicodeInsertLROMark:
        case ContextMenuItemTagUnicodeInsertRLOMark:
        case ContextMenuItemTagUnicodeInsertPDFMark:
        case ContextMenuItemTagUnicodeInsertZWSMark:
        case ContextMenuItemTagUnicodeInsertZWJMark:
        case ContextMenuItemTagUnicodeInsertZWNJMark:
            shouldEnable = true;
            break;
#endif
        case ContextMenuItemTagUnderline: {
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline"_s) != TriState::False;
            shouldEnable = frame->editor().canEditRichly();
            break;
        }
        case ContextMenuItemTagLookUpInDictionary:
            shouldEnable = frame->selection().isRange();
            break;
        case ContextMenuItemTagCheckGrammarWithSpelling:
            if (frame->editor().isGrammarCheckingEnabled())
                shouldCheck = true;
            shouldEnable = true;
            break;
        case ContextMenuItemTagItalic: {
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyFontStyle, "italic"_s) != TriState::False;
            shouldEnable = frame->editor().canEditRichly();
            break;
        }
        case ContextMenuItemTagBold: {
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyFontWeight, "bold"_s) != TriState::False;
            shouldEnable = frame->editor().canEditRichly();
            break;
        }
        case ContextMenuItemTagOutline:
            shouldEnable = false;
            break;
        case ContextMenuItemTagShowSpellingPanel:
            if (frame->editor().spellingPanelIsShowing())
                item.setTitle(contextMenuItemTagShowSpellingPanel(false));
            else
                item.setTitle(contextMenuItemTagShowSpellingPanel(true));
            shouldEnable = frame->editor().canEdit();
            break;
        case ContextMenuItemTagNoGuessesFound:
            shouldEnable = false;
            break;
        case ContextMenuItemTagCheckSpellingWhileTyping:
            shouldCheck = frame->editor().isContinuousSpellCheckingEnabled();
            break;
        case ContextMenuItemTagAddHighlightToCurrentQuickNote:
            shouldEnable = frame->selection().isRange();
            break;
        case ContextMenuItemTagAddHighlightToNewQuickNote:
            shouldEnable = frame->selection().isRange();
            break;
#if PLATFORM(COCOA)
        case ContextMenuItemTagSubstitutionsMenu:
        case ContextMenuItemTagTransformationsMenu:
            break;
        case ContextMenuItemTagShowSubstitutions:
            if (frame->editor().substitutionsPanelIsShowing())
                item.setTitle(contextMenuItemTagShowSubstitutions(false));
            else
                item.setTitle(contextMenuItemTagShowSubstitutions(true));
            shouldEnable = frame->editor().canEdit();
            break;
        case ContextMenuItemTagMakeUpperCase:
        case ContextMenuItemTagMakeLowerCase:
        case ContextMenuItemTagCapitalize:
        case ContextMenuItemTagChangeBack:
            shouldEnable = frame->editor().canEdit();
            break;
        case ContextMenuItemTagCorrectSpellingAutomatically:
            shouldCheck = frame->editor().isAutomaticSpellingCorrectionEnabled();
            shouldEnable = frame->editor().canEnableAutomaticSpellingCorrection();
            break;
        case ContextMenuItemTagSmartCopyPaste:
            shouldCheck = frame->editor().smartInsertDeleteEnabled();
            break;
        case ContextMenuItemTagSmartQuotes:
            shouldCheck = frame->editor().isAutomaticQuoteSubstitutionEnabled();
            break;
        case ContextMenuItemTagSmartDashes:
            shouldCheck = frame->editor().isAutomaticDashSubstitutionEnabled();
            break;
        case ContextMenuItemTagSmartLinks:
            shouldCheck = frame->editor().isAutomaticLinkDetectionEnabled();
            break;
        case ContextMenuItemTagTextReplacement:
            shouldCheck = frame->editor().isAutomaticTextReplacementEnabled();
            break;
        case ContextMenuItemTagStopSpeaking:
            shouldEnable = m_client->isSpeaking();
            break;
#else // PLATFORM(COCOA) ends here
        case ContextMenuItemTagStopSpeaking:
            break;
#endif
#if PLATFORM(GTK)
        case ContextMenuItemTagGoBack:
            shouldEnable = frame->page() && frame->page()->backForward().canGoBackOrForward(-1);
            break;
        case ContextMenuItemTagGoForward:
            shouldEnable = frame->page() && frame->page()->backForward().canGoBackOrForward(1);
            break;
        case ContextMenuItemTagStop:
            shouldEnable = frame->loader().documentLoader()->isLoadingInAPISense();
            break;
        case ContextMenuItemTagReload:
            shouldEnable = !frame->loader().documentLoader()->isLoadingInAPISense();
            break;
        case ContextMenuItemTagFontMenu:
            shouldEnable = frame->editor().canEditRichly();
            break;
#else
        case ContextMenuItemTagGoBack:
        case ContextMenuItemTagGoForward:
        case ContextMenuItemTagStop:
        case ContextMenuItemTagReload:
        case ContextMenuItemTagFontMenu:
#endif
        case ContextMenuItemTagNoAction:
        case ContextMenuItemTagOpenLinkInNewWindow:
        case ContextMenuItemTagDownloadLinkToDisk:
        case ContextMenuItemTagCopyLinkToClipboard:
        case ContextMenuItemTagOpenImageInNewWindow:
        case ContextMenuItemTagCopyImageToClipboard:
        case ContextMenuItemTagCopySubject:
#if PLATFORM(GTK)
        case ContextMenuItemTagCopyImageURLToClipboard:
#endif
            break;
        case ContextMenuItemTagDownloadImageToDisk:
#if PLATFORM(MAC)
            if (m_context.hitTestResult().absoluteImageURL().protocolIsFile())
                shouldEnable = false;
#endif
            break;
        case ContextMenuItemTagOpenMediaInNewWindow:
            if (m_context.hitTestResult().mediaIsVideo())
                item.setTitle(contextMenuItemTagOpenVideoInNewWindow());
            else
                item.setTitle(contextMenuItemTagOpenAudioInNewWindow());
            break;
        case ContextMenuItemTagDownloadMediaToDisk:
            if (m_context.hitTestResult().mediaIsVideo())
                item.setTitle(contextMenuItemTagDownloadVideoToDisk());
            else
                item.setTitle(contextMenuItemTagDownloadAudioToDisk());
            if (m_context.hitTestResult().absoluteImageURL().protocolIsFile())
                shouldEnable = false;
            break;
        case ContextMenuItemTagCopyMediaLinkToClipboard:
            if (m_context.hitTestResult().mediaIsVideo())
                item.setTitle(contextMenuItemTagCopyVideoLinkToClipboard());
            else
                item.setTitle(contextMenuItemTagCopyAudioLinkToClipboard());
            break;
        case ContextMenuItemTagPlayAllAnimations:
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
            item.setTitle(contextMenuItemTagPlayAllAnimations());
#endif
            break;
        case ContextMenuItemTagPauseAllAnimations:
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
            item.setTitle(contextMenuItemTagPauseAllAnimations());
#endif
            break;
        case ContextMenuItemTagPlayAnimation:
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
            item.setTitle(contextMenuItemTagPlayAnimation());
#endif
            break;
        case ContextMenuItemTagPauseAnimation:
#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
            item.setTitle(contextMenuItemTagPauseAnimation());
#endif
            break;
        case ContextMenuItemTagToggleMediaControls:
#if SUPPORTS_TOGGLE_SHOW_HIDE_MEDIA_CONTROLS
            item.setTitle(m_context.hitTestResult().mediaControlsEnabled() ? contextMenuItemTagHideMediaControls() : contextMenuItemTagShowMediaControls());
#else
            shouldCheck = m_context.hitTestResult().mediaControlsEnabled();
#endif
            break;
        case ContextMenuItemTagToggleMediaLoop:
            shouldCheck = m_context.hitTestResult().mediaLoopEnabled();
            break;
        case ContextMenuItemTagShowMediaStats:
            shouldCheck = m_context.hitTestResult().mediaStatsShowing();
            break;
        case ContextMenuItemTagToggleVideoFullscreen:
#if SUPPORTS_TOGGLE_VIDEO_FULLSCREEN
            item.setTitle(m_context.hitTestResult().mediaIsInFullscreen() ? contextMenuItemTagExitVideoFullscreen() : contextMenuItemTagEnterVideoFullscreen());
            break;
#endif
        case ContextMenuItemTagEnterVideoFullscreen:
            shouldEnable = m_context.hitTestResult().mediaSupportsFullscreen();
            break;
        case ContextMenuItemTagToggleVideoEnhancedFullscreen:
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
            item.setTitle(m_context.hitTestResult().mediaIsInEnhancedFullscreen() ? contextMenuItemTagExitVideoEnhancedFullscreen() : contextMenuItemTagEnterVideoEnhancedFullscreen());
#endif
            shouldEnable = m_context.hitTestResult().mediaSupportsEnhancedFullscreen();
            break;
        case ContextMenuItemTagToggleVideoViewer:
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
            item.setTitle(m_context.hitTestResult().mediaIsInVideoViewer() ? contextMenuItemTagExitVideoViewer() : contextMenuItemTagEnterVideoViewer());
#endif
            break;
        case ContextMenuItemTagOpenFrameInNewWindow:
        case ContextMenuItemTagSpellingGuess:
        case ContextMenuItemTagOther:
        case ContextMenuItemTagSearchWeb:
        case ContextMenuItemTagOpenWithDefaultApplication:
        case ContextMenuItemPDFActualSize:
        case ContextMenuItemPDFZoomIn:
        case ContextMenuItemPDFZoomOut:
        case ContextMenuItemPDFAutoSize:
        case ContextMenuItemPDFSinglePage:
        case ContextMenuItemPDFSinglePageContinuous:
        case ContextMenuItemPDFTwoPages:
        case ContextMenuItemPDFTwoPagesContinuous:
        case ContextMenuItemPDFFacingPages:
        case ContextMenuItemPDFContinuous:
        case ContextMenuItemPDFNextPage:
        case ContextMenuItemPDFPreviousPage:
        case ContextMenuItemTagOpenLink:
        case ContextMenuItemTagIgnoreGrammar:
        case ContextMenuItemTagSpellingMenu:
        case ContextMenuItemTagShowFonts:
        case ContextMenuItemTagStyles:
        case ContextMenuItemTagShowColors:
        case ContextMenuItemTagSpeechMenu:
        case ContextMenuItemTagStartSpeaking:
        case ContextMenuItemTagWritingDirectionMenu:
        case ContextMenuItemTagTextDirectionMenu:
        case ContextMenuItemTagPDFSinglePageScrolling:
        case ContextMenuItemTagPDFFacingPagesScrolling:
        case ContextMenuItemTagInspectElement:
        case ContextMenuItemBaseCustomTag:
        case ContextMenuItemLastCustomTag:
        case ContextMenuItemBaseApplicationTag:
        case ContextMenuItemTagDictationAlternative:
        case ContextMenuItemTagShareMenu:
            break;
        case ContextMenuItemTagMediaPlayPause:
            if (m_context.hitTestResult().mediaPlaying())
                item.setTitle(contextMenuItemTagMediaPause());
            else
                item.setTitle(contextMenuItemTagMediaPlay());
            break;
        case ContextMenuItemTagMediaMute:
            shouldEnable = m_context.hitTestResult().mediaHasAudio();
            shouldCheck = shouldEnable &&  m_context.hitTestResult().mediaMuted();
            break;
        case ContextMenuItemTagLookUpImage:
        case ContextMenuItemTagTranslate:
        case ContextMenuItemTagWritingTools:
            break;
    }

    item.setChecked(shouldCheck);
    item.setEnabled(shouldEnable);
}

#if USE(ACCESSIBILITY_CONTEXT_MENUS)

void ContextMenuController::showContextMenuAt(LocalFrame& frame, const IntPoint& clickPoint)
{
    clearContextMenu();
    
    // Simulate a click in the middle of the accessibility object.
    PlatformMouseEvent mouseEvent(clickPoint, clickPoint, MouseButton::Right, PlatformEvent::Type::MousePressed, 1, { }, WallTime::now(), ForceAtClick, SyntheticClickType::NoTap);

    frame.checkedEventHandler()->handleMousePressEvent(mouseEvent);
    bool handled = frame.checkedEventHandler()->sendContextMenuEvent(mouseEvent);
    if (handled)
        m_client->showContextMenu();
}

#endif

#if ENABLE(SERVICE_CONTROLS)

void ContextMenuController::showImageControlsMenu(Event& event)
{
    clearContextMenu();
    handleContextMenuEvent(event);
    m_client->showContextMenu();
}

#endif

#if ENABLE(PDFJS)

void ContextMenuController::performPDFJSAction(LocalFrame& frame, const String& action)
{
    if (RefPtr document = dynamicDowncast<PDFDocument>(frame.ownerElement()->document()))
        document->postMessageToIframe(action, nullptr);
}

#endif

} // namespace WebCore

#endif // ENABLE(CONTEXT_MENUS)
