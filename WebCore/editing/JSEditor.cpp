/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Trolltech ASA
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSEditor.h"

#include "CSSPropertyNames.h"
#include "CreateLinkCommand.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "Editor.h"
#include "EditorClient.h"
#include "FormatBlockCommand.h"
#include "Frame.h"
#include "HTMLFontElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "IndentOutdentCommand.h"
#include "InsertListCommand.h"
#include "Page.h"
#include "ReplaceSelectionCommand.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "UnlinkCommand.h"
#include "htmlediting.h"
#include "markup.h"

namespace WebCore {

using namespace HTMLNames;

class Document;

namespace {

struct CommandImp {
    bool (*execFn)(Frame*, bool userInterface, const String& value);
    bool (*enabledFn)(Frame*);
    Frame::TriState (*stateFn)(Frame*);
    String (*valueFn)(Frame*);
};

typedef HashMap<StringImpl*, const CommandImp*, CaseInsensitiveHash<StringImpl*> > CommandMap;

CommandMap* createCommandDictionary();

const CommandImp* commandImp(const String& command)
{
    static CommandMap* commandDictionary = createCommandDictionary();
    return commandDictionary->get(command.impl());
}

} // anonymous namespace

bool JSEditor::execCommand(const String& command, bool userInterface, const String& value)
{
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return false;
    Frame* frame = m_document->frame();
    if (!frame)
        return false;
    m_document->updateLayoutIgnorePendingStylesheets();
    return cmd->enabledFn(frame) && cmd->execFn(frame, userInterface, value);
}

bool JSEditor::queryCommandEnabled(const String& command)
{
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return false;
    Frame* frame = m_document->frame();
    if (!frame)
        return false;
    m_document->updateLayoutIgnorePendingStylesheets();
    return cmd->enabledFn(frame);
}

bool JSEditor::queryCommandIndeterm(const String& command)
{
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return false;
    Frame* frame = m_document->frame();
    if (!frame)
        return false;
    m_document->updateLayoutIgnorePendingStylesheets();
    return cmd->stateFn(frame) == Frame::mixedTriState;
}

bool JSEditor::queryCommandState(const String& command)
{
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return false;
    Frame* frame = m_document->frame();
    if (!frame)
        return false;
    m_document->updateLayoutIgnorePendingStylesheets();
    return cmd->stateFn(frame) != Frame::falseTriState;
}

bool JSEditor::queryCommandSupported(const String& command)
{
    Settings* settings = m_document->settings();
    if ((!settings || !settings->isDOMPasteAllowed()) && command.lower() == "paste")
        return false;
    return commandImp(command) != 0;
}

String JSEditor::queryCommandValue(const String& command)
{
    const CommandImp* cmd = commandImp(command);
    if (!cmd)
        return String();
    Frame* frame = m_document->frame();
    if (!frame)
        return String();
    m_document->updateLayoutIgnorePendingStylesheets();
    return cmd->valueFn(frame);
}

// =============================================================================================

// Private stuff, all inside an anonymous namespace.

namespace {

bool execStyleChange(Frame* frame, int propertyID, const String& propertyValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = new CSSMutableStyleDeclaration;
    style->setProperty(propertyID, propertyValue);
    frame->editor()->applyStyle(style.get());
    return true;
}

bool execStyleChange(Frame* frame, int propertyID, const char* propertyValue)
{
    return execStyleChange(frame, propertyID, String(propertyValue));
}

bool execStyleChange(Frame* frame, int propertyID, int propertyValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = new CSSMutableStyleDeclaration;
    style->setProperty(propertyID, propertyValue);
    frame->editor()->applyStyle(style.get());
    return true;
}

Frame::TriState stateStyle(Frame* frame, int propertyID, const char* desiredValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = new CSSMutableStyleDeclaration;
    style->setProperty(propertyID, desiredValue);
    return frame->selectionHasStyle(style.get());
}

bool selectionStartHasStyle(Frame* frame, int propertyID, const char* desiredValue)
{
    RefPtr<CSSMutableStyleDeclaration> style = new CSSMutableStyleDeclaration;
    style->setProperty(propertyID, desiredValue);
    return frame->editor()->selectionStartHasStyle(style.get());
}

String valueStyle(Frame* frame, int propertyID)
{
    return frame->selectionStartStylePropertyValue(propertyID);
}

// =============================================================================================
//
// execCommand implementations
//

bool execBackColor(Frame* frame, bool, const String& value)
{
    return execStyleChange(frame, CSS_PROP_BACKGROUND_COLOR, value);
}

bool execBold(Frame* frame, bool, const String&)
{
    bool isBold = selectionStartHasStyle(frame, CSS_PROP_FONT_WEIGHT, "bold");
    return execStyleChange(frame, CSS_PROP_FONT_WEIGHT, isBold ? "normal" : "bold");
}

bool execCopy(Frame* frame, bool, const String&)
{
    frame->editor()->copy();
    return true;
}

bool execCreateLink(Frame* frame, bool userInterface, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box to let the user enter a url.
    if (userInterface)
        LOG_ERROR("A dialog box for link creation is not yet implemented.\n");
    
    if (value.isEmpty())
        return false;
    
    applyCommand(new CreateLinkCommand(frame->document(), value));
    return true;
}

bool execCut(Frame* frame, bool, const String&)
{
    frame->editor()->cut();
    return true;
}

bool execDelete(Frame* frame, bool, const String&)
{
    TypingCommand::deleteKeyPressed(frame->document(), frame->selectionGranularity() == WordGranularity);
    return true;
}

bool execFindString(Frame* frame, bool, const String& value)
{
    return frame->findString(value, true, false, true, false);
}

bool execForwardDelete(Frame* frame, bool, const String&)
{
    TypingCommand::forwardDeleteKeyPressed(frame->document());
    return true;
}

bool execFontName(Frame* frame, bool, const String& value)
{
    return execStyleChange(frame, CSS_PROP_FONT_FAMILY, value);
}

bool execFontSize(Frame* frame, bool, const String& value)
{
    int size;
    if (!HTMLFontElement::cssValueFromFontSizeNumber(value, size))
        return false;
        
    return execStyleChange(frame, CSS_PROP_FONT_SIZE, size);
}

bool execFontSizeDelta(Frame* frame, bool, const String& value)
{
    return execStyleChange(frame, CSS_PROP__WEBKIT_FONT_SIZE_DELTA, value);
}

bool execForeColor(Frame* frame, bool, const String& value)
{
    return execStyleChange(frame, CSS_PROP_COLOR, value);
}

bool execFormatBlock(Frame* frame, bool, const String& value)
{
    String tagName = value.lower();
    if (tagName[0] == '<' && tagName[tagName.length() - 1] == '>')
        tagName = tagName.substring(1, tagName.length() - 2);
    if (!validBlockTag(tagName))
        return false;
    applyCommand(new FormatBlockCommand(frame->document(), tagName));
    return true;
}

bool execInsertHorizontalRule(Frame* frame, bool userInterface, const String& value)
{
    RefPtr<HTMLElement> hr = new HTMLElement(hrTag, frame->document());
    hr->setId(value);
    RefPtr<DocumentFragment> fragment = new DocumentFragment(frame->document());
    ExceptionCode ec = 0;
    fragment->appendChild(hr, ec);
    if (ec)
        return false;
    
    applyCommand(new ReplaceSelectionCommand(frame->document(), fragment.release(),
        false, false, false, true, false, EditActionUnspecified));
    return true;
}

bool execInsertHTML(Frame* frame, bool userInterface, const String& value)
{
    Document* document = frame->document();
    applyCommand(new ReplaceSelectionCommand(frame->document(), createFragmentFromMarkup(document, value, ""), false));
    return true;
}

bool execInsertImage(Frame* frame, bool userInterface, const String& value)
{
    // FIXME: If userInterface is true, we should display a dialog box and let the user choose a local image.
    if (userInterface)
        LOG_ERROR("A dialog box for image insertion is not yet implemented.\n");
    
    RefPtr<HTMLImageElement> image = new HTMLImageElement(imgTag, frame->document());
    image->setSrc(value);
    RefPtr<DocumentFragment> fragment = new DocumentFragment(frame->document());
    ExceptionCode ec = 0;
    fragment->appendChild(image, ec);
    if (ec)
        return false;
    
    applyCommand(new ReplaceSelectionCommand(frame->document(), fragment.release(), false));
    return true;
}

bool execIndent(Frame* frame, bool, const String&)
{
    applyCommand(new IndentOutdentCommand(frame->document(), IndentOutdentCommand::Indent));
    return true;
}

bool execInsertLineBreak(Frame* frame, bool, const String&)
{
    TypingCommand::insertLineBreak(frame->document());
    return true;
}

bool execInsertParagraph(Frame* frame, bool, const String&)
{
    TypingCommand::insertParagraphSeparator(frame->document());
    return true;
}

bool execInsertNewlineInQuotedContent(Frame* frame, bool, const String&)
{
    TypingCommand::insertParagraphSeparatorInQuotedContent(frame->document());
    return true;
}

bool execInsertText(Frame* frame, bool, const String& value)
{
    TypingCommand::insertText(frame->document(), value);
    return true;
}

bool execInsertUnorderedList(Frame* frame, bool, const String& value)
{
    applyCommand(new InsertListCommand(frame->document(), InsertListCommand::UnorderedList, value));
    return true;
}

bool execInsertOrderedList(Frame* frame, bool, const String& value)
{
    applyCommand(new InsertListCommand(frame->document(), InsertListCommand::OrderedList, value));
    return true;
}

bool execItalic(Frame* frame, bool, const String&)
{
    bool isItalic = selectionStartHasStyle(frame, CSS_PROP_FONT_STYLE, "italic");
    return execStyleChange(frame, CSS_PROP_FONT_STYLE, isItalic ? "normal" : "italic");
}

bool execJustifyCenter(Frame* frame, bool, const String&)
{
    return execStyleChange(frame, CSS_PROP_TEXT_ALIGN, "center");
}

bool execJustifyFull(Frame* frame, bool, const String&)
{
    return execStyleChange(frame, CSS_PROP_TEXT_ALIGN, "justify");
}

bool execJustifyLeft(Frame* frame, bool, const String&)
{
    return execStyleChange(frame, CSS_PROP_TEXT_ALIGN, "left");
}

bool execJustifyRight(Frame* frame, bool, const String&)
{
    return execStyleChange(frame, CSS_PROP_TEXT_ALIGN, "right");
}

bool execOutdent(Frame* frame, bool, const String&)
{
    applyCommand(new IndentOutdentCommand(frame->document(), IndentOutdentCommand::Outdent));
    return true;
}

bool execPaste(Frame* frame, bool, const String&)
{
    frame->editor()->paste();
    return true;
}

bool execPasteAndMatchStyle(Frame* frame, bool, const String&)
{
    frame->editor()->pasteAsPlainText();
    return true;
}

bool execPrint(Frame* frame, bool, const String&)
{
    if (Page* page = frame->page())
        page->chrome()->print(frame);
    return true;
}

bool execRedo(Frame* frame, bool, const String&)
{
    frame->editor()->redo();
    return true;
}

bool execRemoveFormat(Frame* frame, bool userInterface, const String& value)
{
    frame->editor()->removeFormattingAndStyle();
    return true;
}

bool execSelectAll(Frame* frame, bool, const String&)
{
    frame->selectionController()->selectAll();
    return true;
}

bool execStrikethrough(Frame* frame, bool, const String&)
{
    bool isStrikethrough = selectionStartHasStyle(frame,  CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, "line-through");
    return execStyleChange(frame, CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, isStrikethrough ? "none" : "line-through");
}

bool execSubscript(Frame* frame, bool, const String&)
{
    return execStyleChange(frame,  CSS_PROP_VERTICAL_ALIGN, "sub");
}

bool execSuperscript(Frame* frame, bool, const String&)
{
    return execStyleChange(frame,  CSS_PROP_VERTICAL_ALIGN, "super");
}

bool execTranspose(Frame* frame, bool, const String&)
{
    frame->transpose();
    return true;
}

bool execUnderline(Frame* frame, bool, const String&)
{
    bool isUnderlined = selectionStartHasStyle(frame,  CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, "underline");
    return execStyleChange(frame,  CSS_PROP__WEBKIT_TEXT_DECORATIONS_IN_EFFECT, isUnderlined ? "none" : "underline");
}

bool execUndo(Frame* frame, bool, const String&)
{
    frame->editor()->undo();
    return true;
}

bool execUnlink(Frame* frame, bool, const String&)
{
    applyCommand(new UnlinkCommand(frame->document()));
    return true;
}

bool execUnselect(Frame* frame, bool, const String&)
{
    frame->selectionController()->clear();
    return true;
}

// =============================================================================================
//
// queryCommandEnabled implementations
//
// It's a bit difficult to get a clear notion of the difference between
// "supported" and "enabled" from reading the Microsoft documentation, but
// what little I could glean from that seems to make some sense.
//     Supported = The command is supported by this object.
//     Enabled =   The command is available and enabled.

bool enabled(Frame*)
{
    return true;
}

bool enabledAnyCaret(Frame* frame)
{
    return frame->selectionController()->isCaret() && frame->selectionController()->isContentEditable();
}

bool enabledAnySelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange();
}

bool enabledAnyEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentEditable();
}

bool enabledAnyRichlyEditableSelection(Frame* frame)
{
    return frame->selectionController()->isCaretOrRange() && frame->selectionController()->isContentRichlyEditable();
}

bool enabledCopy(Frame* frame)
{
    return !frame->selectionController()->isInPasswordField();
}

bool enabledPaste(Frame* frame)
{
    Settings* settings = frame ? frame->settings() : 0;
    return settings && settings->isDOMPasteAllowed() && frame->editor()->canPaste();
}

bool enabledAnyRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange();
}

bool enabledAnyEditableRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange() && frame->selectionController()->isContentEditable();
}

bool enabledAnyRichlyEditableRangeSelection(Frame* frame)
{
    return frame->selectionController()->isRange() && frame->selectionController()->isContentRichlyEditable();
}

bool enabledRedo(Frame* frame)
{
    return frame->editor()->canRedo();
}

bool enabledUndo(Frame* frame)
{
    return frame->editor()->canUndo();
}

// =============================================================================================
//
// queryCommandIndeterm/State implementations
//
// It's a bit difficult to get a clear notion of what these methods are supposed
// to do from reading the Microsoft documentation, but my current guess is this:
//
//     queryCommandState and queryCommandIndeterm work in concert to return
//     the two bits of information that are needed to tell, for instance,
//     if the text of a selection is bold. The answer can be "yes", "no", or
//     "partially".
//
// If this is so, then queryCommandState should return "yes" in the case where
// all the text is bold and "no" for non-bold or partially-bold text.
// Then, queryCommandIndeterm should return "no" in the case where
// all the text is either all bold or not-bold and and "yes" for partially-bold text.

