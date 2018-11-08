/*
 * Copyright (C) 2006-2008, 2014, 2016 Apple Inc. All rights reserved.
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
#include "Event.h"
#include "EventHandler.h"
#include "FormatBlockCommand.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "HTMLFontElement.h"
#include "HTMLHRElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "InsertNestedListCommand.h"
#include "Page.h"
#include "Pasteboard.h"
#include "RenderBox.h"
#include "ReplaceSelectionCommand.h"
#include "Scrollbar.h"
#include "Settings.h"
#include "StyleProperties.h"
#include "TypingCommand.h"
#include "UnlinkCommand.h"
#include "UserGestureIndicator.h"
#include "UserTypingGestureIndicator.h"
#include "markup.h"
#include <pal/system/Sound.h>
#include <pal/text/KillRing.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

using namespace HTMLNames;

class EditorInternalCommand {
public:
    bool (*execute)(Frame&, Event*, EditorCommandSource, const String&);
    bool (*isSupportedFromDOM)(Frame*);
    bool (*isEnabled)(Frame&, Event*, EditorCommandSource);
    TriState (*state)(Frame&, Event*);
    String (*value)(Frame&, Event*);
    bool isTextInsertion;
    bool (*allowExecutionWhenDisabled)(Frame&, EditorCommandSource);
};

typedef HashMap<String, const EditorInternalCommand*, ASCIICaseInsensitiveHash> CommandMap;

static const bool notTextInsertion = false;
static const bool isTextInsertion = true;

// Related to Editor::selectionForCommand.
// Certain operations continue to use the target control's selection even if the event handler
// already moved the selection outside of the text control.
static Frame* targetFrame(Frame& frame, Event* event)
{
    if (!event)
        return &frame;
    if (!is<Node>(event->target()))
        return &frame;
    return downcast<Node>(*event->target()).document().frame();
}

static bool applyCommandToFrame(Frame& frame, EditorCommandSource source, EditAction action, Ref<EditingStyle>&& style)
{
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        // Use InvertColor for testing purposes. foreColor and backColor are never triggered with CommandFromMenuOrKeyBinding outside DRT/WTR.
        frame.editor().applyStyleToSelection(WTFMove(style), action, Editor::ColorFilterMode::InvertColor);
        return true;
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        frame.editor().applyStyle(WTFMove(style), EditAction::Unspecified, Editor::ColorFilterMode::UseOriginalColor);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool isStylePresent(Editor& editor, CSSPropertyID propertyID, const char* onValue)
{
    // Style is considered present when
    // Mac: present at the beginning of selection
    // Windows: present throughout the selection
    if (editor.behavior().shouldToggleStyleBasedOnStartOfSelection())
        return editor.selectionStartHasStyle(propertyID, onValue);
    return editor.selectionHasStyle(propertyID, onValue) == TrueTriState;
}

static bool executeApplyStyle(Frame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, propertyValue));
}

static bool executeApplyStyle(Frame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, CSSValueID propertyValue)
{
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, propertyValue));
}

static bool executeToggleStyle(Frame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const char* offValue, const char* onValue)
{
    bool styleIsPresent = isStylePresent(frame.editor(), propertyID, onValue);
    return applyCommandToFrame(frame, source, action, EditingStyle::create(propertyID, styleIsPresent ? offValue : onValue));
}

static bool executeApplyParagraphStyle(Frame& frame, EditorCommandSource source, EditAction action, CSSPropertyID propertyID, const String& propertyValue)
{
    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(propertyID, propertyValue);
    // FIXME: We don't call shouldApplyStyle when the source is DOM; is there a good reason for that?
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        frame.editor().applyParagraphStyleToSelection(style.get(), action);
        return true;
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        frame.editor().applyParagraphStyle(style.get());
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertFragment(Frame& frame, Ref<DocumentFragment>&& fragment)
{
    ASSERT(frame.document());
    ReplaceSelectionCommand::create(*frame.document(), WTFMove(fragment), ReplaceSelectionCommand::PreventNesting, EditAction::Insert)->apply();
    return true;
}

static bool executeInsertNode(Frame& frame, Ref<Node>&& content)
{
    auto fragment = DocumentFragment::create(*frame.document());
    if (fragment->appendChild(content).hasException())
        return false;
    return executeInsertFragment(frame, WTFMove(fragment));
}

static bool expandSelectionToGranularity(Frame& frame, TextGranularity granularity)
{
    VisibleSelection selection = frame.selection().selection();
    selection.expandUsingGranularity(granularity);
    RefPtr<Range> newRange = selection.toNormalizedRange();
    if (!newRange)
        return false;
    if (newRange->collapsed())
        return false;
    RefPtr<Range> oldRange = selection.toNormalizedRange();
    EAffinity affinity = selection.affinity();
    if (!frame.editor().client()->shouldChangeSelectedRange(oldRange.get(), newRange.get(), affinity, false))
        return false;
    frame.selection().setSelectedRange(newRange.get(), affinity, true);
    return true;
}

static TriState stateStyle(Frame& frame, CSSPropertyID propertyID, const char* desiredValue)
{
    if (frame.editor().behavior().shouldToggleStyleBasedOnStartOfSelection())
        return frame.editor().selectionStartHasStyle(propertyID, desiredValue) ? TrueTriState : FalseTriState;
    return frame.editor().selectionHasStyle(propertyID, desiredValue);
}

static String valueStyle(Frame& frame, CSSPropertyID propertyID)
{
    // FIXME: Rather than retrieving the style at the start of the current selection,
    // we should retrieve the style present throughout the selection for non-Mac platforms.
    return frame.editor().selectionStartCSSPropertyValue(propertyID);
}

static TriState stateTextWritingDirection(Frame& frame, WritingDirection direction)
{
    bool hasNestedOrMultipleEmbeddings;
    WritingDirection selectionDirection = EditingStyle::textDirectionForSelection(frame.selection().selection(),
        frame.selection().typingStyle(), hasNestedOrMultipleEmbeddings);
    // FXIME: We should be returning MixedTriState when selectionDirection == direction && hasNestedOrMultipleEmbeddings
    return (selectionDirection == direction && !hasNestedOrMultipleEmbeddings) ? TrueTriState : FalseTriState;
}

static unsigned verticalScrollDistance(Frame& frame)
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

static RefPtr<Range> unionDOMRanges(Range& a, Range& b)
{
    Range& start = a.compareBoundaryPoints(Range::START_TO_START, b).releaseReturnValue() <= 0 ? a : b;
    Range& end = a.compareBoundaryPoints(Range::END_TO_END, b).releaseReturnValue() <= 0 ? b : a;
    return Range::create(a.ownerDocument(), &start.startContainer(), start.startOffset(), &end.endContainer(), end.endOffset());
}

// Execute command functions

static bool executeBackColor(Frame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetBackgroundColor, CSSPropertyBackgroundColor, value);
}

static bool executeCopy(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().copy();
    return true;
}

static bool executeCreateLink(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box to let the user enter a URL.
    if (value.isEmpty())
        return false;
    ASSERT(frame.document());
    CreateLinkCommand::create(*frame.document(), value)->apply();
    return true;
}

static bool executeCut(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == CommandFromMenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().cut();
    } else
        frame.editor().cut();
    return true;
}

static bool executeClearText(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().clearText();
    return true;
}

static bool executeDefaultParagraphSeparator(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    if (equalLettersIgnoringASCIICase(value, "div"))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparatorIsDiv);
    else if (equalLettersIgnoringASCIICase(value, "p"))
        frame.editor().setDefaultParagraphSeparator(EditorParagraphSeparatorIsP);

    return true;
}

static bool executeDelete(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding: {
        // Doesn't modify the text if the current selection isn't a range.
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().performDelete();
        return true;
    }
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        // If the current selection is a caret, delete the preceding character. IE performs forwardDelete, but we currently side with Firefox.
        // Doesn't scroll to make the selection visible, or modify the kill ring (this time, siding with IE, not Firefox).
        TypingCommand::deleteKeyPressed(*frame.document(), frame.selection().granularity() == WordGranularity ? TypingCommand::SmartDelete : 0);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeDeleteBackward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteBackwardByDecomposingPreviousCharacter(Frame& frame, Event*, EditorCommandSource, const String&)
{
    LOG_ERROR("DeleteBackwardByDecomposingPreviousCharacter is not implemented, doing DeleteBackward instead");
    frame.editor().deleteWithDirection(DirectionBackward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteForward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionForward, CharacterGranularity, false, true);
    return true;
}

static bool executeDeleteToBeginningOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToBeginningOfParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph (like DeleteToEndOfParagraph).
    frame.editor().deleteWithDirection(DirectionForward, LineBoundary, true, false);
    return true;
}

static bool executeDeleteToEndOfParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    // Despite its name, this command should delete the newline at the end of
    // a paragraph if you are at the end of a paragraph.
    frame.editor().deleteWithDirection(DirectionForward, ParagraphBoundary, true, false);
    return true;
}

static bool executeDeleteToMark(Frame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<Range> mark = frame.editor().mark().toNormalizedRange();
    FrameSelection& selection = frame.selection();
    if (mark && frame.editor().selectedRange()) {
        bool selected = selection.setSelectedRange(unionDOMRanges(*mark, *frame.editor().selectedRange()).get(), DOWNSTREAM, true);
        ASSERT(selected);
        if (!selected)
            return false;
    }
    frame.editor().performDelete();
    frame.editor().setMark(selection.selection());
    return true;
}

static bool executeDeleteWordBackward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionBackward, WordGranularity, true, false);
    return true;
}

static bool executeDeleteWordForward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().deleteWithDirection(DirectionForward, WordGranularity, true, false);
    return true;
}

static bool executeFindString(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    return frame.editor().findString(value, { CaseInsensitive, WrapAround, DoNotTraverseFlatTree });
}

static bool executeFontName(Frame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetFont, CSSPropertyFontFamily, value);
}

static bool executeFontSize(Frame& frame, Event*, EditorCommandSource source, const String& value)
{
    CSSValueID size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return false;
    return executeApplyStyle(frame, source, EditAction::ChangeAttributes, CSSPropertyFontSize, size);
}

static bool executeFontSizeDelta(Frame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::ChangeAttributes, CSSPropertyWebkitFontSizeDelta, value);
}

static bool executeForeColor(Frame& frame, Event*, EditorCommandSource source, const String& value)
{
    return executeApplyStyle(frame, source, EditAction::SetColor, CSSPropertyColor, value);
}

static bool executeFormatBlock(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    String tagName = value.convertToASCIILowercase();
    if (tagName[0] == '<' && tagName[tagName.length() - 1] == '>')
        tagName = tagName.substring(1, tagName.length() - 2);

    auto qualifiedTagName = Document::parseQualifiedName(xhtmlNamespaceURI, tagName);
    if (qualifiedTagName.hasException())
        return false;

    ASSERT(frame.document());
    auto command = FormatBlockCommand::create(*frame.document(), qualifiedTagName.releaseReturnValue());
    command->apply();
    return command->didApply();
}

static bool executeForwardDelete(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        frame.editor().deleteWithDirection(DirectionForward, CharacterGranularity, false, true);
        return true;
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // ForwardDelete is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with Delete.
        TypingCommand::forwardDeleteKeyPressed(*frame.document());
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeIgnoreSpelling(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().ignoreSpelling();
    return true;
}

static bool executeIndent(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Indent)->apply();
    return true;
}

static bool executeInsertBacktab(Frame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t"_s, event, TextEventInputBackTab);
}

static bool executeInsertHorizontalRule(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    Ref<HTMLHRElement> rule = HTMLHRElement::create(*frame.document());
    if (!value.isEmpty())
        rule->setIdAttribute(value);
    return executeInsertNode(frame, WTFMove(rule));
}

static bool executeInsertHTML(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    return executeInsertFragment(frame, createFragmentFromMarkup(*frame.document(), value, emptyString()));
}

static bool executeInsertImage(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box and let the user choose a local image.
    Ref<HTMLImageElement> image = HTMLImageElement::create(*frame.document());
    image->setSrc(value);
    return executeInsertNode(frame, WTFMove(image));
}

static bool executeInsertLineBreak(Frame& frame, Event* event, EditorCommandSource source, const String&)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\n"_s, event, TextEventInputLineBreak);
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        // Doesn't scroll to make the selection visible, or modify the kill ring.
        // InsertLineBreak is not implemented in IE or Firefox, so this behavior is only needed for
        // backward compatibility with ourselves, and for consistency with other commands.
        TypingCommand::insertLineBreak(*frame.document(), 0);
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool executeInsertNewline(Frame& frame, Event* event, EditorCommandSource, const String&)
{
    Frame* targetFrame = WebCore::targetFrame(frame, event);
    return targetFrame->eventHandler().handleTextInputEvent("\n"_s, event, targetFrame->editor().canEditRichly() ? TextEventInputKeyboard : TextEventInputLineBreak);
}

static bool executeInsertNewlineInQuotedContent(Frame& frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparatorInQuotedContent(*frame.document());
    return true;
}

static bool executeInsertOrderedList(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertListCommand::create(*frame.document(), InsertListCommand::Type::OrderedList)->apply();
    return true;
}

static bool executeInsertParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    TypingCommand::insertParagraphSeparator(*frame.document(), 0);
    return true;
}

static bool executeInsertTab(Frame& frame, Event* event, EditorCommandSource, const String&)
{
    return targetFrame(frame, event)->eventHandler().handleTextInputEvent("\t"_s, event);
}

static bool executeInsertText(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    TypingCommand::insertText(*frame.document(), value, 0);
    return true;
}

static bool executeInsertUnorderedList(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertListCommand::create(*frame.document(), InsertListCommand::Type::UnorderedList)->apply();
    return true;
}

static bool executeInsertNestedUnorderedList(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertNestedListCommand::insertUnorderedList(*frame.document());
    return true;
}

static bool executeInsertNestedOrderedList(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    InsertNestedListCommand::insertOrderedList(*frame.document());
    return true;
}

static bool executeJustifyCenter(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::Center, CSSPropertyTextAlign, "center"_s);
}

static bool executeJustifyFull(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::Justify, CSSPropertyTextAlign, "justify"_s);
}

static bool executeJustifyLeft(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::AlignLeft, CSSPropertyTextAlign, "left"_s);
}

static bool executeJustifyRight(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyParagraphStyle(frame, source, EditAction::AlignRight, CSSPropertyTextAlign, "right"_s);
}

static bool executeMakeTextWritingDirectionLeftToRight(Frame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueLtr);
    frame.editor().applyStyle(style.get(), EditAction::SetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionNatural(Frame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueNormal);
    frame.editor().applyStyle(style.get(), EditAction::SetWritingDirection);
    return true;
}

static bool executeMakeTextWritingDirectionRightToLeft(Frame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<MutableStyleProperties> style = MutableStyleProperties::create();
    style->setProperty(CSSPropertyUnicodeBidi, CSSValueEmbed);
    style->setProperty(CSSPropertyDirection, CSSValueRtl);
    frame.editor().applyStyle(style.get(), EditAction::SetWritingDirection);
    return true;
}

static bool executeMoveBackward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveBackwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveDown(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, LineGranularity, UserTriggered);
}

static bool executeMoveDownAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, LineGranularity, UserTriggered);
    return true;
}

static bool executeMoveForward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveForwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveLeft(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, CharacterGranularity, UserTriggered);
}

static bool executeMoveLeftAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMovePageDown(Frame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationMove, distance, FrameSelection::DirectionDown,
        UserTriggered, FrameSelection::AlignCursorOnScrollAlways);
}

static bool executeMovePageDownAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationExtend, distance, FrameSelection::DirectionDown,
        UserTriggered, FrameSelection::AlignCursorOnScrollAlways);
}

static bool executeMovePageUp(Frame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationMove, distance, FrameSelection::DirectionUp,
        UserTriggered, FrameSelection::AlignCursorOnScrollAlways);
}

static bool executeMovePageUpAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    unsigned distance = verticalScrollDistance(frame);
    if (!distance)
        return false;
    return frame.selection().modify(FrameSelection::AlterationExtend, distance, FrameSelection::DirectionUp,
        UserTriggered, FrameSelection::AlignCursorOnScrollAlways);
}

static bool executeMoveRight(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, CharacterGranularity, UserTriggered);
}

static bool executeMoveRightAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, CharacterGranularity, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfDocument(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfDocumentAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfLineAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfParagraphAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfSentence(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToBeginningOfSentenceAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfDocument(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfDocumentAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, DocumentBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfSentence(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfSentenceAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, SentenceBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfLineAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveToEndOfParagraphAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, ParagraphBoundary, UserTriggered);
    return true;
}

static bool executeMoveParagraphBackwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveParagraphForwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, ParagraphGranularity, UserTriggered);
    return true;
}

static bool executeMoveUp(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, LineGranularity, UserTriggered);
}

static bool executeMoveUpAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, LineGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordBackward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionBackward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordBackwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionBackward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordForward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionForward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordForwardAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionForward, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordLeft(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordLeftAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordRight(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveWordRightAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, WordGranularity, UserTriggered);
    return true;
}

static bool executeMoveToLeftEndOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionLeft, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToLeftEndOfLineAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionLeft, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToRightEndOfLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationMove, DirectionRight, LineBoundary, UserTriggered);
    return true;
}

static bool executeMoveToRightEndOfLineAndModifySelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().modify(FrameSelection::AlterationExtend, DirectionRight, LineBoundary, UserTriggered);
    return true;
}

static bool executeOutdent(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    IndentOutdentCommand::create(*frame.document(), IndentOutdentCommand::Outdent)->apply();
    return true;
}

static bool executeToggleOverwrite(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().toggleOverwriteModeEnabled();
    return true;
}

static bool executePaste(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == CommandFromMenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().paste();
    } else
        frame.editor().paste();
    return true;
}

#if PLATFORM(GTK)

static bool executePasteGlobalSelection(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    // FIXME: This check should be in an enable function, not here.
    if (!frame.editor().client()->supportsGlobalSelection())
        return false;

    ASSERT_UNUSED(source, source == CommandFromMenuOrKeyBinding);
    UserTypingGestureIndicator typingGestureIndicator(frame);
    frame.editor().paste(*Pasteboard::createForGlobalSelection());
    return true;
}

#endif

static bool executePasteAndMatchStyle(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == CommandFromMenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsPlainText();
    } else
        frame.editor().pasteAsPlainText();
    return true;
}

static bool executePasteAsPlainText(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == CommandFromMenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsPlainText();
    } else
        frame.editor().pasteAsPlainText();
    return true;
}

static bool executePasteAsQuotation(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    if (source == CommandFromMenuOrKeyBinding) {
        UserTypingGestureIndicator typingGestureIndicator(frame);
        frame.editor().pasteAsQuotation();
    } else
        frame.editor().pasteAsQuotation();
    return true;
}

static bool executePrint(Frame& frame, Event*, EditorCommandSource, const String&)
{
    Page* page = frame.page();
    if (!page)
        return false;
    return page->chrome().print(frame);
}

static bool executeRedo(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().redo();
    return true;
}

static bool executeRemoveFormat(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().removeFormattingAndStyle();
    return true;
}

static bool executeScrollPageBackward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionBackward, ScrollByPage);
}

static bool executeScrollPageForward(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionForward, ScrollByPage);
}

static bool executeScrollLineUp(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().scrollRecursively(ScrollUp, ScrollByLine);
}

static bool executeScrollLineDown(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().scrollRecursively(ScrollDown, ScrollByLine);
}

static bool executeScrollToBeginningOfDocument(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionBackward, ScrollByDocument);
}

static bool executeScrollToEndOfDocument(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return frame.eventHandler().logicalScrollRecursively(ScrollBlockDirectionForward, ScrollByDocument);
}

static bool executeSelectAll(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().selectAll();
    return true;
}

static bool executeSelectLine(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, LineGranularity);
}

static bool executeSelectParagraph(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, ParagraphGranularity);
}

static bool executeSelectSentence(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, SentenceGranularity);
}

static bool executeSelectToMark(Frame& frame, Event*, EditorCommandSource, const String&)
{
    RefPtr<Range> mark = frame.editor().mark().toNormalizedRange();
    RefPtr<Range> selection = frame.editor().selectedRange();
    if (!mark || !selection) {
        PAL::systemBeep();
        return false;
    }
    frame.selection().setSelectedRange(unionDOMRanges(*mark, *selection).get(), DOWNSTREAM, true);
    return true;
}

static bool executeSelectWord(Frame& frame, Event*, EditorCommandSource, const String&)
{
    return expandSelectionToGranularity(frame, WordGranularity);
}

static bool executeSetMark(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().setMark(frame.selection().selection());
    return true;
}

static TextDecorationChange textDecorationChangeForToggling(Editor& editor, CSSPropertyID propertyID, const char* onValue)
{
    return isStylePresent(editor, propertyID, onValue) ? TextDecorationChange::Remove : TextDecorationChange::Add;
}

static bool executeStrikethrough(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    Ref<EditingStyle> style = EditingStyle::create();
    style->setStrikeThroughChange(textDecorationChangeForToggling(frame.editor(), CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s));
    // FIXME: Needs a new EditAction!
    return applyCommandToFrame(frame, source, EditAction::Underline, WTFMove(style));
}

static bool executeStyleWithCSS(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(!equalLettersIgnoringASCIICase(value, "false"));
    return true;
}

static bool executeUseCSS(Frame& frame, Event*, EditorCommandSource, const String& value)
{
    frame.editor().setShouldStyleWithCSS(equalLettersIgnoringASCIICase(value, "false"));
    return true;
}

static bool executeSubscript(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Subscript, CSSPropertyVerticalAlign, "baseline"_s, "sub"_s);
}

static bool executeSuperscript(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Superscript, CSSPropertyVerticalAlign, "baseline"_s, "super"_s);
}

static bool executeSwapWithMark(Frame& frame, Event*, EditorCommandSource, const String&)
{
    Ref<Frame> protector(frame);
    const VisibleSelection& mark = frame.editor().mark();
    const VisibleSelection& selection = frame.selection().selection();
    if (mark.isNone() || selection.isNone()) {
        PAL::systemBeep();
        return false;
    }
    frame.selection().setSelection(mark);
    frame.editor().setMark(selection);
    return true;
}

#if PLATFORM(MAC)
static bool executeTakeFindStringFromSelection(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().takeFindStringFromSelection();
    return true;
}
#endif

static bool executeToggleBold(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Bold, CSSPropertyFontWeight, "normal"_s, "bold"_s);
}

static bool executeToggleItalic(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeToggleStyle(frame, source, EditAction::Italics, CSSPropertyFontStyle, "normal"_s, "italic"_s);
}

static bool executeTranspose(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().transpose();
    return true;
}

static bool executeUnderline(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    Ref<EditingStyle> style = EditingStyle::create();
    TextDecorationChange change = textDecorationChangeForToggling(frame.editor(), CSSPropertyWebkitTextDecorationsInEffect, "underline"_s);
    style->setUnderlineChange(change);
    return applyCommandToFrame(frame, source, EditAction::Underline, WTFMove(style));
}

static bool executeUndo(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().undo();
    return true;
}

static bool executeUnlink(Frame& frame, Event*, EditorCommandSource, const String&)
{
    ASSERT(frame.document());
    UnlinkCommand::create(*frame.document())->apply();
    return true;
}

static bool executeUnscript(Frame& frame, Event*, EditorCommandSource source, const String&)
{
    return executeApplyStyle(frame, source, EditAction::Unscript, CSSPropertyVerticalAlign, "baseline"_s);
}

static bool executeUnselect(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.selection().clear();
    return true;
}

static bool executeYank(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), false, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

static bool executeYankAndSelect(Frame& frame, Event*, EditorCommandSource, const String&)
{
    frame.editor().insertTextWithoutSendingTextEvent(frame.editor().killRing().yank(), true, 0);
    frame.editor().killRing().setToYankedState();
    return true;
}

// Supported functions

static bool supported(Frame*)
{
    return true;
}

static bool supportedFromMenuOrKeyBinding(Frame*)
{
    return false;
}

static bool defaultValueForSupportedCopyCut(Frame& frame)
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

static bool supportedCopyCut(Frame* frame)
{
    if (!frame)
        return false;

    bool defaultValue = defaultValueForSupportedCopyCut(*frame);

    EditorClient* client = frame->editor().client();
    return client ? client->canCopyCut(frame, defaultValue) : defaultValue;
}

static bool supportedPaste(Frame* frame)
{
    if (!frame)
        return false;

    bool defaultValue = frame->settings().javaScriptCanAccessClipboard() && frame->settings().DOMPasteAllowed();

    EditorClient* client = frame->editor().client();
    return client ? client->canPaste(frame, defaultValue) : defaultValue;
}

// Enabled functions

static bool enabled(Frame&, Event*, EditorCommandSource)
{
    return true;
}

static bool enabledVisibleSelection(Frame& frame, Event* event, EditorCommandSource)
{
    // The term "visible" here includes a caret in editable text or a range in any text.
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return (selection.isCaret() && selection.isContentEditable()) || selection.isRange();
}

static bool caretBrowsingEnabled(Frame& frame)
{
    return frame.settings().caretBrowsingEnabled();
}

static EditorCommandSource dummyEditorCommandSource = static_cast<EditorCommandSource>(0);

static bool enabledVisibleSelectionOrCaretBrowsing(Frame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledVisibleSelection, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledVisibleSelection(frame, event, dummyEditorCommandSource);
}

static bool enabledVisibleSelectionAndMark(Frame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return ((selection.isCaret() && selection.isContentEditable()) || selection.isRange())
        && frame.editor().mark().isCaretOrRange();
}

static bool enableCaretInEditableText(Frame& frame, Event* event, EditorCommandSource)
{
    const VisibleSelection& selection = frame.editor().selectionForCommand(event);
    return selection.isCaret() && selection.isContentEditable();
}

static bool allowCopyCutFromDOM(Frame& frame)
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

static bool enabledCopy(Frame& frame, Event*, EditorCommandSource source)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:    
        return frame.editor().canDHTMLCopy() || frame.editor().canCopy();
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        return allowCopyCutFromDOM(frame) && (frame.editor().canDHTMLCopy() || frame.editor().canCopy());
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledCut(Frame& frame, Event*, EditorCommandSource source)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:    
        return frame.editor().canDHTMLCut() || frame.editor().canCut();
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        return allowCopyCutFromDOM(frame) && (frame.editor().canDHTMLCut() || frame.editor().canCut());
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledClearText(Frame& frame, Event*, EditorCommandSource)
{
    UNUSED_PARAM(frame);
    return false;
}

static bool enabledInEditableText(Frame& frame, Event* event, EditorCommandSource)
{
    return frame.editor().selectionForCommand(event).rootEditableElement();
}

static bool enabledDelete(Frame& frame, Event* event, EditorCommandSource source)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:    
        return frame.editor().canDelete();
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        // "Delete" from DOM is like delete/backspace keypress, affects selected range if non-empty,
        // otherwise removes a character
        return enabledInEditableText(frame, event, source);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static bool enabledInEditableTextOrCaretBrowsing(Frame& frame, Event* event, EditorCommandSource)
{
    // The EditorCommandSource parameter is unused in enabledInEditableText, so just pass a dummy variable
    return caretBrowsingEnabled(frame) || enabledInEditableText(frame, event, dummyEditorCommandSource);
}

static bool enabledInRichlyEditableText(Frame& frame, Event*, EditorCommandSource)
{
    const VisibleSelection& selection = frame.selection().selection();
    return selection.isCaretOrRange() && selection.isContentRichlyEditable() && selection.rootEditableElement();
}

static bool enabledPaste(Frame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canPaste();
}

static bool enabledRangeInEditableText(Frame& frame, Event*, EditorCommandSource)
{
    return frame.selection().isRange() && frame.selection().selection().isContentEditable();
}

static bool enabledRangeInRichlyEditableText(Frame& frame, Event*, EditorCommandSource)
{
    return frame.selection().isRange() && frame.selection().selection().isContentRichlyEditable();
}

static bool enabledRedo(Frame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canRedo();
}

#if PLATFORM(MAC)
static bool enabledTakeFindStringFromSelection(Frame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canCopyExcludingStandaloneImages();
}
#endif

static bool enabledUndo(Frame& frame, Event*, EditorCommandSource)
{
    return frame.editor().canUndo();
}

// State functions

static TriState stateNone(Frame&, Event*)
{
    return FalseTriState;
}

static TriState stateBold(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontWeight, "bold"_s);
}

static TriState stateItalic(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyFontStyle, "italic"_s);
}

static TriState stateOrderedList(Frame& frame, Event*)
{
    return frame.editor().selectionOrderedListState();
}

static TriState stateStrikethrough(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "line-through"_s);
}

static TriState stateStyleWithCSS(Frame& frame, Event*)
{
    return frame.editor().shouldStyleWithCSS() ? TrueTriState : FalseTriState;
}

static TriState stateSubscript(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "sub"_s);
}

static TriState stateSuperscript(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyVerticalAlign, "super"_s);
}

static TriState stateTextWritingDirectionLeftToRight(Frame& frame, Event*)
{
    return stateTextWritingDirection(frame, LeftToRightWritingDirection);
}

static TriState stateTextWritingDirectionNatural(Frame& frame, Event*)
{
    return stateTextWritingDirection(frame, NaturalWritingDirection);
}

static TriState stateTextWritingDirectionRightToLeft(Frame& frame, Event*)
{
    return stateTextWritingDirection(frame, RightToLeftWritingDirection);
}

static TriState stateUnderline(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyWebkitTextDecorationsInEffect, "underline"_s);
}

static TriState stateUnorderedList(Frame& frame, Event*)
{
    return frame.editor().selectionUnorderedListState();
}

static TriState stateJustifyCenter(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "center"_s);
}

static TriState stateJustifyFull(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "justify"_s);
}

static TriState stateJustifyLeft(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "left"_s);
}

static TriState stateJustifyRight(Frame& frame, Event*)
{
    return stateStyle(frame, CSSPropertyTextAlign, "right"_s);
}

// Value functions

static String valueNull(Frame&, Event*)
{
    return String();
}

static String valueBackColor(Frame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyBackgroundColor);
}

static String valueDefaultParagraphSeparator(Frame& frame, Event*)
{
    switch (frame.editor().defaultParagraphSeparator()) {
    case EditorParagraphSeparatorIsDiv:
        return divTag->localName();
    case EditorParagraphSeparatorIsP:
        return pTag->localName();
    }

    ASSERT_NOT_REACHED();
    return String();
}

static String valueFontName(Frame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontFamily);
}

static String valueFontSize(Frame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyFontSize);
}

static String valueFontSizeDelta(Frame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyWebkitFontSizeDelta);
}

static String valueForeColor(Frame& frame, Event*)
{
    return valueStyle(frame, CSSPropertyColor);
}

static String valueFormatBlock(Frame& frame, Event*)
{
    const VisibleSelection& selection = frame.selection().selection();
    if (selection.isNoneOrOrphaned() || !selection.isContentEditable())
        return emptyString();
    Element* formatBlockElement = FormatBlockCommand::elementForFormatBlockCommand(selection.firstRange().get());
    if (!formatBlockElement)
        return emptyString();
    return formatBlockElement->localName();
}

// allowExecutionWhenDisabled functions

static bool allowExecutionWhenDisabled(Frame&, EditorCommandSource)
{
    return true;
}

static bool doNotAllowExecutionWhenDisabled(Frame&, EditorCommandSource)
{
    return false;
}

static bool allowExecutionWhenDisabledCopyCut(Frame&, EditorCommandSource source)
{
    switch (source) {
    case CommandFromMenuOrKeyBinding:
        return true;
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

static bool allowExecutionWhenDisabledPaste(Frame& frame, EditorCommandSource)
{
    if (frame.mainFrame().loader().shouldSuppressTextInputFromEditing())
        return false;
    return true;
}

// Map of functions

struct CommandEntry {
    const char* name;
    EditorInternalCommand command;
};

static const CommandMap& createCommandMap()
{
    static const CommandEntry commands[] = {
        { "AlignCenter", { executeJustifyCenter, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignJustified", { executeJustifyFull, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignLeft", { executeJustifyLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "AlignRight", { executeJustifyRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "BackColor", { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueBackColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Bold", { executeToggleBold, supported, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ClearText", { executeClearText, supported, enabledClearText, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
        { "Copy", { executeCopy, supportedCopyCut, enabledCopy, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledCopyCut } },
        { "CreateLink", { executeCreateLink, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Cut", { executeCut, supportedCopyCut, enabledCut, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledCopyCut } },
        { "DefaultParagraphSeparator", { executeDefaultParagraphSeparator, supported, enabled, stateNone, valueDefaultParagraphSeparator, notTextInsertion, doNotAllowExecutionWhenDisabled} },
        { "Delete", { executeDelete, supported, enabledDelete, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackward", { executeDeleteBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteBackwardByDecomposingPreviousCharacter", { executeDeleteBackwardByDecomposingPreviousCharacter, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteForward", { executeDeleteForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfLine", { executeDeleteToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToBeginningOfParagraph", { executeDeleteToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfLine", { executeDeleteToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToEndOfParagraph", { executeDeleteToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteToMark", { executeDeleteToMark, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordBackward", { executeDeleteWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "DeleteWordForward", { executeDeleteWordForward, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FindString", { executeFindString, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontName", { executeFontName, supported, enabledInEditableText, stateNone, valueFontName, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSize", { executeFontSize, supported, enabledInEditableText, stateNone, valueFontSize, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FontSizeDelta", { executeFontSizeDelta, supported, enabledInEditableText, stateNone, valueFontSizeDelta, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForeColor", { executeForeColor, supported, enabledInRichlyEditableText, stateNone, valueForeColor, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "FormatBlock", { executeFormatBlock, supported, enabledInRichlyEditableText, stateNone, valueFormatBlock, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ForwardDelete", { executeForwardDelete, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "HiliteColor", { executeBackColor, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "IgnoreSpelling", { executeIgnoreSpelling, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Indent", { executeIndent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertBacktab", { executeInsertBacktab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHTML", { executeInsertHTML, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertHorizontalRule", { executeInsertHorizontalRule, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertImage", { executeInsertImage, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertLineBreak", { executeInsertLineBreak, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNewline", { executeInsertNewline, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },    
        { "InsertNewlineInQuotedContent", { executeInsertNewlineInQuotedContent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertOrderedList", { executeInsertOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNestedOrderedList", { executeInsertNestedOrderedList, supported, enabledInRichlyEditableText, stateOrderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertParagraph", { executeInsertParagraph, supported, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertTab", { executeInsertTab, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertText", { executeInsertText, supported, enabledInEditableText, stateNone, valueNull, isTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertUnorderedList", { executeInsertUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "InsertNestedUnorderedList", { executeInsertNestedUnorderedList, supported, enabledInRichlyEditableText, stateUnorderedList, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Italic", { executeToggleItalic, supported, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyCenter", { executeJustifyCenter, supported, enabledInRichlyEditableText, stateJustifyCenter, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyFull", { executeJustifyFull, supported, enabledInRichlyEditableText, stateJustifyFull, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyLeft", { executeJustifyLeft, supported, enabledInRichlyEditableText, stateJustifyLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyNone", { executeJustifyLeft, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "JustifyRight", { executeJustifyRight, supported, enabledInRichlyEditableText, stateJustifyRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionLeftToRight", { executeMakeTextWritingDirectionLeftToRight, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionLeftToRight, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionNatural", { executeMakeTextWritingDirectionNatural, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionNatural, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MakeTextWritingDirectionRightToLeft", { executeMakeTextWritingDirectionRightToLeft, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateTextWritingDirectionRightToLeft, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackward", { executeMoveBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveBackwardAndModifySelection", { executeMoveBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDown", { executeMoveDown, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveDownAndModifySelection", { executeMoveDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForward", { executeMoveForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveForwardAndModifySelection", { executeMoveForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeft", { executeMoveLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveLeftAndModifySelection", { executeMoveLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDown", { executeMovePageDown, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageDownAndModifySelection", { executeMovePageDownAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUp", { executeMovePageUp, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MovePageUpAndModifySelection", { executeMovePageUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphBackwardAndModifySelection", { executeMoveParagraphBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveParagraphForwardAndModifySelection", { executeMoveParagraphForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRight", { executeMoveRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveRightAndModifySelection", { executeMoveRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocument", { executeMoveToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfDocumentAndModifySelection", { executeMoveToBeginningOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLine", { executeMoveToBeginningOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfLineAndModifySelection", { executeMoveToBeginningOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraph", { executeMoveToBeginningOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfParagraphAndModifySelection", { executeMoveToBeginningOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentence", { executeMoveToBeginningOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToBeginningOfSentenceAndModifySelection", { executeMoveToBeginningOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocument", { executeMoveToEndOfDocument, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfDocumentAndModifySelection", { executeMoveToEndOfDocumentAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLine", { executeMoveToEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfLineAndModifySelection", { executeMoveToEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraph", { executeMoveToEndOfParagraph, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfParagraphAndModifySelection", { executeMoveToEndOfParagraphAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentence", { executeMoveToEndOfSentence, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToEndOfSentenceAndModifySelection", { executeMoveToEndOfSentenceAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToLeftEndOfLine", { executeMoveToLeftEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToLeftEndOfLineAndModifySelection", { executeMoveToLeftEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToRightEndOfLine", { executeMoveToRightEndOfLine, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveToRightEndOfLineAndModifySelection", { executeMoveToRightEndOfLineAndModifySelection, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUp", { executeMoveUp, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveUpAndModifySelection", { executeMoveUpAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackward", { executeMoveWordBackward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordBackwardAndModifySelection", { executeMoveWordBackwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForward", { executeMoveWordForward, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordForwardAndModifySelection", { executeMoveWordForwardAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeft", { executeMoveWordLeft, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordLeftAndModifySelection", { executeMoveWordLeftAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRight", { executeMoveWordRight, supportedFromMenuOrKeyBinding, enabledInEditableTextOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "MoveWordRightAndModifySelection", { executeMoveWordRightAndModifySelection, supportedFromMenuOrKeyBinding, enabledVisibleSelectionOrCaretBrowsing, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Outdent", { executeOutdent, supported, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "OverWrite", { executeToggleOverwrite, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Paste", { executePaste, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAndMatchStyle", { executePasteAndMatchStyle, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAsPlainText", { executePasteAsPlainText, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "PasteAsQuotation", { executePasteAsQuotation, supportedPaste, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabledPaste } },
        { "Print", { executePrint, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Redo", { executeRedo, supported, enabledRedo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "RemoveFormat", { executeRemoveFormat, supported, enabledRangeInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollPageBackward", { executeScrollPageBackward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollPageForward", { executeScrollPageForward, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollLineUp", { executeScrollLineUp, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollLineDown", { executeScrollLineDown, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollToBeginningOfDocument", { executeScrollToBeginningOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ScrollToEndOfDocument", { executeScrollToEndOfDocument, supportedFromMenuOrKeyBinding, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectAll", { executeSelectAll, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectLine", { executeSelectLine, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectParagraph", { executeSelectParagraph, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectSentence", { executeSelectSentence, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectToMark", { executeSelectToMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SelectWord", { executeSelectWord, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SetMark", { executeSetMark, supportedFromMenuOrKeyBinding, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Strikethrough", { executeStrikethrough, supported, enabledInRichlyEditableText, stateStrikethrough, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "StyleWithCSS", { executeStyleWithCSS, supported, enabled, stateStyleWithCSS, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Subscript", { executeSubscript, supported, enabledInRichlyEditableText, stateSubscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Superscript", { executeSuperscript, supported, enabledInRichlyEditableText, stateSuperscript, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "SwapWithMark", { executeSwapWithMark, supportedFromMenuOrKeyBinding, enabledVisibleSelectionAndMark, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleBold", { executeToggleBold, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateBold, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleItalic", { executeToggleItalic, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateItalic, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "ToggleUnderline", { executeUnderline, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Transpose", { executeTranspose, supported, enableCaretInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Underline", { executeUnderline, supported, enabledInRichlyEditableText, stateUnderline, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Undo", { executeUndo, supported, enabledUndo, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unlink", { executeUnlink, supported, enabledRangeInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unscript", { executeUnscript, supportedFromMenuOrKeyBinding, enabledInRichlyEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Unselect", { executeUnselect, supported, enabledVisibleSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "UseCSS", { executeUseCSS, supported, enabled, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "Yank", { executeYank, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
        { "YankAndSelect", { executeYankAndSelect, supportedFromMenuOrKeyBinding, enabledInEditableText, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },

#if PLATFORM(GTK)
        { "PasteGlobalSelection", { executePasteGlobalSelection, supportedFromMenuOrKeyBinding, enabledPaste, stateNone, valueNull, notTextInsertion, allowExecutionWhenDisabled } },
#endif

#if PLATFORM(MAC)
        { "TakeFindStringFromSelection", { executeTakeFindStringFromSelection, supportedFromMenuOrKeyBinding, enabledTakeFindStringFromSelection, stateNone, valueNull, notTextInsertion, doNotAllowExecutionWhenDisabled } },
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
    return Command(internalCommand(commandName), CommandFromMenuOrKeyBinding, m_frame);
}

Editor::Command Editor::command(const String& commandName, EditorCommandSource source)
{
    return Command(internalCommand(commandName), source, m_frame);
}

bool Editor::commandIsSupportedFromMenuOrKeyBinding(const String& commandName)
{
    return internalCommand(commandName);
}

Editor::Command::Command()
{
}

Editor::Command::Command(const EditorInternalCommand* command, EditorCommandSource source, Frame& frame)
    : m_command(command)
    , m_source(source)
    , m_frame(command ? &frame : nullptr)
{
    ASSERT(command || !m_frame);
}

bool Editor::Command::execute(const String& parameter, Event* triggeringEvent) const
{
    if (!isEnabled(triggeringEvent)) {
        // Let certain commands be executed when performed explicitly even if they are disabled.
        if (!allowExecutionWhenDisabled())
            return false;
    }
    auto document = m_frame->document();
    document->updateLayoutIgnorePendingStylesheets();
    if (m_frame->document() != document)
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
    case CommandFromMenuOrKeyBinding:
        return true;
    case CommandFromDOM:
    case CommandFromDOMWithUserInterface:
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
        return FalseTriState;
    return m_command->state(*m_frame, triggeringEvent);
}

String Editor::Command::value(Event* triggeringEvent) const
{
    if (!isSupported() || !m_frame)
        return String();
    if (m_command->value == valueNull && m_command->state != stateNone)
        return m_command->state(*m_frame, triggeringEvent) == TrueTriState ? "true"_s : "false"_s;
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
