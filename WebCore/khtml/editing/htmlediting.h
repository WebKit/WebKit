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

#include "dom_nodeimpl.h"
#include "qvaluelist.h"
#include "selection.h"
#include "shared.h"

namespace DOM {
    class CSSProperty;
    class DocumentFragmentImpl;
    class HTMLElementImpl;
    class TextImpl;
}

namespace khtml {

class EditCommand;
class Selection;

//------------------------------------------------------------------------------------------
// EditCommandPtr

class EditCommandPtr : public SharedPtr<EditCommand>
{
public:
    EditCommandPtr();
    EditCommandPtr(EditCommand *);
    EditCommandPtr(const EditCommandPtr &);
    ~EditCommandPtr();

    EditCommandPtr &operator=(const EditCommandPtr &);

    bool isCompositeStep() const;

    void apply() const;
    void unapply() const;
    void reapply() const;

    DOM::DocumentImpl * const document() const;

    khtml::Selection startingSelection() const;
    khtml::Selection endingSelection() const;

    void setStartingSelection(const khtml::Selection &s) const;
    void setEndingSelection(const khtml::Selection &s) const;

    DOM::CSSStyleDeclarationImpl *typingStyle() const;
    void setTypingStyle(DOM::CSSStyleDeclarationImpl *) const;

    EditCommandPtr parent() const;
    void setParent(const EditCommandPtr &) const;

    bool isInputTextCommand() const;
    bool isInputNewlineCommand() const;
    bool isTypingCommand() const;

    static EditCommandPtr &emptyCommand();
};

//------------------------------------------------------------------------------------------
// StyleChange

class StyleChange {
public:
    StyleChange() : m_applyBold(false), m_applyItalic(false) { }
    explicit StyleChange(DOM::CSSStyleDeclarationImpl *);
    StyleChange(DOM::CSSStyleDeclarationImpl *, const DOM::Position &);

    DOM::DOMString cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }

private:
    void init(DOM::CSSStyleDeclarationImpl *, const DOM::Position &);
    static bool currentlyHasStyle(const DOM::Position &, const DOM::CSSProperty *);
    
    DOM::DOMString m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
};

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public Shared<EditCommand>
{
public:
    EditCommand(DOM::DocumentImpl *);
    virtual ~EditCommand();

    bool isCompositeStep() const { return m_parent.notNull(); }
    EditCommand *parent() const { return m_parent.get(); }
    void setParent(EditCommand *parent) { m_parent = parent; }

    enum ECommandState { NotApplied, Applied };
    
    void apply();	
    void unapply();
    void reapply();

    virtual void doApply() = 0;
    virtual void doUnapply() = 0;
    virtual void doReapply();  // calls doApply()

    virtual DOM::DocumentImpl * const document() const { return m_document; }

    khtml::Selection startingSelection() const { return m_startingSelection; }
    khtml::Selection endingSelection() const { return m_endingSelection; }
        
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const khtml::Selection &s);
    void setEndingSelection(const khtml::Selection &s);

    DOM::CSSStyleDeclarationImpl *typingStyle() const { return m_typingStyle; };
    void setTypingStyle(DOM::CSSStyleDeclarationImpl *);
    
    virtual bool isInputTextCommand() const;
    virtual bool isTypingCommand() const;

private:
    void assignTypingStyle(DOM::CSSStyleDeclarationImpl *);

    virtual bool preservesTypingStyle() const;

    DOM::DocumentImpl *m_document;
    ECommandState m_state;
    khtml::Selection m_startingSelection;
    khtml::Selection m_endingSelection;
    DOM::CSSStyleDeclarationImpl *m_typingStyle;
    EditCommandPtr m_parent;
};

//------------------------------------------------------------------------------------------
// CompositeEditCommand

class CompositeEditCommand : public EditCommand
{
public:
    CompositeEditCommand(DOM::DocumentImpl *);
	
