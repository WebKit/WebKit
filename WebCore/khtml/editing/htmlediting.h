/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef __htmlediting_h__
#define __htmlediting_h__

#include "dom_position.h"
#include "dom_selection.h"
#include "dom_string.h"
#include "shared.h"
#include "xml/dom_nodeimpl.h"

namespace DOM {
    class CSSStyleDeclarationImpl;
    class DocumentFragmentImpl;
    class DocumentImpl;
    class DOMString;
    class ElementImpl;
    class NodeImpl;
    class Position;
    class Selection;
    class TextImpl;
};

namespace khtml {

class AppendNodeCommandImpl;
class ApplyStyleCommandImpl;
class CompositeEditCommandImpl;
class DeleteSelectionCommandImpl;
class DeleteTextCommandImpl;
class EditCommand;
class EditCommandImpl;
class InputNewlineCommandImpl;
class InputTextCommandImpl;
class InsertNodeBeforeCommandImpl;
class InsertTextCommandImpl;
class JoinTextNodesCommandImpl;
class MoveSelectionCommandImpl;
class ReplaceSelectionCommandImpl;
class RemoveCSSPropertyCommandImpl;
class RemoveNodeAttributeCommandImpl;
class RemoveNodeCommandImpl;
class RemoveNodePreservingChildrenCommandImpl;
class SetNodeAttributeCommandImpl;
class SplitTextNodeCommandImpl;
class TypingCommandImpl;

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public SharedPtr<EditCommandImpl>
{
public:
    EditCommand();
    EditCommand(EditCommandImpl *);
    EditCommand(const EditCommand &);
    ~EditCommand();

    EditCommand &operator=(const EditCommand &);

    bool isCompositeStep() const;

    void apply() const;
    void unapply() const;
    void reapply() const;

    DOM::DocumentImpl * const document() const;

    DOM::Selection startingSelection() const;
    DOM::Selection endingSelection() const;

    void setStartingSelection(const DOM::Selection &s) const;
    void setEndingSelection(const DOM::Selection &s) const;

    DOM::CSSStyleDeclarationImpl *typingStyle() const;
    void setTypingStyle(DOM::CSSStyleDeclarationImpl *) const;

    EditCommand parent() const;
    void setParent(const EditCommand &) const;

    bool isInputTextCommand() const;
    bool isInputNewlineCommand() const;
    bool isTypingCommand() const;

    static EditCommand &emptyCommand();
};

//------------------------------------------------------------------------------------------
// CompositeEditCommand

class CompositeEditCommand : public EditCommand
{
protected:
    CompositeEditCommand(CompositeEditCommandImpl *);

private:
    CompositeEditCommandImpl *impl() const;
};

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommand

class AppendNodeCommand : public EditCommand
{
public:
    AppendNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *appendChild, DOM::NodeImpl *parentNode);

    DOM::NodeImpl *appendChild() const;
    DOM::NodeImpl *parentNode() const;
    
private:
    AppendNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// ApplyStyleCommand

class ApplyStyleCommand : public CompositeEditCommand
{
public:
    ApplyStyleCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *);

    DOM::CSSStyleDeclarationImpl *style() const;

private:
    ApplyStyleCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommand

class DeleteSelectionCommand : public CompositeEditCommand
{
public:
    DeleteSelectionCommand(DOM::DocumentImpl *document, bool smartDelete=false);
    DeleteSelectionCommand(DOM::DocumentImpl *document, const DOM::Selection &selection, bool smartDelete=false);

private:
    DeleteSelectionCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// DeleteTextCommand

class DeleteTextCommand : public EditCommand
{
public:
    DeleteTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long offset, long count);

    DOM::TextImpl *node() const;
    long offset() const;
    long count() const;

private:
    DeleteTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InputNewlineCommand

class InputNewlineCommand : public CompositeEditCommand
{
public:
    InputNewlineCommand(DOM::DocumentImpl *document);

private:
    InputNewlineCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InputTextCommand

class InputTextCommand : public CompositeEditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document);

    void deleteCharacter();
    void input(const DOM::DOMString &text, bool selectInsertedText = false);

    unsigned long charactersAdded() const;

private:
    InputTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommand

class InsertNodeBeforeCommand : public EditCommand
{
public:
    InsertNodeBeforeCommand(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);

    DOM::NodeImpl *insertChild() const;
    DOM::NodeImpl *refChild() const;
    
private:
    InsertNodeBeforeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// InsertTextCommand

class InsertTextCommand : public EditCommand
{
public:
    InsertTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);

    DOM::TextImpl *node() const;
    long offset() const;
    DOM::DOMString text() const;

private:
    InsertTextCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// JoinTextNodesCommand

class JoinTextNodesCommand : public EditCommand
{
public:
    JoinTextNodesCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);

    DOM::TextImpl *firstNode() const;
    DOM::TextImpl *secondNode() const;
    
private:
    JoinTextNodesCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// ReplaceSelectionCommand

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement=true, bool smartReplace=false);

private:
    ReplaceSelectionCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// MoveSelectionCommand

class MoveSelectionCommand : public CompositeEditCommand
{
public:
    MoveSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position);
    
private:
    MoveSelectionCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

class RemoveCSSPropertyCommand : public EditCommand
{
public:
    RemoveCSSPropertyCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *, int property);

    DOM::CSSStyleDeclarationImpl *styleDeclaration() const;
    int property() const;
    
private:
    RemoveCSSPropertyCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommand

class RemoveNodeAttributeCommand : public EditCommand
{
public:
    RemoveNodeAttributeCommand(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute);

    DOM::ElementImpl *element() const;
    DOM::NodeImpl::Id attribute() const;
    
private:
    RemoveNodeAttributeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

class RemoveNodeCommand : public EditCommand
{
public:
    RemoveNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *node);

    DOM::NodeImpl *node() const;
    
private:
    RemoveNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommand

class RemoveNodePreservingChildrenCommand : public CompositeEditCommand
{
public:
    RemoveNodePreservingChildrenCommand(DOM::DocumentImpl *document, DOM::NodeImpl *node);

    DOM::NodeImpl *node() const;

private:
    RemoveNodePreservingChildrenCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommand

class SetNodeAttributeCommand : public EditCommand
{
public:
    SetNodeAttributeCommand(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute, const DOM::DOMString &value);

    DOM::ElementImpl *element() const;
    DOM::NodeImpl::Id attribute() const;
    DOM::DOMString value() const;
    
private:
    SetNodeAttributeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

class SplitTextNodeCommand : public EditCommand
{
public:
    SplitTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);

    DOM::TextImpl *node() const;
    long offset() const;
    
private:
    SplitTextNodeCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------
// TypingCommand

class TypingCommand : public CompositeEditCommand
{
public:
    static void deleteKeyPressed(DOM::DocumentImpl *document);
    static void insertText(DOM::DocumentImpl *document, const DOM::DOMString &text, bool selectInsertedText = false);
    static void insertNewline(DOM::DocumentImpl *document);
    static bool isOpenForMoreTypingCommand(const EditCommand &);
    static void closeTyping(const EditCommand &);

    bool openForMoreTyping() const;
    void closeTyping() const;

    enum ETypingCommand { DeleteKey, InsertText, InsertNewline };

private:
    TypingCommand(DOM::DocumentImpl *document, ETypingCommand, const DOM::DOMString &text = "", bool selectInsertedText = false);

    void deleteKeyPressed() const;
    void insertText(const DOM::DOMString &text, bool selectInsertedText = false) const;
    void insertNewline() const;

    TypingCommandImpl *impl() const;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif
