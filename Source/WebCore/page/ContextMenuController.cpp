/*
 * Copyright (C) 2006-2016 Apple Inc. All rights reserved.
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
#include "Event.h"
#include "EventHandler.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "FrameSelection.h"
#include "HTMLFormControlElement.h"
#include "HTMLFormElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "InspectorController.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "NavigationAction.h"
#include "Node.h"
#include "Page.h"
#include "PlatformEvent.h"
#include "RenderImage.h"
#include "ReplaceSelectionCommand.h"
#include "ResourceRequest.h"
#include "Settings.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "UserTypingGestureIndicator.h"
#include "WindowFeatures.h"
#include "markup.h"
#include <wtf/SetForScope.h>
#include <wtf/WallTime.h>
#include <wtf/unicode/CharacterNames.h>


namespace WebCore {

using namespace WTF::Unicode;

ContextMenuController::ContextMenuController(Page& page, ContextMenuClient& client)
    : m_page(page)
    , m_client(client)
{
}

ContextMenuController::~ContextMenuController()
{
    m_client.contextMenuDestroyed();
}

void ContextMenuController::clearContextMenu()
{
    m_contextMenu = nullptr;
    if (m_menuProvider)
        m_menuProvider->contextMenuCleared();
    m_menuProvider = nullptr;
}

void ContextMenuController::handleContextMenuEvent(Event& event)
{
    if (m_isHandlingContextMenuEvent)
        return;

    SetForScope<bool> isHandlingContextMenuEventForScope(m_isHandlingContextMenuEvent, true);

    m_contextMenu = maybeCreateContextMenu(event);
    if (!m_contextMenu)
        return;

    populate();

    showContextMenu(event);
}

static std::unique_ptr<ContextMenuItem> separatorItem()
{
    return std::unique_ptr<ContextMenuItem>(new ContextMenuItem(SeparatorType, ContextMenuItemTagNoAction, String()));
}

void ContextMenuController::showContextMenu(Event& event, ContextMenuProvider& provider)
{
    m_menuProvider = &provider;

    m_contextMenu = maybeCreateContextMenu(event);
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

#if ENABLE(SERVICE_CONTROLS)

static Image* imageFromImageElementNode(Node& node)
{
    auto* renderer = node.renderer();
    if (!is<RenderImage>(renderer))
        return nullptr;
    auto* image = downcast<RenderImage>(*renderer).cachedImage();
    if (!image || image->errorOccurred())
        return nullptr;
    return image->imageForRenderer(renderer);
}

#endif

std::unique_ptr<ContextMenu> ContextMenuController::maybeCreateContextMenu(Event& event)
{
    if (!is<MouseEvent>(event))
        return nullptr;

    auto& mouseEvent = downcast<MouseEvent>(event);
    if (!is<Node>(mouseEvent.target()))
        return nullptr;
    auto& node = downcast<Node>(*mouseEvent.target());
    auto* frame = node.document().frame();
    if (!frame)
        return nullptr;

    constexpr OptionSet<HitTestRequest::RequestType> hitType { HitTestRequest::ReadOnly, HitTestRequest::Active, HitTestRequest::DisallowUserAgentShadowContent, HitTestRequest::AllowChildFrameContent };
    auto result = frame->eventHandler().hitTestResultAtPoint(mouseEvent.absoluteLocation(), hitType);
    if (!result.innerNonSharedNode())
        return nullptr;

    m_context = ContextMenuContext(result);

#if ENABLE(SERVICE_CONTROLS)
    if (node.isImageControlsButtonElement()) {
        if (auto* image = imageFromImageElementNode(*result.innerNonSharedNode()))
            m_context.setControlledImage(image);

        // FIXME: If we couldn't get the image then we shouldn't try to show the image controls menu for it.
        return nullptr;
    }
#endif

    return makeUnique<ContextMenu>();
}

void ContextMenuController::showContextMenu(Event& event)
{
    if (m_page.inspectorController().enabled())
        addInspectElementItem();

    event.setDefaultHandled();
}

static void openNewWindow(const URL& urlToLoad, Frame& frame, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy)
{
    Page* oldPage = frame.page();
    if (!oldPage)
        return;

    FrameLoadRequest frameLoadRequest { *frame.document(), frame.document()->securityOrigin(), ResourceRequest(urlToLoad, frame.loader().outgoingReferrer()), { }, InitiatedByMainFrame::Unknown };
    frameLoadRequest.setShouldOpenExternalURLsPolicy(shouldOpenExternalURLsPolicy);
    frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);

    Page* newPage = oldPage->chrome().createWindow(frame, { }, { *frame.document(), frameLoadRequest.resourceRequest(), frameLoadRequest.initiatedByMainFrame() });
    if (!newPage)
        return;
    newPage->chrome().show();
    newPage->mainFrame().loader().loadFrameRequest(WTFMove(frameLoadRequest), nullptr, { });
}

#if PLATFORM(GTK)

static void insertUnicodeCharacter(UChar character, Frame& frame)
{
    String text(&character, 1);
    if (!frame.editor().shouldInsertText(text, frame.selection().selection().toNormalizedRange(), EditorInsertAction::Typed))
        return;

    ASSERT(frame.document());
    TypingCommand::insertText(*frame.document(), text, 0, TypingCommand::TextCompositionNone);
}

#endif

void ContextMenuController::contextMenuItemSelected(ContextMenuAction action, const String& title)
{
    if (action >= ContextMenuItemBaseCustomTag) {
        ASSERT(m_menuProvider);
        m_menuProvider->contextMenuItemSelected(action, title);
        return;
    }

    auto& document = m_context.hitTestResult().innerNonSharedNode()->document();

    auto* frame = document.frame();
    if (!frame)
        return;

    Ref<Frame> protector(*frame);

    switch (action) {
    case ContextMenuItemTagOpenLinkInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteLinkURL(), *frame, ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes);
        break;
    case ContextMenuItemTagDownloadLinkToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client.downloadURL(m_context.hitTestResult().absoluteLinkURL());
        break;
    case ContextMenuItemTagCopyLinkToClipboard:
        frame->editor().copyURL(m_context.hitTestResult().absoluteLinkURL(), m_context.hitTestResult().textContent());
        break;
    case ContextMenuItemTagOpenImageInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteImageURL(), *frame, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    case ContextMenuItemTagDownloadImageToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client.downloadURL(m_context.hitTestResult().absoluteImageURL());
        break;
    case ContextMenuItemTagCopyImageToClipboard:
        // FIXME: The Pasteboard class is not written yet
        // For now, call into the client. This is temporary!
        frame->editor().copyImage(m_context.hitTestResult());
        break;
#if PLATFORM(GTK)
    case ContextMenuItemTagCopyImageUrlToClipboard:
        frame->editor().copyURL(m_context.hitTestResult().absoluteImageURL(), m_context.hitTestResult().textContent());
        break;
#endif
    case ContextMenuItemTagOpenMediaInNewWindow:
        openNewWindow(m_context.hitTestResult().absoluteMediaURL(), *frame, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    case ContextMenuItemTagDownloadMediaToDisk:
        // FIXME: Some day we should be able to do this from within WebCore. (Bug 117709)
        m_client.downloadURL(m_context.hitTestResult().absoluteMediaURL());
        break;
    case ContextMenuItemTagCopyMediaLinkToClipboard:
        frame->editor().copyURL(m_context.hitTestResult().absoluteMediaURL(), m_context.hitTestResult().textContent());
        break;
    case ContextMenuItemTagToggleMediaControls:
        m_context.hitTestResult().toggleMediaControlsDisplay();
        break;
    case ContextMenuItemTagToggleMediaLoop:
        m_context.hitTestResult().toggleMediaLoopPlayback();
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
    case ContextMenuItemTagOpenFrameInNewWindow: {
        DocumentLoader* loader = frame->loader().documentLoader();
        if (!loader->unreachableURL().isEmpty())
            openNewWindow(loader->unreachableURL(), *frame, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        else
            openNewWindow(loader->url(), *frame, ShouldOpenExternalURLsPolicy::ShouldNotAllow);
        break;
    }
    case ContextMenuItemTagCopy:
        frame->editor().copy();
        break;
    case ContextMenuItemTagGoBack:
        if (Page* page = frame->page())
            page->backForward().goBackOrForward(-1);
        break;
    case ContextMenuItemTagGoForward:
        if (Page* page = frame->page())
            page->backForward().goBackOrForward(1);
        break;
    case ContextMenuItemTagStop:
        frame->loader().stop();
        break;
    case ContextMenuItemTagReload:
        frame->loader().reload();
        break;
    case ContextMenuItemTagCut:
        frame->editor().command("Cut").execute();
        break;
    case ContextMenuItemTagPaste:
        frame->editor().command("Paste").execute();
        break;
#if PLATFORM(GTK)
    case ContextMenuItemTagPasteAsPlainText:
        frame->editor().command("PasteAsPlainText").execute();
        break;
    case ContextMenuItemTagDelete:
        frame->editor().performDelete();
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
        frame->editor().command("SelectAll").execute();
        break;
    case ContextMenuItemTagInsertEmoji:
        m_client.insertEmoji(*frame);
        break;
#endif
    case ContextMenuItemTagSpellingGuess: {
        VisibleSelection selection = frame->selection().selection();
        if (frame->editor().shouldInsertText(title, selection.toNormalizedRange(), EditorInsertAction::Pasted)) {
            OptionSet<ReplaceSelectionCommand::CommandOption> replaceOptions { ReplaceSelectionCommand::MatchStyle, ReplaceSelectionCommand::PreventNesting };

            if (frame->editor().behavior().shouldAllowSpellingSuggestionsWithoutSelection()) {
                ASSERT(selection.isCaretOrRange());
                VisibleSelection wordSelection(selection.base());
                wordSelection.expandUsingGranularity(TextGranularity::WordGranularity);
                frame->selection().setSelection(wordSelection);
            } else {
                ASSERT(frame->editor().selectedText().length());
                replaceOptions.add(ReplaceSelectionCommand::SelectReplacement);
            }

            Document* document = frame->document();
            ASSERT(document);
            auto command = ReplaceSelectionCommand::create(*document, createFragmentFromMarkup(*document, title, emptyString()), replaceOptions);
            command->apply();
            frame->selection().revealSelection(SelectionRevealMode::Reveal, ScrollAlignment::alignToEdgeIfNeeded);
        }
        break;
    }
    case ContextMenuItemTagIgnoreSpelling:
        frame->editor().ignoreSpelling();
        break;
    case ContextMenuItemTagLearnSpelling:
        frame->editor().learnSpelling();
        break;
    case ContextMenuItemTagSearchWeb:
        m_client.searchWithGoogle(frame);
        break;
    case ContextMenuItemTagLookUpInDictionary:
        // FIXME: Some day we may be able to do this from within WebCore.
        m_client.lookUpInDictionary(frame);
        break;
    case ContextMenuItemTagOpenLink:
        if (Frame* targetFrame = m_context.hitTestResult().targetFrame()) {
            ResourceRequest resourceRequest { m_context.hitTestResult().absoluteLinkURL(), frame->loader().outgoingReferrer() };
            FrameLoadRequest frameLoadRequest { *frame->document(), frame->document()->securityOrigin(), WTFMove(resourceRequest), { }, InitiatedByMainFrame::Unknown };
            frameLoadRequest.setNewFrameOpenerPolicy(NewFrameOpenerPolicy::Suppress);
            if (targetFrame->isMainFrame())
                frameLoadRequest.setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy::ShouldAllow);
            targetFrame->loader().loadFrameRequest(WTFMove(frameLoadRequest), nullptr,  { });
        } else
            openNewWindow(m_context.hitTestResult().absoluteLinkURL(), *frame, ShouldOpenExternalURLsPolicy::ShouldAllow);
        break;
    case ContextMenuItemTagBold:
        frame->editor().command("ToggleBold").execute();
        break;
    case ContextMenuItemTagItalic:
        frame->editor().command("ToggleItalic").execute();
        break;
    case ContextMenuItemTagUnderline:
        frame->editor().toggleUnderline();
        break;
    case ContextMenuItemTagOutline:
        // We actually never enable this because CSS does not have a way to specify an outline font,
        // which may make this difficult to implement. Maybe a special case of text-shadow?
        break;
    case ContextMenuItemTagStartSpeaking: {
        auto selectedRange = frame->selection().selection().toNormalizedRange();
        if (!selectedRange || selectedRange->collapsed())
            selectedRange = makeRangeSelectingNodeContents(document);
        m_client.speak(plainText(*selectedRange));
        break;
    }
    case ContextMenuItemTagStopSpeaking:
        m_client.stopSpeaking();
        break;
    case ContextMenuItemTagDefaultDirection:
        frame->editor().setBaseWritingDirection(WritingDirection::Natural);
        break;
    case ContextMenuItemTagLeftToRight:
        frame->editor().setBaseWritingDirection(WritingDirection::LeftToRight);
        break;
    case ContextMenuItemTagRightToLeft:
        frame->editor().setBaseWritingDirection(WritingDirection::RightToLeft);
        break;
    case ContextMenuItemTagTextDirectionDefault:
        frame->editor().command("MakeTextWritingDirectionNatural").execute();
        break;
    case ContextMenuItemTagTextDirectionLeftToRight:
        frame->editor().command("MakeTextWritingDirectionLeftToRight").execute();
        break;
    case ContextMenuItemTagTextDirectionRightToLeft:
        frame->editor().command("MakeTextWritingDirectionRightToLeft").execute();
        break;
#if PLATFORM(COCOA)
    case ContextMenuItemTagSearchInSpotlight:
        m_client.searchWithSpotlight();
        break;
#endif
    case ContextMenuItemTagShowSpellingPanel:
        frame->editor().showSpellingGuessPanel();
        break;
    case ContextMenuItemTagCheckSpelling:
        frame->editor().advanceToNextMisspelling();
        break;
    case ContextMenuItemTagCheckSpellingWhileTyping:
        frame->editor().toggleContinuousSpellChecking();
        break;
    case ContextMenuItemTagCheckGrammarWithSpelling:
        frame->editor().toggleGrammarChecking();
        break;
#if PLATFORM(COCOA)
    case ContextMenuItemTagShowFonts:
        frame->editor().showFontPanel();
        break;
    case ContextMenuItemTagStyles:
        frame->editor().showStylesPanel();
        break;
    case ContextMenuItemTagShowColors:
        frame->editor().showColorPanel();
        break;
#endif
#if USE(APPKIT)
    case ContextMenuItemTagMakeUpperCase:
        frame->editor().uppercaseWord();
        break;
    case ContextMenuItemTagMakeLowerCase:
        frame->editor().lowercaseWord();
        break;
    case ContextMenuItemTagCapitalize:
        frame->editor().capitalizeWord();
        break;
#endif
#if PLATFORM(COCOA)
    case ContextMenuItemTagChangeBack:
        frame->editor().changeBackToReplacedString(m_context.hitTestResult().replacedString());
        break;
#endif
#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    case ContextMenuItemTagShowSubstitutions:
        frame->editor().showSubstitutionsPanel();
        break;
    case ContextMenuItemTagSmartCopyPaste:
        frame->editor().toggleSmartInsertDelete();
        break;
    case ContextMenuItemTagSmartQuotes:
        frame->editor().toggleAutomaticQuoteSubstitution();
        break;
    case ContextMenuItemTagSmartDashes:
        frame->editor().toggleAutomaticDashSubstitution();
        break;
    case ContextMenuItemTagSmartLinks:
        frame->editor().toggleAutomaticLinkDetection();
        break;
    case ContextMenuItemTagTextReplacement:
        frame->editor().toggleAutomaticTextReplacement();
        break;
    case ContextMenuItemTagCorrectSpellingAutomatically:
        frame->editor().toggleAutomaticSpellingCorrection();
        break;
#endif
    case ContextMenuItemTagInspectElement:
        if (Page* page = frame->page())
            page->inspectorController().inspect(m_context.hitTestResult().innerNonSharedNode());
        break;
    case ContextMenuItemTagDictationAlternative:
        frame->editor().applyDictationAlternativelternative(title);
        break;
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
    ContextMenuItem showFonts(ActionType, ContextMenuItemTagShowFonts, contextMenuItemTagShowFonts());
#endif
    ContextMenuItem bold(CheckableActionType, ContextMenuItemTagBold, contextMenuItemTagBold());
    ContextMenuItem italic(CheckableActionType, ContextMenuItemTagItalic, contextMenuItemTagItalic());
    ContextMenuItem underline(CheckableActionType, ContextMenuItemTagUnderline, contextMenuItemTagUnderline());
    ContextMenuItem outline(ActionType, ContextMenuItemTagOutline, contextMenuItemTagOutline());
#if PLATFORM(COCOA)
    ContextMenuItem styles(ActionType, ContextMenuItemTagStyles, contextMenuItemTagStyles());
    ContextMenuItem showColors(ActionType, ContextMenuItemTagShowColors, contextMenuItemTagShowColors());
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

    ContextMenuItem showSpellingPanel(ActionType, ContextMenuItemTagShowSpellingPanel, 
        contextMenuItemTagShowSpellingPanel(true));
    ContextMenuItem checkSpelling(ActionType, ContextMenuItemTagCheckSpelling, 
        contextMenuItemTagCheckSpelling());
    ContextMenuItem checkAsYouType(CheckableActionType, ContextMenuItemTagCheckSpellingWhileTyping, 
        contextMenuItemTagCheckSpellingWhileTyping());
    ContextMenuItem grammarWithSpelling(CheckableActionType, ContextMenuItemTagCheckGrammarWithSpelling, 
        contextMenuItemTagCheckGrammarWithSpelling());
#if PLATFORM(COCOA)
    ContextMenuItem correctSpelling(CheckableActionType, ContextMenuItemTagCorrectSpellingAutomatically, 
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

    ContextMenuItem start(ActionType, ContextMenuItemTagStartSpeaking, contextMenuItemTagStartSpeaking());
    ContextMenuItem stop(ActionType, ContextMenuItemTagStopSpeaking, contextMenuItemTagStopSpeaking());

    appendItem(start, &speechMenu);
    appendItem(stop, &speechMenu);

    speechMenuItem.setSubMenu(&speechMenu);
}

#endif
 
#if PLATFORM(GTK)

void ContextMenuController::createAndAppendUnicodeSubMenu(ContextMenuItem& unicodeMenuItem)
{
    ContextMenu unicodeMenu;

    ContextMenuItem leftToRightMarkMenuItem(ActionType, ContextMenuItemTagUnicodeInsertLRMMark, contextMenuItemTagUnicodeInsertLRMMark());
    ContextMenuItem rightToLeftMarkMenuItem(ActionType, ContextMenuItemTagUnicodeInsertRLMMark, contextMenuItemTagUnicodeInsertRLMMark());
    ContextMenuItem leftToRightEmbedMenuItem(ActionType, ContextMenuItemTagUnicodeInsertLREMark, contextMenuItemTagUnicodeInsertLREMark());
    ContextMenuItem rightToLeftEmbedMenuItem(ActionType, ContextMenuItemTagUnicodeInsertRLEMark, contextMenuItemTagUnicodeInsertRLEMark());
    ContextMenuItem leftToRightOverrideMenuItem(ActionType, ContextMenuItemTagUnicodeInsertLROMark, contextMenuItemTagUnicodeInsertLROMark());
    ContextMenuItem rightToLeftOverrideMenuItem(ActionType, ContextMenuItemTagUnicodeInsertRLOMark, contextMenuItemTagUnicodeInsertRLOMark());
    ContextMenuItem popDirectionalFormattingMenuItem(ActionType, ContextMenuItemTagUnicodeInsertPDFMark, contextMenuItemTagUnicodeInsertPDFMark());
    ContextMenuItem zeroWidthSpaceMenuItem(ActionType, ContextMenuItemTagUnicodeInsertZWSMark, contextMenuItemTagUnicodeInsertZWSMark());
    ContextMenuItem zeroWidthJoinerMenuItem(ActionType, ContextMenuItemTagUnicodeInsertZWJMark, contextMenuItemTagUnicodeInsertZWJMark());
    ContextMenuItem zeroWidthNonJoinerMenuItem(ActionType, ContextMenuItemTagUnicodeInsertZWNJMark, contextMenuItemTagUnicodeInsertZWNJMark());

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

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagDefaultDirection, 
        contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagRightToLeft, contextMenuItemTagRightToLeft());

    appendItem(defaultItem, &writingDirectionMenu);
    appendItem(ltr, &writingDirectionMenu);
    appendItem(rtl, &writingDirectionMenu);

    writingDirectionMenuItem.setSubMenu(&writingDirectionMenu);
}

void ContextMenuController::createAndAppendTextDirectionSubMenu(ContextMenuItem& textDirectionMenuItem)
{
    ContextMenu textDirectionMenu;

    ContextMenuItem defaultItem(ActionType, ContextMenuItemTagTextDirectionDefault, contextMenuItemTagDefaultDirection());
    ContextMenuItem ltr(CheckableActionType, ContextMenuItemTagTextDirectionLeftToRight, contextMenuItemTagLeftToRight());
    ContextMenuItem rtl(CheckableActionType, ContextMenuItemTagTextDirectionRightToLeft, contextMenuItemTagRightToLeft());

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

    ContextMenuItem showSubstitutions(ActionType, ContextMenuItemTagShowSubstitutions, contextMenuItemTagShowSubstitutions(true));
    ContextMenuItem smartCopyPaste(CheckableActionType, ContextMenuItemTagSmartCopyPaste, contextMenuItemTagSmartCopyPaste());
    ContextMenuItem smartQuotes(CheckableActionType, ContextMenuItemTagSmartQuotes, contextMenuItemTagSmartQuotes());
    ContextMenuItem smartDashes(CheckableActionType, ContextMenuItemTagSmartDashes, contextMenuItemTagSmartDashes());
    ContextMenuItem smartLinks(CheckableActionType, ContextMenuItemTagSmartLinks, contextMenuItemTagSmartLinks());
    ContextMenuItem textReplacement(CheckableActionType, ContextMenuItemTagTextReplacement, contextMenuItemTagTextReplacement());

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

    ContextMenuItem makeUpperCase(ActionType, ContextMenuItemTagMakeUpperCase, contextMenuItemTagMakeUpperCase());
    ContextMenuItem makeLowerCase(ActionType, ContextMenuItemTagMakeLowerCase, contextMenuItemTagMakeLowerCase());
    ContextMenuItem capitalize(ActionType, ContextMenuItemTagCapitalize, contextMenuItemTagCapitalize());

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
    ContextMenuItem OpenLinkItem(ActionType, ContextMenuItemTagOpenLink, contextMenuItemTagOpenLink());
    ContextMenuItem OpenLinkInNewWindowItem(ActionType, ContextMenuItemTagOpenLinkInNewWindow, 
        contextMenuItemTagOpenLinkInNewWindow());
    ContextMenuItem DownloadFileItem(ActionType, ContextMenuItemTagDownloadLinkToDisk, 
        contextMenuItemTagDownloadLinkToDisk());
    ContextMenuItem CopyLinkItem(ActionType, ContextMenuItemTagCopyLinkToClipboard, 
        contextMenuItemTagCopyLinkToClipboard());
    ContextMenuItem OpenImageInNewWindowItem(ActionType, ContextMenuItemTagOpenImageInNewWindow, 
        contextMenuItemTagOpenImageInNewWindow());
    ContextMenuItem DownloadImageItem(ActionType, ContextMenuItemTagDownloadImageToDisk, 
        contextMenuItemTagDownloadImageToDisk());
    ContextMenuItem CopyImageItem(ActionType, ContextMenuItemTagCopyImageToClipboard, 
        contextMenuItemTagCopyImageToClipboard());
#if PLATFORM(GTK)
    ContextMenuItem CopyImageUrlItem(ActionType, ContextMenuItemTagCopyImageUrlToClipboard, 
        contextMenuItemTagCopyImageUrlToClipboard());
#endif
    ContextMenuItem OpenMediaInNewWindowItem(ActionType, ContextMenuItemTagOpenMediaInNewWindow, String());
    ContextMenuItem DownloadMediaItem(ActionType, ContextMenuItemTagDownloadMediaToDisk, String());
    ContextMenuItem CopyMediaLinkItem(ActionType, ContextMenuItemTagCopyMediaLinkToClipboard, String());
    ContextMenuItem MediaPlayPause(ActionType, ContextMenuItemTagMediaPlayPause, 
        contextMenuItemTagMediaPlay());
    ContextMenuItem MediaMute(ActionType, ContextMenuItemTagMediaMute, 
        contextMenuItemTagMediaMute());
#if SUPPORTS_TOGGLE_SHOW_HIDE_MEDIA_CONTROLS
    ContextMenuItem ToggleMediaControls(ActionType, ContextMenuItemTagToggleMediaControls,
        contextMenuItemTagHideMediaControls());
#else
    ContextMenuItem ToggleMediaControls(CheckableActionType, ContextMenuItemTagToggleMediaControls, 
        contextMenuItemTagToggleMediaControls());
#endif
    ContextMenuItem ToggleMediaLoop(CheckableActionType, ContextMenuItemTagToggleMediaLoop, 
        contextMenuItemTagToggleMediaLoop());
    ContextMenuItem EnterVideoFullscreen(ActionType, ContextMenuItemTagEnterVideoFullscreen,
        contextMenuItemTagEnterVideoFullscreen());
    ContextMenuItem ToggleVideoFullscreen(ActionType, ContextMenuItemTagToggleVideoFullscreen,
        contextMenuItemTagEnterVideoFullscreen());
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    ContextMenuItem ToggleVideoEnhancedFullscreen(ActionType, ContextMenuItemTagToggleVideoEnhancedFullscreen, contextMenuItemTagEnterVideoEnhancedFullscreen());
#endif
#if PLATFORM(COCOA)
    ContextMenuItem SearchSpotlightItem(ActionType, ContextMenuItemTagSearchInSpotlight, 
        contextMenuItemTagSearchInSpotlight());
#endif
#if !PLATFORM(GTK)
    ContextMenuItem SearchWebItem(ActionType, ContextMenuItemTagSearchWeb, contextMenuItemTagSearchWeb());
#endif
    ContextMenuItem CopyItem(ActionType, ContextMenuItemTagCopy, contextMenuItemTagCopy());
    ContextMenuItem BackItem(ActionType, ContextMenuItemTagGoBack, contextMenuItemTagGoBack());
    ContextMenuItem ForwardItem(ActionType, ContextMenuItemTagGoForward,  contextMenuItemTagGoForward());
    ContextMenuItem StopItem(ActionType, ContextMenuItemTagStop, contextMenuItemTagStop());
    ContextMenuItem ReloadItem(ActionType, ContextMenuItemTagReload, contextMenuItemTagReload());
    ContextMenuItem OpenFrameItem(ActionType, ContextMenuItemTagOpenFrameInNewWindow, 
        contextMenuItemTagOpenFrameInNewWindow());
    ContextMenuItem NoGuessesItem(ActionType, ContextMenuItemTagNoGuessesFound, 
        contextMenuItemTagNoGuessesFound());
    ContextMenuItem IgnoreSpellingItem(ActionType, ContextMenuItemTagIgnoreSpelling, 
        contextMenuItemTagIgnoreSpelling());
    ContextMenuItem LearnSpellingItem(ActionType, ContextMenuItemTagLearnSpelling, 
        contextMenuItemTagLearnSpelling());
    ContextMenuItem IgnoreGrammarItem(ActionType, ContextMenuItemTagIgnoreGrammar, 
        contextMenuItemTagIgnoreGrammar());
    ContextMenuItem CutItem(ActionType, ContextMenuItemTagCut, contextMenuItemTagCut());
    ContextMenuItem PasteItem(ActionType, ContextMenuItemTagPaste, contextMenuItemTagPaste());
#if PLATFORM(GTK)
    ContextMenuItem PasteAsPlainTextItem(ActionType, ContextMenuItemTagPasteAsPlainText, contextMenuItemTagPasteAsPlainText());
    ContextMenuItem DeleteItem(ActionType, ContextMenuItemTagDelete, contextMenuItemTagDelete());
    ContextMenuItem SelectAllItem(ActionType, ContextMenuItemTagSelectAll, contextMenuItemTagSelectAll());
    ContextMenuItem InsertEmojiItem(ActionType, ContextMenuItemTagInsertEmoji, contextMenuItemTagInsertEmoji());
#endif

#if PLATFORM(GTK) || PLATFORM(WIN)
    ContextMenuItem ShareMenuItem;
#else
    ContextMenuItem ShareMenuItem(SubmenuType, ContextMenuItemTagShareMenu, emptyString());
#endif

    Node* node = m_context.hitTestResult().innerNonSharedNode();
    if (!node)
        return;
#if PLATFORM(GTK)
    if (!m_context.hitTestResult().isContentEditable() && is<HTMLFormControlElement>(*node))
        return;
#endif
    Frame* frame = node->document().frame();
    if (!frame)
        return;

#if ENABLE(SERVICE_CONTROLS)
    // The default image control menu gets populated solely by the platform.
    if (m_context.controlledImage())
        return;
#endif

    if (!m_context.hitTestResult().isContentEditable()) {
        String selectedString = m_context.hitTestResult().selectedText();
        m_context.setSelectedText(selectedString);

        FrameLoader& loader = frame->loader();
        URL linkURL = m_context.hitTestResult().absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader.client().canHandleRequest(ResourceRequest(linkURL))) {
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
            if (imageURL.isLocalFile() || m_context.hitTestResult().image())
                appendItem(CopyImageItem, m_contextMenu.get());
#if PLATFORM(GTK)
            appendItem(CopyImageUrlItem, m_contextMenu.get());
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
            appendItem(ToggleVideoFullscreen, m_contextMenu.get());
#else
            appendItem(EnterVideoFullscreen, m_contextMenu.get());
#endif
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
            appendItem(ToggleVideoEnhancedFullscreen, m_contextMenu.get());
#endif
            if (m_context.hitTestResult().isDownloadableMedia() && loader.client().canHandleRequest(ResourceRequest(mediaURL))) {
                appendItem(*separatorItem(), m_contextMenu.get());
                appendItem(CopyMediaLinkItem, m_contextMenu.get());
                appendItem(OpenMediaInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadMediaItem, m_contextMenu.get());
            }
        }

        if (imageURL.isEmpty() && linkURL.isEmpty() && mediaURL.isEmpty()) {
            if (m_context.hitTestResult().isSelected()) {
                if (!selectedString.isEmpty()) {
#if PLATFORM(COCOA)
                    ContextMenuItem LookUpInDictionaryItem(ActionType, ContextMenuItemTagLookUpInDictionary, contextMenuItemTagLookUpInDictionary(selectedString));

                    appendItem(LookUpInDictionaryItem, m_contextMenu.get());
#endif

#if !PLATFORM(GTK)
                    appendItem(SearchWebItem, m_contextMenu.get());
                    appendItem(*separatorItem(), m_contextMenu.get());
#endif
                }

                appendItem(CopyItem, m_contextMenu.get());
#if PLATFORM(COCOA)
                appendItem(*separatorItem(), m_contextMenu.get());

                appendItem(ShareMenuItem, m_contextMenu.get());
                appendItem(*separatorItem(), m_contextMenu.get());

                ContextMenuItem SpeechMenuItem(SubmenuType, ContextMenuItemTagSpeechMenu, contextMenuItemTagSpeechMenu());
                createAndAppendSpeechSubMenu(SpeechMenuItem);
                appendItem(SpeechMenuItem, m_contextMenu.get());
#endif                
            } else {
                if (!(frame->page() && (frame->page()->inspectorController().inspectionLevel() > 0 || frame->page()->inspectorController().hasRemoteFrontend()))) {

                // In GTK+ unavailable items are not hidden but insensitive.
#if PLATFORM(GTK)
                appendItem(BackItem, m_contextMenu.get());
                appendItem(ForwardItem, m_contextMenu.get());
                appendItem(StopItem, m_contextMenu.get());
                appendItem(ReloadItem, m_contextMenu.get());
#else
                if (frame->page() && frame->page()->backForward().canGoBackOrForward(-1))
                    appendItem(BackItem, m_contextMenu.get());

                if (frame->page() && frame->page()->backForward().canGoBackOrForward(1))
                    appendItem(ForwardItem, m_contextMenu.get());

                // use isLoadingInAPISense rather than isLoading because Stop/Reload are
                // intended to match WebKit's API, not WebCore's internal notion of loading status
                if (loader.documentLoader()->isLoadingInAPISense())
                    appendItem(StopItem, m_contextMenu.get());
                else
                    appendItem(ReloadItem, m_contextMenu.get());
#endif
                }

                if (frame->page() && !frame->isMainFrame())
                    appendItem(OpenFrameItem, m_contextMenu.get());

                if (!ShareMenuItem.isNull()) {
                    appendItem(*separatorItem(), m_contextMenu.get());
                    appendItem(ShareMenuItem, m_contextMenu.get());
                }
            }
        } else if (!ShareMenuItem.isNull()) {
            appendItem(*separatorItem(), m_contextMenu.get());
            appendItem(ShareMenuItem, m_contextMenu.get());
        }
    } else { // Make an editing context menu
        bool inPasswordField = frame->selection().selection().isInPasswordField();
        if (!inPasswordField) {
            bool haveContextMenuItemsForMisspellingOrGrammer = false;
            bool spellCheckingEnabled = frame->editor().isSpellCheckingEnabledFor(node);
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
                                ContextMenuItem item(ActionType, ContextMenuItemTagSpellingGuess, guess);
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
                        ContextMenuItem item(ActionType, ContextMenuItemTagChangeBack, contextMenuItemTagChangeBack(replacedString));
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
                        ContextMenuItem item(ActionType, ContextMenuItemTagDictationAlternative, alternative);
                        appendItem(item, m_contextMenu.get());
                    }
                    appendItem(*separatorItem(), m_contextMenu.get());
                }
            }
        }

        FrameLoader& loader = frame->loader();
        URL linkURL = m_context.hitTestResult().absoluteLinkURL();
        if (!linkURL.isEmpty()) {
            if (loader.client().canHandleRequest(ResourceRequest(linkURL))) {
                appendItem(OpenLinkItem, m_contextMenu.get());
                appendItem(OpenLinkInNewWindowItem, m_contextMenu.get());
                appendItem(DownloadFileItem, m_contextMenu.get());
            }
            appendItem(CopyLinkItem, m_contextMenu.get());
            appendItem(*separatorItem(), m_contextMenu.get());
        }

        String selectedText = m_context.hitTestResult().selectedText();
        if (m_context.hitTestResult().isSelected() && !inPasswordField && !selectedText.isEmpty()) {
#if PLATFORM(COCOA)
            ContextMenuItem LookUpInDictionaryItem(ActionType, ContextMenuItemTagLookUpInDictionary, contextMenuItemTagLookUpInDictionary(selectedText));

            appendItem(LookUpInDictionaryItem, m_contextMenu.get());
#endif

#if !PLATFORM(GTK)
            appendItem(SearchWebItem, m_contextMenu.get());
            appendItem(*separatorItem(), m_contextMenu.get());
#endif
        }

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
#if !PLATFORM(GTK)
            appendItem(*separatorItem(), m_contextMenu.get());
            ContextMenuItem SpellingAndGrammarMenuItem(SubmenuType, ContextMenuItemTagSpellingMenu, 
                contextMenuItemTagSpellingMenu());
            createAndAppendSpellingAndGrammarSubMenu(SpellingAndGrammarMenuItem);
            appendItem(SpellingAndGrammarMenuItem, m_contextMenu.get());
#endif
#if PLATFORM(COCOA)
            ContextMenuItem substitutionsMenuItem(SubmenuType, ContextMenuItemTagSubstitutionsMenu, 
                contextMenuItemTagSubstitutionsMenu());
            createAndAppendSubstitutionsSubMenu(substitutionsMenuItem);
            appendItem(substitutionsMenuItem, m_contextMenu.get());
            ContextMenuItem transformationsMenuItem(SubmenuType, ContextMenuItemTagTransformationsMenu, 
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
                ContextMenuItem FontMenuItem(SubmenuType, ContextMenuItemTagFontMenu, 
                    contextMenuItemTagFontMenu());
                createAndAppendFontSubMenu(FontMenuItem);
                appendItem(FontMenuItem, m_contextMenu.get());
            }
#if PLATFORM(COCOA)
            ContextMenuItem SpeechMenuItem(SubmenuType, ContextMenuItemTagSpeechMenu, contextMenuItemTagSpeechMenu());
            createAndAppendSpeechSubMenu(SpeechMenuItem);
            appendItem(SpeechMenuItem, m_contextMenu.get());
#endif
#if PLATFORM(GTK)
            EditorClient* client = frame->editor().client();
            if (client && client->shouldShowUnicodeMenu()) {
                ContextMenuItem UnicodeMenuItem(SubmenuType, ContextMenuItemTagUnicode, contextMenuItemTagUnicode());
                createAndAppendUnicodeSubMenu(UnicodeMenuItem);
                appendItem(*separatorItem(), m_contextMenu.get());
                appendItem(UnicodeMenuItem, m_contextMenu.get());
            }
#else
            ContextMenuItem WritingDirectionMenuItem(SubmenuType, ContextMenuItemTagWritingDirectionMenu, 
                contextMenuItemTagWritingDirectionMenu());
            createAndAppendWritingDirectionSubMenu(WritingDirectionMenuItem);
            appendItem(WritingDirectionMenuItem, m_contextMenu.get());
            if (Page* page = frame->page()) {
                bool includeTextDirectionSubmenu = page->settings().textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAlwaysIncluded
                    || (page->settings().textDirectionSubmenuInclusionBehavior() == TextDirectionSubmenuAutomaticallyIncluded && frame->editor().hasBidiSelection());
                if (includeTextDirectionSubmenu) {
                    ContextMenuItem TextDirectionMenuItem(SubmenuType, ContextMenuItemTagTextDirectionMenu, contextMenuItemTagTextDirectionMenu());
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
}

void ContextMenuController::addInspectElementItem()
{
    Node* node = m_context.hitTestResult().innerNonSharedNode();
    if (!node)
        return;

    Frame* frame = node->document().frame();
    if (!frame)
        return;

    Page* page = frame->page();
    if (!page)
        return;

    ContextMenuItem InspectElementItem(ActionType, ContextMenuItemTagInspectElement, contextMenuItemTagInspectElement());
    if (m_contextMenu && !m_contextMenu->items().isEmpty())
        appendItem(*separatorItem(), m_contextMenu.get());
    appendItem(InspectElementItem, m_contextMenu.get());
}

void ContextMenuController::checkOrEnableIfNeeded(ContextMenuItem& item) const
{
    if (item.type() == SeparatorType)
        return;
    
    Frame* frame = m_context.hitTestResult().innerNonSharedNode()->document().frame();
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
            String direction = item.action() == ContextMenuItemTagLeftToRight ? "ltr" : "rtl";
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyDirection, direction) != TriState::False;
            shouldEnable = true;
            break;
        }
        case ContextMenuItemTagTextDirectionDefault: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionNatural");
            shouldCheck = command.state() == TriState::True;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionLeftToRight: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionLeftToRight");
            shouldCheck = command.state() == TriState::True;
            shouldEnable = command.isEnabled();
            break;
        }
        case ContextMenuItemTagTextDirectionRightToLeft: {
            Editor::Command command = frame->editor().command("MakeTextWritingDirectionRightToLeft");
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
            shouldEnable = frame->editor().canDHTMLPaste() || frame->editor().canPaste();
            break;
#if PLATFORM(GTK)
        case ContextMenuItemTagPasteAsPlainText:
            shouldEnable = frame->editor().canDHTMLPaste() || frame->editor().canPaste();
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
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyWebkitTextDecorationsInEffect, "underline") != TriState::False;
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
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyFontStyle, "italic") != TriState::False;
            shouldEnable = frame->editor().canEditRichly();
            break;
        }
        case ContextMenuItemTagBold: {
            shouldCheck = frame->editor().selectionHasStyle(CSSPropertyFontWeight, "bold") != TriState::False;
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
            shouldEnable = m_client.isSpeaking();
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
#if PLATFORM(GTK)
        case ContextMenuItemTagCopyImageUrlToClipboard:
#endif
            break;
        case ContextMenuItemTagDownloadImageToDisk:
#if PLATFORM(MAC)
            if (m_context.hitTestResult().absoluteImageURL().protocolIs("file"))
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
            if (m_context.hitTestResult().absoluteImageURL().protocolIs("file"))
                shouldEnable = false;
            break;
        case ContextMenuItemTagCopyMediaLinkToClipboard:
            if (m_context.hitTestResult().mediaIsVideo())
                item.setTitle(contextMenuItemTagCopyVideoLinkToClipboard());
            else
                item.setTitle(contextMenuItemTagCopyAudioLinkToClipboard());
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
        case ContextMenuItemTagOpenFrameInNewWindow:
        case ContextMenuItemTagSpellingGuess:
        case ContextMenuItemTagOther:
        case ContextMenuItemTagSearchInSpotlight:
        case ContextMenuItemTagSearchWeb:
        case ContextMenuItemTagOpenWithDefaultApplication:
        case ContextMenuItemPDFActualSize:
        case ContextMenuItemPDFZoomIn:
        case ContextMenuItemPDFZoomOut:
        case ContextMenuItemPDFAutoSize:
        case ContextMenuItemPDFSinglePage:
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
    }

    item.setChecked(shouldCheck);
    item.setEnabled(shouldEnable);
}

#if USE(ACCESSIBILITY_CONTEXT_MENUS)

void ContextMenuController::showContextMenuAt(Frame& frame, const IntPoint& clickPoint)
{
    clearContextMenu();
    
    // Simulate a click in the middle of the accessibility object.
    PlatformMouseEvent mouseEvent(clickPoint, clickPoint, RightButton, PlatformEvent::MousePressed, 1, false, false, false, false, WallTime::now(), ForceAtClick, NoTap);
    frame.eventHandler().handleMousePressEvent(mouseEvent);
    bool handled = frame.eventHandler().sendContextMenuEvent(mouseEvent);
    if (handled)
        m_client.showContextMenu();
}

#endif

#if ENABLE(SERVICE_CONTROLS)

void ContextMenuController::showImageControlsMenu(Event& event)
{
    clearContextMenu();
    handleContextMenuEvent(event);
    m_client.showContextMenu();
}

#endif

} // namespace WebCore

#endif // ENABLE(CONTEXT_MENUS)