    virtual void doUnapply();
    virtual void doReapply();

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(DOM::NodeImpl *appendChild, DOM::NodeImpl *parentNode);
    void applyCommandToComposite(EditCommandPtr &);
    void deleteKeyPressed();
    void deleteSelection(bool smartDelete=false);
    void deleteSelection(const khtml::Selection &selection, bool smartDelete=false);
    void deleteText(DOM::TextImpl *node, long offset, long count);
    void inputText(const DOM::DOMString &text, bool selectInsertedText = false);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property);
    void removeFullySelectedNode(DOM::NodeImpl *);
    void removeNodeAttribute(DOM::ElementImpl *, int attribute);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodePreservingChildren(DOM::NodeImpl *node);
    void replaceText(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText);
    void setNodeAttribute(DOM::ElementImpl *, int attribute, const DOM::DOMString &);
    void splitTextNode(DOM::TextImpl *text, long offset);

    DOM::NodeImpl *applyTypingStyle(DOM::NodeImpl *) const;

    void deleteInsignificantText(DOM::TextImpl *, int start, int end);
    void deleteInsignificantText(const DOM::Position &start, const DOM::Position &end);
    void deleteInsignificantTextDownstream(const DOM::Position &);

    void insertBlockPlaceholderIfNeeded(DOM::NodeImpl *);
    void removeBlockPlaceholderIfNeeded(DOM::NodeImpl *);

    QValueList<EditCommandPtr> m_cmds;
};

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommand

class AppendNodeCommand : public EditCommand
{
public:
    AppendNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *appendChild, DOM::NodeImpl *parentNode);
    virtual ~AppendNodeCommand();

    virtual void doApply();
    virtual void doUnapply();

    DOM::NodeImpl *appendChild() const { return m_appendChild; }
    DOM::NodeImpl *parentNode() const { return m_parentNode; }

private:
    DOM::NodeImpl *m_appendChild;
    DOM::NodeImpl *m_parentNode;    
};

//------------------------------------------------------------------------------------------
// ApplyStyleCommand

class ApplyStyleCommand : public CompositeEditCommand
{
public:
    ApplyStyleCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *style);
    virtual ~ApplyStyleCommand();
	
    virtual void doApply();

    DOM::CSSStyleDeclarationImpl *style() const { return m_style; }

private:
    // style-removal helpers
    bool isHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeCSSStyle(DOM::HTMLElementImpl *);
    void removeStyle(const DOM::Position &start, const DOM::Position &end);
    bool nodeFullySelected(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end) const;

    // style-application helpers
    bool splitTextAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    DOM::NodeImpl *splitTextAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    void surroundNodeRangeWithElement(DOM::NodeImpl *start, DOM::NodeImpl *end, DOM::ElementImpl *element);
    DOM::Position positionInsertionPoint(DOM::Position);
    void applyStyleIfNeeded(DOM::NodeImpl *start, DOM::NodeImpl *end);
    
    DOM::CSSStyleDeclarationImpl *m_style;
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommand

class DeleteSelectionCommand : public CompositeEditCommand
{ 
public:
    DeleteSelectionCommand(DOM::DocumentImpl *document, bool smartDelete=false);
    DeleteSelectionCommand(DOM::DocumentImpl *document, const khtml::Selection &selection, bool smartDelete=false);
	
    virtual void doApply();
    
private:
    virtual bool preservesTypingStyle() const;

    void deleteDownstreamWS(const DOM::Position &start);
    bool containsOnlyWhitespace(const DOM::Position &start, const DOM::Position &end);
    void moveNodesAfterNode(DOM::NodeImpl *startNode, DOM::NodeImpl *dstNode);

    khtml::Selection m_selectionToDelete;
    bool m_hasSelectionToDelete;
    bool m_smartDelete;
};

//------------------------------------------------------------------------------------------
// DeleteTextCommand

