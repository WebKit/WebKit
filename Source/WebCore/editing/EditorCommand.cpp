/*
 * Copyright (C) 2006-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Google Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Igalia S.L.
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
#include "Editor.h"

#include "CSSComputedStyleDeclaration.h"
#include "CSSValueList.h"
#include "Chrome.h"
#include "CreateLinkCommand.h"
#include "DocumentFragment.h"
#include "Editing.h"
#include "EditorClient.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "FormatBlockCommand.h"
#include "FrameLoader.h"
#include "HTMLFontElement.h"
#include "HTMLHRElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "InsertNestedListCommand.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MutableStyleProperties.h"
#include "Page.h"
#include "PagePasteboardContext.h"
#include "Pasteboard.h"
#include "Range.h"
#include "RenderBox.h"
#include "ReplaceSelectionCommand.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "SystemSoundManager.h"
#include "TypingCommand.h"
#include "UnlinkCommand.h"
#include "UserGestureIndicator.h"
#include "UserTypingGestureIndicator.h"
#include "markup.h"
#include <pal/text/KillRing.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

using namespace HTMLNames;

class EditorInternalCommand {
public:
    bool (*execute)(LocalFrame&, Event*, EditorCommandSource, const String&);
    bool (*isSupportedFromDOM)(LocalFrame*);
    bool (*isEnabled)(LocalFrame&, Event*, EditorCommandSource);
    TriState (*state)(LocalFrame&, Event*);
    String (*value)(LocalFrame&, Event*);
    bool isTextInsertion;
    bool (*allowExecutionWhenDisabled)(LocalFrame&, EditorCommandSource);
};

typedef HashMap<String, const EditorInternalCommand*, ASCIICaseInsensitiveHash> CommandMap;

static const bool notTextInsertion = false;
static const bool isTextInsertion = true;

// Related to Editor::selectionForCommand.
// Certain operations continue to use the target control's selection even if the event handler
// already moved the selection outside of the text control.
static LocalFrame* targetFrame(LocalFrame& frame, Event* event)
{
    if (!event)
        return &frame;
    if (!is<Node>(event->target()))
        return &frame;
    return downcast<Node>(*event->target()).document().frame();
}

static bool applyCommandToFrame(LocalFrame& frame, EditorCommandSource source, EditAction action, Ref<EditingStyle>&& style)
{
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        // Use InvertColor for testing purposes. foreColor and backColor are never triggered with EditorCommandSource::MenuOrKeyBinding outside DRT/WTR.
        frame.editor().applyStyleToSelection(WTFMove(style), action, Editor::ColorFilterMode::InvertColor);
        return true;
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        frame.editor().applyStyle(WTFMove(style), action, Editor::ColorFilterMode::UseOriginalColor);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool isStylePresent(Editor& editor, CSSPropertyID propertyID, ASCIILiteral onValue)
{
    // Style is considered present when
    // Mac: present at the beginning of selection
    // Windows: present throughout the selection
    if (editor.behavior().shouldToggleStyleBasedOnStartOfSelection())
        return editor.selectionStartHasStyle(propertyID, onValue);
    return editor.selectionHasStyle(propertyID, onValue) == TriState::True;
}

static bool executeApplyStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, propertyValue));
}

static bool executeApplyStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, CSSValueID propertyValue)
{
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, propertyValue));
}

static bool executeToggleStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, ASCIILiteral offValue, ASCIILiteral onValue)
{
    bool styleIsPresent = isStylePresent(frame.editor(), propertyID, onValue);
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, styleIsPresent ? offValue : onValue));
}

static bool executeApplyParagraphStyle(LocalFrame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        frame.editor().applyParagraphStyleToSelection(style.ptr(), action);
        return true;
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        frame.editor().applyParagraphStyle(style.ptr());
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertFragment(LocalFrame& frame, Ref<DocumentFragment>&& fragment)
{
    ASSERT(frame.document());
    ReplaceSelectionCommand::create(*frame.document(), WTFMove(fragment), ReplaceSelectionCommand::PreventNesting, EditAction::Insert)->apply();
    return true;
}

static bool executeInsertNode(LocalFrame& frame, Ref<Node>&& content)
{
    auto fragment = DocumentFragment::create(*frame.document());
    if (fragment->appendChild(content).hasException())
        return false;
    return executeInsertFragment(frame, WTFMove(fragment));
}

static bool expandSelectionToGranularity(LocalFrame& frame, TextGranularity granularity)
{
    VisibleSelection selection = frame.selection().selection();
    auto oldRange = selection.toNormalizedRange();
    selection.expandUsingGranularity(granularity);
    auto newRange = selection.toNormalizedRange();
    if (!newRange || newRange->collapsed())
        return false;
    auto affinity = selection.affinity();
    if (!frame.editor().client()->shouldChangeSelectedRange(*oldRange, *newRange, affinity, false))
        return false;
    frame.selection().setSelectedRange(*newRange, affinity, FrameSelection::ShouldCloseTyping::Yes);
    // FIXME: Why do we ignore the return value from setSelectedRange here?
    return true;
}

static TriState stateStyle(LocalFrame& frame, CSSPropertyID propertyID, ASCIILiteral desiredValue)
{
    if (frame.editor().behavior().shouldToggleStyleBasedOnStartOfSelection())
        return frame.editor().selectionStartHasStyle(propertyID, desiredValue) ? TriState::True : TriState::False;
    return frame.editor().selectionHasStyle(propertyID, desiredValue);
}

static String valueStyle(LocalFrame& frame, CSSPropertyID propertyID)
{
    // FIXME: Rather than retrieving the style at the start of the current selection,
    // we should retrieve the style present throughout the selection for non-Mac platforms.
    return frame.editor().selectionStartCSSPropertyValue(propertyID);
}

static TriState stateTextWritingDirection(LocalFrame& frame, WritingDirection direction)
{
    bool hasNestedOrMultipleEmbeddings;
    WritingDirection selectionDirection = EditingStyle::textDirectionForSelection(frame.selection().selection(),
        frame.selection().typingStyle(), hasNestedOrMultipleEmbeddings);
    // FXIME: We should be returning TriState::Indeterminate when selectionDirection == direction && hasNestedOrMultipleEmbeddings
    return (selectionDirection == direction && !hasNestedOrMultipleEmbeddings) ? TriState::True : TriState::False;
}

static unsigned verticalScrollDistance(LocalFrame& frame)
{
    Element* focusedElement = frame.document()->focusedElement();
    if (!focusedElement)
        return 0;
    auto* renderer = focusedElement->renderer();
    if (!is<RenderBox>(renderer))
        return 0;
    const RenderStyle& style = renderer->style();
    if (!(style.overflowY() == Overflow::Scroll || style.overflowY() == Overflow::Auto || focusedElement->hasEditableStyle()))
        return 0;
    int height = std::min<int>(downcast<RenderBox>(*renderer).clientHeight(), frame.view()->visibleHeight());
    return static_cast<unsigned>(Scrollbar::pageStep(height));
}

// Execute command functions

static bool executeBackColor(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetBackgroundColor, CSSPropertyBackgroundColor, value);
}

static bool executeCopy(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    frame.editor().copy(source == EditorCommandSource::MenuOrKeyBinding ? Editor::FromMenuOrKeyBinding::Yes : Editor::FromMenuOrKeyBinding::No);
    return true;
}

static bool executeCopyFont(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    frame.editor().copyFont(source == EditorCommandSource::MenuOrKeyBinding ? Editor::FromMenuOrKeyBinding::Yes : Editor::FromMenuOrKeyBinding::No);
    return true;
}

static bool executeCreateLink(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box to let the user enter a URL.
    if (value.isEmpty())
        return false;
    ASSERT(frame.document());
    CreateLinkCommand::create(*frame.document(), value)->apply();
    return true;
}

static bool executeCut(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().cut(Editor::FromMenuOrKeyBinding::Yes);
    } else
        frame.editor().cut();
    return true;
}

static bool executeClearText(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().clearText();
    return true;
}

static bool executeDefaultParagraphSeparator(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    if (equalLettersIgnoringASCIICase(value, "div"_s))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparator::div);
    else if (equalLettersIgnoringASCIICase(value, "p"_s))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparator::p);

    return true;
}

static bool executeDelete(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding: {
        // Doesn't modify the text if the current selection isn't a range.
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().performDelete();
        return true;
    }
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        // If the current selection is a caret, delete the preceding character. IE performs forwardDelete, but we currently side with Firefox.
        // Doesn't scroll to make the selection visible, or modify the kill ring (this time, siding with IE, not Firefox).
        TypingCommand::deleteKeyPressed(*frame.document(), frame.editor().shouldSmartDelete() ? TypingCommand::Option::SmartDelete : OptionSet<TypingCommand::Option> { });
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeDeleteBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteBackwardByDecomposingPreviousCharacter(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    LOG_ERROR("DeleteBackwardByDecomposingPreviousCharacter is not implemented, doing DeleteBackward instead");
    frame.editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Forward, TextGranularity::CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteToBeginningOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::LineBoundary, true, false);
    return true;
}

static bool executeDeleteToBeginningOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like DeleteToEndOfParagraph).
    frame.editor().deleteWithDirection(SelectionDirection::Forward, TextGranularity::LineBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph.
    frame.editor().deleteWithDirection(SelectionDirection::Forward, TextGranularity::ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    auto& editor = frame.editor();
    auto& selection = frame.selection();
    auto markRange = editor.mark().toNormalizedRange();
    auto selectionRange = selection.selection().toNormalizedRange();
    if (markRange && selectionRange) {
        if (!selection.setSelectedRange(unionRange(*markRange, *selectionRange), Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes))
            return false;
    }
    editor.performDelete();
    editor.setMark(selection.selection());
    return true;
}

static bool executeDeleteWordBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Backward, TextGranularity::WordGranularity, true, false);
    return true;
}

static bool executeDeleteWordForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(SelectionDirection::Forward, TextGranularity::WordGranularity, true, false);
    return true;
}

static bool executeFindString(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    return frame.editor().findString(value, { CaseInsensitive, WrapAround, DoNotTraverseFlatTree });
}

static bool executeFontName(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetFont, CSSPropertyFontFamily, value);
}

static bool executeFontSize(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    CSSValueID size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return false;
    return executeApplyStyle(frame, source, EditAction::ChangeAttributes, CSSPropertyFontSize, size);
}

static bool executeFontSizeDelta(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::ChangeAttributes, CSSPropertyWebkitFontSizeDelta, value);
}

static bool executeForeColor(LocalFrame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetColor, CSSPropertyColor, value);
}

static bool executeFormatBlock(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    String lowercaseValue = value.convertToASCIILowercase();
    AtomString tagName;
    if (lowercaseValue[0] == '<' && lowercaseValue[lowercaseValue.length() - 1] == '>')
        tagName = StringView(lowercaseValue).substring(1, lowercaseValue.length() - 2).toAtomString();
    else
        tagName = AtomString { WTFMove(lowercaseValue) };

    auto qualifiedTagName = Document::parseQualifiedName(xhtmlNamespaceURI, tagName);
    if (qualifiedTagName.hasException())
        return false;

    ASSERT(frame.document());
    auto command = FormatBlockCommand::create(*frame.document(), qualifiedTagName.releaseReturnValue());
    command->apply();
    return command->didApply();
}

static bool executeForwardDelete(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        frame.editor().deleteWithDirection(SelectionDirection::Forward, TextGranularity::CharacterGranularity, false, true);
        return true;
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // ForwardDelete is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with Delete.
        TypingCommand::forwardDeleteKeyPressed(*frame.document());
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeIgnoreSpelling(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().ignoreSpelling();
    return true;
}

static bool executeIndent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Indent)->apply();
    return true;
}

static bool executeInsertBacktab(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t"_s, event, TextEventInputBackTab);
}

static bool executeInsertHorizontalRule(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    Ref<HTMLHRElement> rule = HTMLHRElement::create(*frame.document());
    if (!value.isEmpty())
        rule->setIdAttribute(AtomString { value });
    return executeInsertNode(frame, WTFMove(rule));
}

static bool executeInsertHTML(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    return executeInsertFragment(frame, createFragmentFromMarkup(*frame.document(), value, emptyString()));
}

static bool executeInsertImage(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box and let the user choose a local image.
    Ref<HTMLImageElement> image = HTMLImageElement::create(*frame.document());
    if (!value.isEmpty())
        image->setSrc(AtomString { value });
    return executeInsertNode(frame, WTFMove(image));
}

static bool executeInsertLineBreak(LocalFrame& frame, Event* event, EditorCommandSource source, const String&)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\n"_s, event, TextEventInputLineBreak);
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // InsertLineBreak is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with other commands.
        TypingCommand::insertLineBreak(*frame.document(), { });
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertNewline(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    LocalFrame* targetFrame = WebCore::targetFrame(frame, event);
    return targetFrame->eventHandler().handleTextInputEvent("\n"_s, event, targetFrame->editor().canEditRichly() ? TextEventInputKeyboard : TextEventInputLineBreak);
}

static bool executeInsertNewlineInQuotedContent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparatorInQuotedContent(*frame.document());
    return true;
}

static bool executeInsertOrderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertListCommand::create(*frame.document(), InsertListCommand::Type::OrderedList)->apply();
    return true;
}

static bool executeInsertParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparator(*frame.document(), { });
    return true;
}

static bool executeInsertTab(LocalFrame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t"_s, event);
}

static bool executeInsertText(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    TypingCommand::insertText(*frame.document(), value, { });
    return true;
}

static bool executeInsertUnorderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertListCommand::create(*frame.document(), InsertListCommand::Type::UnorderedList)->apply();
    return true;
}

static bool executeInsertNestedUnorderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertNestedListCommand::insertUnorderedList(*frame.document());
    return true;
}

static bool executeInsertNestedOrderedList(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertNestedListCommand::insertOrderedList(*frame.document());
    return true;
}

static bool executeJustifyCenter(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::Center, CSSPropertyTextAlign, "center"_s);
}

static bool executeJustifyFull(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::Justify, CSSPropertyTextAlign, "justify"_s);
}

static bool executeJustifyLeft(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::AlignLeft, CSSPropertyTextAlign, "left"_s);
}

static bool executeJustifyRight(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::AlignRight, CSSPropertyTextAlign, "right"_s);
}

static bool executeMakeTextWritingDirectionLeftToRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueLtr);
    frame.editor().applyStyle(style.ptr(), EditAction::SetInlineWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionNatural(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
    frame.editor().applyStyle(style.ptr(), EditAction::SetInlineWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionRightToLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    auto style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueRtl);
    frame.editor().applyStyle(style.ptr(), EditAction::SetInlineWritingDirection);
    return true;
}

static bool executeMoveBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::LineGranularity, UserTriggered::Yes);
}

static bool executeMoveDownAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::LineGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Left, TextGranularity::CharacterGranularity, UserTriggered::Yes);
}

static bool executeMoveLeftAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Left, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMovePageDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::Alteration::Move, distance, FrameSelection::VerticalDirection::Down,
        UserTriggered::Yes, FrameSelection::CursorAlignOnScroll::Always);
}

static bool executeMovePageDownAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::Alteration::Extend, distance, FrameSelection::VerticalDirection::Down,
        UserTriggered::Yes, FrameSelection::CursorAlignOnScroll::Always);
}

static bool executeMovePageUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::Alteration::Move, distance, FrameSelection::VerticalDirection::Up,
        UserTriggered::Yes, FrameSelection::CursorAlignOnScroll::Always);
}

static bool executeMovePageUpAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::Alteration::Extend, distance, FrameSelection::VerticalDirection::Up,
        UserTriggered::Yes, FrameSelection::CursorAlignOnScroll::Always);
}

static bool executeMoveRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Right, TextGranularity::CharacterGranularity, UserTriggered::Yes);
}

static bool executeMoveRightAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Right, TextGranularity::CharacterGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::DocumentBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfDocumentAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::DocumentBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::ParagraphBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfParagraphAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::ParagraphBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::SentenceBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToBeginningOfSentenceAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::SentenceBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::DocumentBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfDocumentAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::DocumentBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::SentenceBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfSentenceAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::SentenceBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::ParagraphBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToEndOfParagraphAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::ParagraphBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveParagraphBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::ParagraphGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveParagraphForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::ParagraphGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::LineGranularity, UserTriggered::Yes);
}

static bool executeMoveUpAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::LineGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Backward, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordBackwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Backward, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Forward, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordForwardAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Forward, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordLeft(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Left, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordLeftAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Left, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordRight(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Right, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveWordRightAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Right, TextGranularity::WordGranularity, UserTriggered::Yes);
    return true;
}

static bool executeMoveToLeftEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Left, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToLeftEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Left, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToRightEndOfLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Move, SelectionDirection::Right, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeMoveToRightEndOfLineAndModifySelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::Alteration::Extend, SelectionDirection::Right, TextGranularity::LineBoundary, UserTriggered::Yes);
    return true;
}

static bool executeOutdent(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Outdent)->apply();
    return true;
}

static bool executeToggleOverwrite(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().toggleOverwriteModeEnabled();
    return true;
}

static bool executePaste(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().paste(Editor::FromMenuOrKeyBinding::Yes);
        return true;
    }

    if (!frame.requestDOMPasteAccess())
        return false;

    frame.editor().paste();
    return true;
}

static bool executePasteFont(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteFont(Editor::FromMenuOrKeyBinding::Yes);
        return true;
    }

    if (!frame.requestDOMPasteAccess(DOMPasteAccessCategory::Fonts))
        return false;

    frame.editor().pasteFont();
    return true;
}

#if PLATFORM(GTK)

static bool executePasteGlobalSelection(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    // FIXME: This check should be in an enable function, not here.
    if (!frame.editor().client()->supportsGlobalSelection())
        return false;

    ASSERT_UNUSED(source, source == EditorCommandSource::MenuOrKeyBinding);
    UserTypingGestureIndicator typingGestureIndicator(frame);
    frame.editor().paste(*Pasteboard::createForGlobalSelection(PagePasteboardContext::create(frame.pageID())));
    return true;
}

#endif

static bool executePasteAndMatchStyle(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsPlainText(Editor::FromMenuOrKeyBinding::Yes);
        return true;
    }

    if (!frame.requestDOMPasteAccess())
        return false;

    frame.editor().pasteAsPlainText();
    return true;
}

static bool executePasteAsPlainText(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsPlainText(Editor::FromMenuOrKeyBinding::Yes);
        return true;
    }

    if (!frame.requestDOMPasteAccess())
        return false;

    frame.editor().pasteAsPlainText();
    return true;
}

static bool executePasteAsQuotation(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == EditorCommandSource::MenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsQuotation(Editor::FromMenuOrKeyBinding::Yes);
        return true;
    }

    if (!frame.requestDOMPasteAccess())
        return false;

    frame.editor().pasteAsQuotation();
    return true;
}

static bool executePrint(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    Page* page = frame.page();
    if (!page)
        return false;
    return page->chrome().print(frame);
}

static bool executeRedo(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().redo();
    return true;
}

static bool executeRemoveFormat(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().removeFormattingAndStyle();
    return true;
}

static bool executeScrollPageBackward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    if (frame.eventHandler().shouldUseSmoothKeyboardScrollingForFocusedScrollableArea()) {
        auto isKeyRepeat = frame.eventHandler().isProcessingKeyRepeatForPotentialScroll();
        frame.eventHandler().setProcessingKeyRepeatForPotentialScroll(false);
        return frame.eventHandler().keyboardScrollRecursively(ScrollDirection::ScrollUp, ScrollGranularity::Page, nullptr, isKeyRepeat);
    }

    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionBackward, ScrollGranularity::Page);
}

static bool executeScrollPageForward(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    if (frame.eventHandler().shouldUseSmoothKeyboardScrollingForFocusedScrollableArea()) {
        auto isKeyRepeat = frame.eventHandler().isProcessingKeyRepeatForPotentialScroll();
        frame.eventHandler().setProcessingKeyRepeatForPotentialScroll(false);
        return frame.eventHandler().keyboardScrollRecursively(ScrollDirection::ScrollDown, ScrollGranularity::Page, nullptr, isKeyRepeat);
    }

    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionForward, ScrollGranularity::Page);
}

static bool executeScrollLineUp(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().scrollRecursively(ScrollUp, ScrollGranularity::Line);
}

static bool executeScrollLineDown(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().scrollRecursively(ScrollDown, ScrollGranularity::Line);
}

static bool executeScrollToBeginningOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionBackward, ScrollGranularity::Document);
}

static bool executeScrollToEndOfDocument(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionForward, ScrollGranularity::Document);
}

static bool executeSelectAll(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().selectAll();
    return true;
}

static bool executeSelectLine(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, TextGranularity::LineGranularity);
}

static bool executeSelectParagraph(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, TextGranularity::ParagraphGranularity);
}

static bool executeSelectSentence(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, TextGranularity::SentenceGranularity);
}

static bool executeSelectToMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    auto& editor = frame.editor();
    auto& selection = frame.selection();
    auto markRange = editor.mark().toNormalizedRange();
    auto selectionRange = selection.selection().toNormalizedRange();
    if (!markRange || !selectionRange) {
        SystemSoundManager::singleton().systemBeep();
        return false;
    }
    selection.setSelectedRange(unionRange(*markRange, *selectionRange), Affinity::Downstream, FrameSelection::ShouldCloseTyping::Yes);
    // FIXME: Why do we ignore the return value from setSelectedRange here?
    return true;
}

static bool executeSelectWord(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, TextGranularity::WordGranularity);
}

static bool executeSetMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().setMark(frame.selection().selection());
    return true;
}

static TextDecorationChange textDecorationChangeForToggling(Editor& editor, CSSPropertyID propertyID, ASCIILiteral onValue)
{
    return isStylePresent(editor, propertyID, onValue) ? TextDecorationChange::Remove : TextDecorationChange::Add;
}

static bool executeStrikethrough(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    Ref<EditingStyle> style = EditingStyle::create();
    style->setStrikeThroughChange(textDecorationChangeForToggling(frame.editor(), CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s));
    return applyCommandToFrame(frame, source, EditAction::StrikeThrough, WTFMove(style));
}

static bool executeStyleWithCSS(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(!equalLettersIgnoringASCIICase(value, "false"_s));
    return true;
}

static bool executeUseCSS(LocalFrame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(equalLettersIgnoringASCIICase(value, "false"_s));
    return true;
}

static bool executeSubscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Subscript, CSSPropertyVerticalAlign, "baseline"_s, "sub"_s);
}

static bool executeSuperscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Superscript, CSSPropertyVerticalAlign, "baseline"_s, "super"_s);
}

static bool executeSwapWithMark(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr protectedDocument { frame.document() };
    Ref protectedFrame { frame };
    const VisibleSelection& mark = frame.editor().mark();
    const VisibleSelection& selection = frame.selection().selection();
    if (mark.isNone() || selection.isNone()) {
        SystemSoundManager::singleton().systemBeep();
        return false;
    }
    frame.selection().setSelection(mark);
    frame.editor().setMark(selection);
    return true;
}

#if PLATFORM(COCOA)

static bool executeTakeFindStringFromSelection(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().takeFindStringFromSelection();
    return true;
}

#endif // PLATFORM(COCOA)

static bool executeToggleBold(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Bold, CSSPropertyFontWeight, "normal"_s, "bold"_s);
}

static bool executeToggleItalic(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Italics, CSSPropertyFontStyle, "normal"_s, "italic"_s);
}

static bool executeTranspose(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().transpose();
    return true;
}

static bool executeUnderline(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    Ref<EditingStyle> style = EditingStyle::create();
    TextDecorationChange change = textDecorationChangeForToggling(frame.editor(), CSSPropertyWebkitTextDecorationsInEffect, "underline"_s);
    style->setUnderlineChange(change);
    return applyCommandToFrame(frame, source, EditAction::Underline, WTFMove(style));
}

static bool executeUndo(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().undo();
    return true;
}

static bool executeUnlink(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    UnlinkCommand::create(*frame.document())->apply();
    return true;
}

static bool executeUnscript(LocalFrame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyStyle(frame, source, EditAction::Unscript, CSSPropertyVerticalAlign, "baseline"_s);
}

static bool executeUnselect(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().clear();
    return true;
}

static bool executeYank(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), false, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

static bool executeYankAndSelect(LocalFrame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), true, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

// Supported functions

static bool supported(LocalFrame*)
{
    return true;
}

static bool supportedFromMenuOrKeyBinding(LocalFrame*)
{
    return false;
}

static bool defaultValueForSupportedCopyCut(LocalFrame& frame)
{
    auto& settings = frame.settings();
    if (settings.javaScriptCanAccessClipboard())
        return true;
    
    switch (settings.clipboardAccessPolicy()) {
    case ClipboardAccessPolicy::Allow:
    case ClipboardAccessPolicy::RequiresUserGesture:
        return true;
    case ClipboardAccessPolicy::Deny:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool supportedCopyCut(LocalFrame* frame)
{
    if (!frame)
        return false;

    bool defaultValue = defaultValueForSupportedCopyCut(*frame);

    EditorClient* client = frame->editor().client();
    return client ? client->canCopyCut(frame, defaultValue) : defaultValue;
}

static bool defaultValueForSupportedPaste(LocalFrame& frame)
{
    auto& settings = frame.settings();
    if (settings.javaScriptCanAccessClipboard() && settings.domPasteAllowed())
        return true;

    return settings.domPasteAccessRequestsEnabled();
}

static bool supportedPaste(LocalFrame* frame)
{
    if (!frame)
        return false;

    bool defaultValue = defaultValueForSupportedPaste(*frame);

    EditorClient* client = frame->editor().client();
    return client ? client->canPaste(frame, defaultValue) : defaultValue;
}

// Enabled functions

static bool enabled(LocalFrame&, Event*, EditorCommandSource)
{
    return true;
}

static bool enabledVisibleSelection(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The term "visible" here includes a caret in editable text or a range in any text.
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return (selection.isCaret() && selection.isContentEditable()) || selection.isRange();
}

static bool caretBrowsingEnabled(LocalFrame& frame)
{
    return frame.settings().caretBrowsingEnabled();
}

static EditorCommandSource dummyEditorCommandSource = static_cast<EditorCommandSource>(0);

static bool enabledVisibleSelectionOrCaretBrowsing(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledVisibleSelection, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledVisibleSelection(frame, event, dummyEditorCommandSource);
}

static bool enabledVisibleSelectionAndMark(LocalFrame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return ((selection.isCaret() && selection.isContentEditable()) || selection.isRange())
        && frame.editor().mark().isCaretOrRange();
}

static bool enableCaretInEditableText(LocalFrame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return selection.isCaret() && selection.isContentEditable();
}

static bool allowCopyCutFromDOM(LocalFrame& frame)
{
    auto& settings = frame.settings();
    if (settings.javaScriptCanAccessClipboard())
        return true;
    
    switch (settings.clipboardAccessPolicy()) {
    case ClipboardAccessPolicy::Allow:
        return true;
    case ClipboardAccessPolicy::Deny:
        return false;
    case ClipboardAccessPolicy::RequiresUserGesture:
        return UserGestureIndicator::processingUserGesture();
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledCopy(LocalFrame& frame, EditorCommandSource source, Function<bool(const Editor&)>&& canCopy)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return frame.editor().canDHTMLCopy() || canCopy(frame.editor());
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        return allowCopyCutFromDOM(frame) && (frame.editor().canDHTMLCopy() || canCopy(frame.editor()));
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledCopy(LocalFrame& frame, Event*, EditorCommandSource source)
{
    return enabledCopy(frame, source, [](auto& editor) {
        return editor.canCopy();
    });
}

static bool enabledCopyFont(LocalFrame& frame, Event*, EditorCommandSource source)
{
    return enabledCopy(frame, source, [](auto& editor) {
        return editor.canCopyFont();
    });
}

static bool enabledCut(LocalFrame& frame, Event*, EditorCommandSource source)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return frame.editor().canDHTMLCut() || frame.editor().canCut();
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        return allowCopyCutFromDOM(frame) && (frame.editor().canDHTMLCut() || frame.editor().canCut());
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledClearText(LocalFrame& frame, Event*, EditorCommandSource)
{
    UNUSED_PARAM(frame);
    return false;
}

static bool enabledInEditableText(LocalFrame& frame, Event* event, EditorCommandSource)
{
    return frame.editor().selectionForCommand(event).rootEditableElement();
}

static bool enabledDelete(LocalFrame& frame, Event* event, EditorCommandSource source)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return frame.editor().canDelete();
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        // "Delete" from DOM is like delete/backspace keypress, affects selected range if non-empty,
        // otherwise removes a character
        return enabledInEditableText(frame, event, source);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledInEditableTextOrCaretBrowsing(LocalFrame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledInEditableText, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledInEditableText(frame, event, dummyEditorCommandSource);
}

static bool enabledInRichlyEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    const VisibleSelection& selection = frame.selection().selection();
    return selection.isCaretOrRange() && selection.isContentRichlyEditable() && selection.rootEditableElement();
}

static bool allowPasteFromDOM(LocalFrame& frame)
{
    auto& settings = frame.settings();
    if (settings.javaScriptCanAccessClipboard() && settings.domPasteAllowed())
        return true;

    return settings.domPasteAccessRequestsEnabled() && UserGestureIndicator::processingUserGesture();
}

static bool enabledPaste(LocalFrame& frame, Event*, EditorCommandSource source)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return frame.editor().canDHTMLPaste() || frame.editor().canPaste();
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        return allowPasteFromDOM(frame) && (frame.editor().canDHTMLPaste() || frame.editor().canPaste());
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledRangeInEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.selection().isRange() && frame.selection().selection().isContentEditable();
}

static bool enabledRangeInRichlyEditableText(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.selection().isRange() && frame.selection().selection().isContentRichlyEditable();
}

static bool enabledRedo(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canRedo();
}

#if PLATFORM(COCOA)

static bool enabledTakeFindStringFromSelection(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canCopyExcludingStandaloneImages();
}

#endif // PLATFORM(COCOA)

static bool enabledUndo(LocalFrame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canUndo();
}

// State functions

static TriState stateNone(LocalFrame&, Event*)
{
    return TriState::False;
}

static TriState stateBold(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontWeight, "bold"_s);
}

static TriState stateItalic(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontStyle, "italic"_s);
}

static TriState stateOrderedList(LocalFrame& frame, Event*)
{
    return frame.editor().selectionOrderedListState();
}

static TriState stateStrikethrough(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s);
}

static TriState stateStyleWithCSS(LocalFrame& frame, Event*)
{
    return frame.editor().shouldStyleWithCSS() ? TriState::True : TriState::False;
}

static TriState stateSubscript(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "sub"_s);
}

static TriState stateSuperscript(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "super"_s);
}

static TriState stateTextWritingDirectionLeftToRight(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, WritingDirection::LeftToRight);
}

static TriState stateTextWritingDirectionNatural(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, WritingDirection::Natural);
}

static TriState stateTextWritingDirectionRightToLeft(LocalFrame& frame, Event*)
{
    return stateTextWritingDirection(frame, WritingDirection::RightToLeft);
}

static TriState stateUnderline(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "underline"_s);
}

static TriState stateUnorderedList(LocalFrame& frame, Event*)
{
    return frame.editor().selectionUnorderedListState();
}

static TriState stateJustifyCenter(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "center"_s);
}

static TriState stateJustifyFull(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "justify"_s);
}

static TriState stateJustifyLeft(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "left"_s);
}

static TriState stateJustifyRight(LocalFrame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "right"_s);
}

// Value functions

static String valueNull(LocalFrame&, Event*)
{
    return String();
}

// The command has no value.
// https://w3c.github.io/editing/execCommand.html#querycommandvalue()
// > ... or has no value, return the empty string.
static String valueAsEmptyString(LocalFrame&, Event*)
{
    return emptyString();
}

static String valueBackColor(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyBackgroundColor);
}

static String valueDefaultParagraphSeparator(LocalFrame& frame, Event*)
{
    switch (frame.editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparator::div:
        return divTag->localName();
    case EditorParagraphSeparator::p:
        return pTag->localName();
    }

    ASSERT_NOT_REACHED();
    return String();
}

static String valueFontName(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontFamily);
}

static String valueFontSize(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontSize);
}

static String valueFontSizeDelta(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyWebkitFontSizeDelta);
}

static String valueForeColor(LocalFrame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyColor);
}

static String valueFormatBlock(LocalFrame& frame, Event*)
{
    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNoneOrOrphaned() || !selection.isContentEditable())
        return emptyString();
    auto* formatBlockElement = FormatBlockCommand::elementForFormatBlockCommand(selection.firstRange());
    if (!formatBlockElement)
        return emptyString();
    return formatBlockElement->localName();
}

// allowExecutionWhenDisabled functions

static bool allowExecutionWhenDisabled(LocalFrame&, EditorCommandSource)
{
    return true;
}

static bool doNotAllowExecutionWhenDisabled(LocalFrame&, EditorCommandSource)
{
    return false;
}

static bool allowExecutionWhenDisabledCopyCut(LocalFrame&, EditorCommandSource source)
{
    switch (source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return true;
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool allowExecutionWhenDisabledPaste(LocalFrame& frame, EditorCommandSource)
{
    auto* localFrame = dynamicDowncast<LocalFrame>(frame.mainFrame());
    if (!localFrame)
        return false;
    if (localFrame->loader().shouldSuppressTextInputFromEditing())
        return false;
    return true;
}

// Map of functions

struct CommandEntry {
    ASCIILiteral name;
    EditorInternalCommand command;
};

static const CommandMap& createCommandMap()
{
    static const CommandEntry commands[] = {
        { "AlignCenter"_s, { executeJustifyCenter, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignJustified"_s, { executeJustifyFull, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignLeft"_s, { executeJustifyLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignRight"_s, { executeJustifyRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "BackColor"_s, { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueBackColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Bold"_s, { executeToggleBold, supported, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ClearText"_s, { executeClearText, supported, enabledClearText, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "Copy"_s, { executeCopy, supportedCopyCut, enabledCopy, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledCopyCut } },
        { "CopyFont"_s, { executeCopyFont, supportedCopyCut, enabledCopyFont, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledCopyCut } },
        { "CreateLink"_s, { executeCreateLink, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Cut"_s, { executeCut, supportedCopyCut, enabledCut, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledCopyCut } },
        { "DefaultParagraphSeparator"_s, { executeDefaultParagraphSeparator, supported, enabled, stateNone, valueDefaultParagraphSeparator, notTextInsertion, doNotAllowExecutionWhenDisabled} },
        { "Delete"_s, { executeDelete, supported, enabledDelete, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackward"_s, { executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackwardByDecomposingPreviousCharacter"_s, { executeDeleteBackwardByDecomposingPreviousCharacter, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteForward"_s, { executeDeleteForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfLine"_s, { executeDeleteToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfParagraph"_s, { executeDeleteToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfLine"_s, { executeDeleteToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfParagraph"_s, { executeDeleteToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToMark"_s, { executeDeleteToMark, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordBackward"_s, { executeDeleteWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordForward"_s, { executeDeleteWordForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FindString"_s, { executeFindString, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontName"_s, { executeFontName, supported, enabledInRichlyEditableText, stateNone, valueFontName, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSize"_s, { executeFontSize, supported, enabledInRichlyEditableText, stateNone, valueFontSize, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSizeDelta"_s, { executeFontSizeDelta, supported, enabledInRichlyEditableText, stateNone, valueFontSizeDelta, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForeColor"_s, { executeForeColor, supported, enabledInRichlyEditableText, stateNone, valueForeColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FormatBlock"_s, { executeFormatBlock, supported, enabledInRichlyEditableText, stateNone, valueFormatBlock, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForwardDelete"_s, { executeForwardDelete, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "HiliteColor"_s, { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "IgnoreSpelling"_s, { executeIgnoreSpelling, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Indent"_s, { executeIndent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertBacktab"_s, { executeInsertBacktab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHTML"_s, { executeInsertHTML, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHorizontalRule"_s, { executeInsertHorizontalRule, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertImage"_s, { executeInsertImage, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertLineBreak"_s, { executeInsertLineBreak, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNewline"_s, { executeInsertNewline, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },    
        { "InsertNewlineInQuotedContent"_s, { executeInsertNewlineInQuotedContent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertOrderedList"_s, { executeInsertOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNestedOrderedList"_s, { executeInsertNestedOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertParagraph"_s, { executeInsertParagraph, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertTab"_s, { executeInsertTab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertText"_s, { executeInsertText, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertUnorderedList"_s, { executeInsertUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNestedUnorderedList"_s, { executeInsertNestedUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Italic"_s, { executeToggleItalic, supported, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyCenter"_s, { executeJustifyCenter, supported, enabledInRichlyEditableText, stateJustifyCenter, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyFull"_s, { executeJustifyFull, supported, enabledInRichlyEditableText, stateJustifyFull, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyLeft"_s, { executeJustifyLeft, supported, enabledInRichlyEditableText, stateJustifyLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyNone"_s, { executeJustifyLeft, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyRight"_s, { executeJustifyRight, supported, enabledInRichlyEditableText, stateJustifyRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionLeftToRight"_s, { executeMakeTextWritingDirectionLeftToRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionLeftToRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionNatural"_s, { executeMakeTextWritingDirectionNatural, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionNatural, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionRightToLeft"_s, { executeMakeTextWritingDirectionRightToLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionRightToLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackward"_s, { executeMoveBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackwardAndModifySelection"_s, { executeMoveBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDown"_s, { executeMoveDown, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDownAndModifySelection"_s, { executeMoveDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForward"_s, { executeMoveForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForwardAndModifySelection"_s, { executeMoveForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeft"_s, { executeMoveLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeftAndModifySelection"_s, { executeMoveLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDown"_s, { executeMovePageDown, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDownAndModifySelection"_s, { executeMovePageDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUp"_s, { executeMovePageUp, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUpAndModifySelection"_s, { executeMovePageUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphBackwardAndModifySelection"_s, { executeMoveParagraphBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphForwardAndModifySelection"_s, { executeMoveParagraphForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRight"_s, { executeMoveRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRightAndModifySelection"_s, { executeMoveRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocument"_s, { executeMoveToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocumentAndModifySelection"_s, { executeMoveToBeginningOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLine"_s, { executeMoveToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLineAndModifySelection"_s, { executeMoveToBeginningOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraph"_s, { executeMoveToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraphAndModifySelection"_s, { executeMoveToBeginningOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentence"_s, { executeMoveToBeginningOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentenceAndModifySelection"_s, { executeMoveToBeginningOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocument"_s, { executeMoveToEndOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocumentAndModifySelection"_s, { executeMoveToEndOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLine"_s, { executeMoveToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLineAndModifySelection"_s, { executeMoveToEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraph"_s, { executeMoveToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraphAndModifySelection"_s, { executeMoveToEndOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentence"_s, { executeMoveToEndOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentenceAndModifySelection"_s, { executeMoveToEndOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToLeftEndOfLine"_s, { executeMoveToLeftEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToLeftEndOfLineAndModifySelection"_s, { executeMoveToLeftEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToRightEndOfLine"_s, { executeMoveToRightEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToRightEndOfLineAndModifySelection"_s, { executeMoveToRightEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUp"_s, { executeMoveUp, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUpAndModifySelection"_s, { executeMoveUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackward"_s, { executeMoveWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackwardAndModifySelection"_s, { executeMoveWordBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForward"_s, { executeMoveWordForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForwardAndModifySelection"_s, { executeMoveWordForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeft"_s, { executeMoveWordLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeftAndModifySelection"_s, { executeMoveWordLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRight"_s, { executeMoveWordRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRightAndModifySelection"_s, { executeMoveWordRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Outdent"_s, { executeOutdent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "OverWrite"_s, { executeToggleOverwrite, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Paste"_s, { executePaste, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAndMatchStyle"_s, { executePasteAndMatchStyle, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAsPlainText"_s, { executePasteAsPlainText, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAsQuotation"_s, { executePasteAsQuotation, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteFont"_s, { executePasteFont, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "Print"_s, { executePrint, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Redo"_s, { executeRedo, supported, enabledRedo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "RemoveFormat"_s, { executeRemoveFormat, supported, enabledRangeInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollPageBackward"_s, { executeScrollPageBackward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollPageForward"_s, { executeScrollPageForward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollLineUp"_s, { executeScrollLineUp, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollLineDown"_s, { executeScrollLineDown, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollToBeginningOfDocument"_s, { executeScrollToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollToEndOfDocument"_s, { executeScrollToEndOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectAll"_s, { executeSelectAll, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectLine"_s, { executeSelectLine, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectParagraph"_s, { executeSelectParagraph, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectSentence"_s, { executeSelectSentence, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectToMark"_s, { executeSelectToMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectWord"_s, { executeSelectWord, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SetMark"_s, { executeSetMark, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Strikethrough"_s, { executeStrikethrough, supported, enabledInRichlyEditableText, stateStrikethrough, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "StyleWithCSS"_s, { executeStyleWithCSS, supported, enabled, stateStyleWithCSS, valueAsEmptyString, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Subscript"_s, { executeSubscript, supported, enabledInRichlyEditableText, stateSubscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Superscript"_s, { executeSuperscript, supported, enabledInRichlyEditableText, stateSuperscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SwapWithMark"_s, { executeSwapWithMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleBold"_s, { executeToggleBold, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleItalic"_s, { executeToggleItalic, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleUnderline"_s, { executeUnderline, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Transpose"_s, { executeTranspose, supported, enableCaretInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Underline"_s, { executeUnderline, supported, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Undo"_s, { executeUndo, supported, enabledUndo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unlink"_s, { executeUnlink, supported, enabledRangeInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unscript"_s, { executeUnscript, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unselect"_s, { executeUnselect, supported, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "UseCSS"_s, { executeUseCSS, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Yank"_s, { executeYank, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "YankAndSelect"_s, { executeYankAndSelect, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },

#if PLATFORM(GTK)
        { "PasteGlobalSelection"_s, { executePasteGlobalSelection, supportedFromMenuOrKeyBinding, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
#endif

#if PLATFORM(COCOA)
        { "TakeFindStringFromSelection"_s, { executeTakeFindStringFromSelection, supportedFromMenuOrKeyBinding, enabledTakeFindStringFromSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
#endif
    };

    // These unsupported commands are listed here since they appear in the Microsoft
    // documentation used as the starting point for our DOM executeCommand support.
    //
    // 2D-Position (not supported)
    // AbsolutePosition (not supported)
    // BlockDirLTR (not supported)
    // BlockDirRTL (not supported)
    // BrowseMode (not supported)
    // ClearAuthenticationCache (not supported)
    // CreateBookmark (not supported)
    // DirLTR (not supported)
    // DirRTL (not supported)
    // EditMode (not supported)
    // InlineDirLTR (not supported)
    // InlineDirRTL (not supported)
    // InsertButton (not supported)
    // InsertFieldSet (not supported)
    // InsertIFrame (not supported)
    // InsertInputButton (not supported)
    // InsertInputCheckbox (not supported)
    // InsertInputFileUpload (not supported)
    // InsertInputHidden (not supported)
    // InsertInputImage (not supported)
    // InsertInputPassword (not supported)
    // InsertInputRadio (not supported)
    // InsertInputReset (not supported)
    // InsertInputSubmit (not supported)
    // InsertInputText (not supported)
    // InsertMarquee (not supported)
    // InsertSelectDropDown (not supported)
    // InsertSelectListBox (not supported)
    // InsertTextArea (not supported)
    // LiveResize (not supported)
    // MultipleSelection (not supported)
    // Open (not supported)
    // PlayImage (not supported)
    // Refresh (not supported)
    // RemoveParaFormat (not supported)
    // SaveAs (not supported)
    // SizeToControl (not supported)
    // SizeToControlHeight (not supported)
    // SizeToControlWidth (not supported)
    // Stop (not supported)
    // StopImage (not supported)
    // Unbookmark (not supported)

    CommandMap& commandMap = *new CommandMap;

    for (auto& command : commands) {
        ASSERT(!commandMap.get(command.name));
        commandMap.set(command.name, &command.command);
    }

    return commandMap;
}

static const EditorInternalCommand* internalCommand(const String& commandName)
{
    static const CommandMap& commandMap = createCommandMap();
    return commandName.isEmpty() ? 0 : commandMap.get(commandName);
}

Editor::Command Editor::command(const String& commandName)
{
    return Command(internalCommand(commandName), EditorCommandSource::MenuOrKeyBinding, m_document);
}

Editor::Command Editor::command(const String& commandName, EditorCommandSource source)
{
    return Command(internalCommand(commandName), source, m_document);
}

bool Editor::commandIsSupportedFromMenuOrKeyBinding(const String& commandName)
{
    return internalCommand(commandName);
}

Editor::Command::Command()
{
}

Editor::Command::Command(const EditorInternalCommand* command, EditorCommandSource source, Document& document)
    : m_command(command)
    , m_source(source)
    , m_document(command ? &document : nullptr)
    , m_frame(command ? document.frame() : nullptr)
{
    ASSERT(command || !m_document);
}

bool Editor::Command::execute(const String& parameter, Event* triggeringEvent) const
{
    if (!isEnabled(triggeringEvent)) {
        // Let certain commands be executed when performed explicitly even if they are disabled.
        if (!allowExecutionWhenDisabled())
            return false;
    }

    m_document->updateLayoutIgnorePendingStylesheets();
    if (m_document->frame() != m_frame)
        return false;

    return m_command->execute(*m_frame, triggeringEvent, m_source, parameter);
}

bool Editor::Command::execute(Event* triggeringEvent) const
{
    return execute(String(), triggeringEvent);
}

bool Editor::Command::isSupported() const
{
    if (!m_command)
        return false;
    switch (m_source) {
    case EditorCommandSource::MenuOrKeyBinding:
        return true;
    case EditorCommandSource::DOM:
    case EditorCommandSource::DOMWithUserInterface:
        return m_command->isSupportedFromDOM(m_frame.get());
    }
    ASSERT_NOT_REACHED();
    return false;
}

bool Editor::Command::isEnabled(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return false;
    return m_command->isEnabled(*m_frame, triggeringEvent, m_source);
}

TriState Editor::Command::state(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return TriState::False;
    return m_command->state(*m_frame, triggeringEvent);
}

String Editor::Command::value(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return String();
    if (m_command->value == valueNull && m_command->state != stateNone)
        return m_command->state(*m_frame, triggeringEvent) == TriState::True ? "true"_s : "false"_s;
    return m_command->value(*m_frame, triggeringEvent);
}

bool Editor::Command::isTextInsertion() const
{
    return m_command && m_command->isTextInsertion;
}

bool Editor::Command::allowExecutionWhenDisabled() const
{
    if (!isSupported() || !m_frame)
        return false;
    return m_command->allowExecutionWhenDisabled(*m_frame, m_source);
}

} // namespace WebCore