Frame::TriState stateNone(Frame*)
{
    return Frame::falseTriState;
}

Frame::TriState stateBold(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_FONT_WEIGHT, "bold");
}

Frame::TriState stateItalic(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_FONT_STYLE, "italic");
}

Frame::TriState stateUnorderedList(Frame* frame)
{
    return frame->editor()->selectionUnorderedListState();
}

Frame::TriState stateOrderedList(Frame* frame)
{
    return frame->editor()->selectionOrderedListState();
}

Frame::TriState stateStrikethrough(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_TEXT_DECORATION, "line-through");
}

Frame::TriState stateSubscript(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_VERTICAL_ALIGN, "sub");
}

Frame::TriState stateSuperscript(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_VERTICAL_ALIGN, "super");
}

Frame::TriState stateUnderline(Frame* frame)
{
    return stateStyle(frame, CSS_PROP_TEXT_DECORATION, "underline");
}

// =============================================================================================
//
// queryCommandValue implementations
//

String valueNull(Frame*)
{
    return String();
}

String valueBackColor(Frame* frame)
{
    return valueStyle(frame, CSS_PROP_BACKGROUND_COLOR);
}

String valueFontName(Frame* frame)
{
    return valueStyle(frame, CSS_PROP_FONT_FAMILY);
}

String valueFontSize(Frame* frame)
{
    return valueStyle(frame, CSS_PROP_FONT_SIZE);
}

String valueFontSizeDelta(Frame* frame)
{
    return valueStyle(frame, CSS_PROP__WEBKIT_FONT_SIZE_DELTA);
}

String valueForeColor(Frame* frame)
{
    return valueStyle(frame, CSS_PROP_COLOR);
}

// =============================================================================================

CommandMap* createCommandDictionary()
{
    struct EditorCommand { const char* name; CommandImp imp; };

    static const EditorCommand commands[] = {

        { "BackColor", { execBackColor, enabledAnyRichlyEditableRangeSelection, stateNone, valueBackColor } },
        { "Bold", { execBold, enabledAnyRichlyEditableSelection, stateBold, valueNull } },
        { "Copy", { execCopy, enabledCopy, stateNone, valueNull } },
        { "CreateLink", { execCreateLink, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "Cut", { execCut, enabledAnyEditableRangeSelection, stateNone, valueNull } },
        { "Delete", { execDelete, enabledAnyEditableSelection, stateNone, valueNull } },
        { "FindString", { execFindString, enabled, stateNone, valueNull } },
        { "FontName", { execFontName, enabledAnySelection, stateNone, valueFontName } },
        { "FontSize", { execFontSize, enabledAnySelection, stateNone, valueFontSize } },
        { "FontSizeDelta", { execFontSizeDelta, enabledAnySelection, stateNone, valueFontSizeDelta } },
        { "ForeColor", { execForeColor, enabledAnySelection, stateNone, valueForeColor } },
        { "FormatBlock", { execFormatBlock, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "ForwardDelete", { execForwardDelete, enabledAnyEditableSelection, stateNone, valueNull } },
        { "HiliteColor", { execBackColor, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "Indent", { execIndent, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "InsertHorizontalRule", { execInsertHorizontalRule, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "InsertHTML", { execInsertHTML, enabledAnyEditableSelection, stateNone, valueNull } },
        { "InsertImage", { execInsertImage, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "InsertLineBreak", { execInsertLineBreak, enabledAnyEditableSelection, stateNone, valueNull } },
        { "InsertOrderedList", { execInsertOrderedList, enabledAnyRichlyEditableSelection, stateOrderedList, valueNull } },
        { "InsertParagraph", { execInsertParagraph, enabledAnyEditableSelection, stateNone, valueNull } },
        { "InsertNewlineInQuotedContent", { execInsertNewlineInQuotedContent, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "InsertText", { execInsertText, enabledAnyEditableSelection, stateNone, valueNull } },
        { "InsertUnorderedList", { execInsertUnorderedList, enabledAnyRichlyEditableSelection, stateUnorderedList, valueNull } },
        { "Italic", { execItalic, enabledAnyRichlyEditableSelection, stateItalic, valueNull } },
        { "JustifyCenter", { execJustifyCenter, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "JustifyFull", { execJustifyFull, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "JustifyLeft", { execJustifyLeft, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "JustifyNone", { execJustifyLeft, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "JustifyRight", { execJustifyRight, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "Outdent", { execOutdent, enabledAnyRichlyEditableSelection, stateNone, valueNull } },
        { "Paste", { execPaste, enabledPaste, stateNone, valueNull } },
        { "PasteAndMatchStyle", { execPasteAndMatchStyle, enabledPaste, stateNone, valueNull } },
        { "Print", { execPrint, enabled, stateNone, valueNull } },
        { "Redo", { execRedo, enabledRedo, stateNone, valueNull } },
        { "RemoveFormat", { execRemoveFormat, enabledAnyEditableRangeSelection, stateNone, valueNull } },
        { "SelectAll", { execSelectAll, enabled, stateNone, valueNull } },
        { "Strikethrough", { execStrikethrough, enabledAnyRichlyEditableSelection, stateStrikethrough, valueNull } },
        { "Subscript", { execSubscript, enabledAnyRichlyEditableSelection, stateSubscript, valueNull } },
        { "Superscript", { execSuperscript, enabledAnyRichlyEditableSelection, stateSuperscript, valueNull } },
        { "Transpose", { execTranspose, enabledAnyCaret, stateNone, valueNull } },
        { "Underline", { execUnderline, enabledAnyRichlyEditableSelection, stateUnderline, valueNull } },
        { "Undo", { execUndo, enabledUndo, stateNone, valueNull } },
        { "Unlink", { execUnlink, enabledAnyRichlyEditableRangeSelection, stateNone, valueNull } },
        { "Unselect", { execUnselect, enabledAnySelection, stateNone, valueNull } }

        //
        // The "unsupported" commands are listed here since they appear in the Microsoft
        // documentation used as the basis for the list.
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
        // Overwrite (not supported)
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
    };

    CommandMap* commandMap = new CommandMap;

    const int numCommands = sizeof(commands) / sizeof(commands[0]);
    for (int i = 0; i < numCommands; ++i) {
        StringImpl *name = new StringImpl(commands[i].name);
        name->ref();
        commandMap->set(name, &commands[i].imp);
    }
    return commandMap;
}

} // anonymous namespace

} // namespace WebCore