class DeleteTextCommand : public EditCommand
{
public:
    DeleteTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *node, long offset, long count);
    virtual ~DeleteTextCommand();
	
    virtual void doApply();
    virtual void doUnapply();

    DOM::TextImpl *node() const { return m_node; }
    long offset() const { return m_offset; }
    long count() const { return m_count; }

private:
    DOM::TextImpl *m_node;
    long m_offset;
    long m_count;
    DOM::DOMString m_text;
};

//------------------------------------------------------------------------------------------
// InputNewlineCommand

class InputNewlineCommand : public CompositeEditCommand
{
public:
    InputNewlineCommand(DOM::DocumentImpl *document);

    virtual void doApply();

private:
    void insertNodeAfterPosition(DOM::NodeImpl *node, const DOM::Position &pos);
    void insertNodeBeforePosition(DOM::NodeImpl *node, const DOM::Position &pos);
};

//------------------------------------------------------------------------------------------
// InputTextCommand

class InputTextCommand : public CompositeEditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document);

    virtual void doApply();

    void deleteCharacter();
    void input(const DOM::DOMString &text, bool selectInsertedText = false);
    
    unsigned long charactersAdded() const { return m_charactersAdded; }
    
private:
    virtual bool isInputTextCommand() const;

    DOM::Position prepareForTextInsertion(bool adjustDownstream);
    void insertSpace(DOM::TextImpl *textNode, unsigned long offset);

    unsigned long m_charactersAdded;
};

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommand

class InsertNodeBeforeCommand : public EditCommand
{
public:
    InsertNodeBeforeCommand(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    virtual ~InsertNodeBeforeCommand();

    virtual void doApply();
    virtual void doUnapply();

    DOM::NodeImpl *insertChild() const { return m_insertChild; }
    DOM::NodeImpl *refChild() const { return m_refChild; }

private:
    DOM::NodeImpl *m_insertChild;
    DOM::NodeImpl *m_refChild; 
};

//------------------------------------------------------------------------------------------
// InsertTextCommand

class InsertTextCommand : public EditCommand
{
public:
    InsertTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
    virtual ~InsertTextCommand();
	
    virtual void doApply();
    virtual void doUnapply();

    DOM::TextImpl *node() const { return m_node; }
    long offset() const { return m_offset; }
    DOM::DOMString text() const { return m_text; }

private:
    DOM::TextImpl *m_node;
    long m_offset;
    DOM::DOMString m_text;
};

//------------------------------------------------------------------------------------------
// JoinTextNodesCommand

class JoinTextNodesCommand : public EditCommand
{
public:
    JoinTextNodesCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
    virtual ~JoinTextNodesCommand();
	
    virtual void doApply();
    virtual void doUnapply();

    DOM::TextImpl *firstNode() const { return m_text1; }
    DOM::TextImpl *secondNode() const { return m_text2; }

private:
    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    unsigned long m_offset;
};

//------------------------------------------------------------------------------------------
// ReplaceSelectionCommand

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement=true, bool smartReplace=false);
    virtual ~ReplaceSelectionCommand();
    
    virtual void doApply();

private:
    DOM::DocumentFragmentImpl *m_fragment;
    bool m_selectReplacement;
    bool m_smartReplace;
};

//------------------------------------------------------------------------------------------
// MoveSelectionCommand

class MoveSelectionCommand : public CompositeEditCommand
{
public:
    MoveSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position, bool smartMove=false);
    virtual ~MoveSelectionCommand();
    
    virtual void doApply();
    
private:
    DOM::DocumentFragmentImpl *m_fragment;
    DOM::Position m_position;
    bool m_smartMove;
};

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

class RemoveCSSPropertyCommand : public EditCommand
{
public:
    RemoveCSSPropertyCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *, int property);
    virtual ~RemoveCSSPropertyCommand();

    virtual void doApply();
    virtual void doUnapply();

    DOM::CSSStyleDeclarationImpl *styleDeclaration() const { return m_decl; }
    int property() const { return m_property; }
    
private:
    DOM::CSSStyleDeclarationImpl *m_decl;
    int m_property;
    DOM::DOMString m_oldValue;
    bool m_important;
};

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommand

class RemoveNodeAttributeCommand : public EditCommand
{
public:
    RemoveNodeAttributeCommand(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute);
    virtual ~RemoveNodeAttributeCommand();

    virtual void doApply();
    virtual void doUnapply();

    DOM::ElementImpl *element() const { return m_element; }
    DOM::NodeImpl::Id attribute() const { return m_attribute; }
    
private:
    DOM::ElementImpl *m_element;
    DOM::NodeImpl::Id m_attribute;
    DOM::DOMString m_oldValue;
};

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

class RemoveNodeCommand : public EditCommand
{
public:
    RemoveNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *);
    virtual ~RemoveNodeCommand();
	
    virtual void doApply();
    virtual void doUnapply();

    DOM::NodeImpl *node() const { return m_removeChild; }

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_removeChild;
    DOM::NodeImpl *m_refChild;    
};

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommand

class RemoveNodePreservingChildrenCommand : public CompositeEditCommand
{
public:
    RemoveNodePreservingChildrenCommand(DOM::DocumentImpl *, DOM::NodeImpl *);
    virtual ~RemoveNodePreservingChildrenCommand();
	
    virtual void doApply();

    DOM::NodeImpl *node() const { return m_node; }

private:
    DOM::NodeImpl *m_node;
};

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommand

class SetNodeAttributeCommand : public EditCommand
{
public:
    SetNodeAttributeCommand(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute, const DOM::DOMString &value);
    virtual ~SetNodeAttributeCommand();

    virtual void doApply();
    virtual void doUnapply();

    DOM::ElementImpl *element() const { return m_element; }
    DOM::NodeImpl::Id attribute() const { return m_attribute; }
    DOM::DOMString value() const { return m_value; }
    
private:
    DOM::ElementImpl *m_element;
    DOM::NodeImpl::Id m_attribute;
    DOM::DOMString m_value;
    DOM::DOMString m_oldValue;
};

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

class SplitTextNodeCommand : public EditCommand
{
public:
    SplitTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);
    virtual ~SplitTextNodeCommand();
	
    virtual void doApply();
    virtual void doUnapply();

    DOM::TextImpl *node() const { return m_text2; }
    long offset() const { return m_offset; }

private:
    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    unsigned long m_offset;
};

//------------------------------------------------------------------------------------------
// TypingCommand

class TypingCommand : public CompositeEditCommand
{
public:
    enum ETypingCommand { DeleteKey, InsertText, InsertNewline };

    TypingCommand(DOM::DocumentImpl *document, ETypingCommand, const DOM::DOMString &text = "", bool selectInsertedText = false);

    static void deleteKeyPressed(DOM::DocumentImpl *document);
    static void insertText(DOM::DocumentImpl *document, const DOM::DOMString &text, bool selectInsertedText = false);
    static void insertNewline(DOM::DocumentImpl *document);
    static bool isOpenForMoreTypingCommand(const EditCommandPtr &);
    static void closeTyping(const EditCommandPtr &);
    
    virtual void doApply();

    bool openForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const DOM::DOMString &text, bool selectInsertedText);
    void insertNewline();
    void deleteKeyPressed();

private:
    virtual bool isTypingCommand() const;
    virtual bool preservesTypingStyle() const;

    void issueCommandForDeleteKey();
    void removeCommand(const EditCommandPtr &);
    void markMisspellingsAfterTyping();
    void typingAddedToOpenCommand();
    
    ETypingCommand m_commandType;
    DOM::DOMString m_textToInsert;
    bool m_openForMoreTyping;
    bool m_applyEditing;
    bool m_selectInsertedText;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif
