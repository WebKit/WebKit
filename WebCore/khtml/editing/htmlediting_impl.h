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

#ifndef __htmleditingimpl_h__
#define __htmleditingimpl_h__

#include "htmlediting.h"

#include "dom_position.h"
#include "dom_selection.h"
#include "dom_string.h"
#include "dom2_range.h"
#include "qvaluelist.h"
#include "shared.h"

namespace DOM {
    class AtomicString;
    class CSSStyleDeclarationImpl;
    class DocumentImpl;
    class DOMString;
    class ElementImpl;
    class HTMLElementImpl;
    class NodeImpl;
    class NodeListImpl;
    class Position;
    class Range;
    class Selection;
    class TextImpl;
    class TreeWalkerImpl;
};

namespace khtml {

//------------------------------------------------------------------------------------------
// EditCommandImpl

class EditCommandImpl : public SharedCommandImpl
{
public:
	EditCommandImpl(DOM::DocumentImpl *);
	virtual ~EditCommandImpl();

    virtual int commandID() const;
    bool isCompositeStep() const { return parent().notNull(); }
    EditCommand parent() const;
    void setParent(const EditCommand &);

    enum ECommandState { NotApplied, Applied };
    
	void apply();	
	void unapply();
	void reapply();

    virtual void doApply() = 0;
    virtual void doUnapply() = 0;
    virtual void doReapply();  // calls doApply()

    virtual DOM::DocumentImpl * const document() const { return m_document; }

    DOM::Selection startingSelection() const { return m_startingSelection; }
    DOM::Selection endingSelection() const { return m_endingSelection; }
        
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const DOM::Selection &s);
    void setEndingSelection(const DOM::Selection &s);

private:
    DOM::DocumentImpl *m_document;
    ECommandState m_state;
    DOM::Selection m_startingSelection;
    DOM::Selection m_endingSelection;
    EditCommand m_parent;
};

//------------------------------------------------------------------------------------------
// CompositeEditCommandImpl

class CompositeEditCommandImpl : public EditCommandImpl
{
public:
	CompositeEditCommandImpl(DOM::DocumentImpl *);
	virtual ~CompositeEditCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply() = 0;	
	virtual void doUnapply();
	virtual void doReapply();

protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
    void applyCommandToComposite(EditCommand &);
    void deleteCollapsibleWhitespace();
    void deleteCollapsibleWhitespace(const DOM::Selection &selection);
    void deleteKeyPressed();
    void deleteSelection();
    void deleteSelection(const DOM::Selection &selection);
    void deleteText(DOM::TextImpl *node, long offset, long count);
    void inputText(const DOM::DOMString &text);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property);
    void removeNodeAttribute(DOM::ElementImpl *, int attribute);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodeAndPrune(DOM::NodeImpl *removeChild);
    void removeNodePreservingChildren(DOM::NodeImpl *node);
    void replaceText(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText);
    void setNodeAttribute(DOM::ElementImpl *, int attribute, const DOM::DOMString &);
    void splitTextNode(DOM::TextImpl *text, long offset);

    QValueList<EditCommand> m_cmds;
};

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommandImpl

class AppendNodeCommandImpl : public EditCommandImpl
{
public:
    AppendNodeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *parentNode, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommandImpl();

    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    DOM::NodeImpl *parentNode() const { return m_parentNode; }
    DOM::NodeImpl *appendChild() const { return m_appendChild; }

private:
    DOM::NodeImpl *m_parentNode;    
    DOM::NodeImpl *m_appendChild;
};

//------------------------------------------------------------------------------------------
// ApplyStyleCommandImpl

class ApplyStyleCommandImpl : public CompositeEditCommandImpl
{
public:
	ApplyStyleCommandImpl(DOM::DocumentImpl *, ApplyStyleCommand::EStyle);
	virtual ~ApplyStyleCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();

private:
    // style-removal helpers
    enum EUndoable { UNDOABLE, NOTUNDOABLE };
    bool isHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeHTMLStyleNode(DOM::HTMLElementImpl *, EUndoable undoable);
    void removeCSSStyle(DOM::HTMLElementImpl *, EUndoable undoable);
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property, EUndoable undoable);
    void setNodeAttribute(DOM::ElementImpl *, int attribute, const DOM::DOMString &, EUndoable undoable);
    void removeNodeAttribute(DOM::ElementImpl *, int attribute, EUndoable undoable);
    void removeNodePreservingChildren(DOM::NodeImpl *, EUndoable undoable);

    // shared helpers
    bool mustExlicitlyApplyStyle(const DOM::Position &) const;
    DOM::NodeImpl *createExplicitApplyStyleNode() const;
    bool removingStyle() const { return m_removingStyle; }
    bool currentlyHasStyle() const;
    bool currentlyHasStyle(const DOM::Position &) const;
    int cssProperty() const;
     
    // apply-in-place helpers
    bool matchesTargetStyle(bool hasStyle) const;
    DOM::Position positionInsertionPoint(DOM::Position);
    bool splitTextAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    void removeStyle(const DOM::Position &start, const DOM::Position &end);
    void applyStyleIfNeeded(const DOM::Position &);

    // apply using fragment helpers
    DOM::DocumentFragmentImpl *cloneSelection() const;
    void removeStyle(DOM::DocumentFragmentImpl *);
    void applyStyleIfNeeded(DOM::DocumentFragmentImpl *, const DOM::Position &);
    void insertFragment(DOM::DocumentFragmentImpl *, const DOM::Position &);

    // different cases we recognize and treat differently
    void applyInPlace(const DOM::Position &s, const DOM::Position &e);
    void applyUsingFragment();
    
    ApplyStyleCommand::EStyle m_styleConstant;
    bool m_removingStyle;
};

//------------------------------------------------------------------------------------------
// DeleteCollapsibleWhitespaceCommandImpl

class DeleteCollapsibleWhitespaceCommandImpl : public CompositeEditCommandImpl
{ 
public:
	DeleteCollapsibleWhitespaceCommandImpl(DOM::DocumentImpl *document);
	DeleteCollapsibleWhitespaceCommandImpl(DOM::DocumentImpl *document, const DOM::Selection &selection);
    
	virtual ~DeleteCollapsibleWhitespaceCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();

private:
    DOM::Position deleteWhitespace(const DOM::Position &pos);

    DOM::Selection m_selectionToCollapse;
    unsigned long m_charactersDeleted;
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommandImpl

class DeleteSelectionCommandImpl : public CompositeEditCommandImpl
{ 
public:
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document);
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document, const DOM::Selection &selection);
    
	virtual ~DeleteSelectionCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
    
private:
    void deleteDownstreamWS(const DOM::Position &start);
    bool containsOnlyWhitespace(const DOM::Position &start, const DOM::Position &end);
    void joinTextNodesWithSameStyle();

    DOM::Selection m_selectionToDelete;
};

//------------------------------------------------------------------------------------------
// DeleteTextCommandImpl

class DeleteTextCommandImpl : public EditCommandImpl
{
public:
	DeleteTextCommandImpl(DOM::DocumentImpl *document, DOM::TextImpl *node, long offset, long count);
	virtual ~DeleteTextCommandImpl();
	
    virtual int commandID() const;

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
// InputNewlineCommandImpl

class InputNewlineCommandImpl : public CompositeEditCommandImpl
{
public:
    InputNewlineCommandImpl(DOM::DocumentImpl *document);
    virtual ~InputNewlineCommandImpl();

    virtual int commandID() const;

    virtual void doApply();
};

//------------------------------------------------------------------------------------------
// InputTextCommandImpl

class InputTextCommandImpl : public CompositeEditCommandImpl
{
public:
    InputTextCommandImpl(DOM::DocumentImpl *document);
    virtual ~InputTextCommandImpl();

    virtual int commandID() const;

    virtual void doApply();

    void deleteCharacter();
    void input(const DOM::DOMString &text);
    
    unsigned long charactersAdded() const { return m_charactersAdded; }
    
private:
    DOM::Position prepareForTextInsertion(bool adjustDownstream);
    void execute(const DOM::DOMString &text);
    void insertSpace(DOM::TextImpl *textNode, unsigned long offset);

    DOM::TextImpl *m_insertedTextNode;
    unsigned long m_charactersAdded;
};

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommandImpl

class InsertNodeBeforeCommandImpl : public EditCommandImpl
{
public:
    InsertNodeBeforeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
	virtual ~InsertNodeBeforeCommandImpl();

    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    DOM::NodeImpl *insertChild() const { return m_insertChild; }
    DOM::NodeImpl *refChild() const { return m_refChild; }

private:
    DOM::NodeImpl *m_insertChild;
    DOM::NodeImpl *m_refChild; 
};

//------------------------------------------------------------------------------------------
// InsertTextCommandImpl

class InsertTextCommandImpl : public EditCommandImpl
{
public:
	InsertTextCommandImpl(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextCommandImpl();
	
    virtual int commandID() const;

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
// JoinTextNodesCommandImpl

class JoinTextNodesCommandImpl : public EditCommandImpl
{
public:
	JoinTextNodesCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommandImpl();
	
    virtual int commandID() const;

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
// ReplaceSelectionCommandImpl

class ReplaceSelectionCommandImpl : public CompositeEditCommandImpl
{
public:
    ReplaceSelectionCommandImpl(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement=true);
    virtual ~ReplaceSelectionCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();

private:
    DOM::DocumentFragmentImpl *m_fragment;
    bool m_selectReplacement;
};

//------------------------------------------------------------------------------------------
// MoveSelectionCommandImpl

class MoveSelectionCommandImpl : public CompositeEditCommandImpl
{
public:
    MoveSelectionCommandImpl(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position);
    virtual ~MoveSelectionCommandImpl();
    
    virtual int commandID() const;
    
    virtual void doApply();
    
private:
    DOM::DocumentFragmentImpl *m_fragment;
    DOM::Position m_position;
};

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

class RemoveCSSPropertyCommandImpl : public EditCommandImpl
{
public:
	RemoveCSSPropertyCommandImpl(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *, int property);
	virtual ~RemoveCSSPropertyCommandImpl();

    virtual int commandID() const;

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
// RemoveNodeAttributeCommandImpl

class RemoveNodeAttributeCommandImpl : public EditCommandImpl
{
public:
	RemoveNodeAttributeCommandImpl(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute);
	virtual ~RemoveNodeAttributeCommandImpl();

    virtual int commandID() const;

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
// RemoveNodeCommandImpl

class RemoveNodeCommandImpl : public EditCommandImpl
{
public:
	RemoveNodeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodeCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    DOM::NodeImpl *node() const { return m_removeChild; }

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_removeChild;
    DOM::NodeImpl *m_refChild;    
};

//------------------------------------------------------------------------------------------
// RemoveNodeAndPruneCommandImpl

class RemoveNodeAndPruneCommandImpl : public CompositeEditCommandImpl
{
public:
	RemoveNodeAndPruneCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodeAndPruneCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();

    DOM::NodeImpl *node() const { return m_removeChild; }

private:
    DOM::NodeImpl *m_removeChild;
};

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommandImpl

class RemoveNodePreservingChildrenCommandImpl : public CompositeEditCommandImpl
{
public:
	RemoveNodePreservingChildrenCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodePreservingChildrenCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();

    DOM::NodeImpl *node() const { return m_node; }

private:
    DOM::NodeImpl *m_node;
};

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommandImpl

class SetNodeAttributeCommandImpl : public EditCommandImpl
{
public:
	SetNodeAttributeCommandImpl(DOM::DocumentImpl *, DOM::ElementImpl *, DOM::NodeImpl::Id attribute, const DOM::DOMString &value);
	virtual ~SetNodeAttributeCommandImpl();

    virtual int commandID() const;

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
// SplitTextNodeCommandImpl

class SplitTextNodeCommandImpl : public EditCommandImpl
{
public:
	SplitTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommandImpl();
	
    virtual int commandID() const;

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
// TypingCommandImpl

class TypingCommandImpl : public CompositeEditCommandImpl
{
public:
    TypingCommandImpl(DOM::DocumentImpl *document);
    virtual ~TypingCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();

    bool openForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const DOM::DOMString &text);
    void insertNewline();
    void deleteKeyPressed();

private:
    void issueCommandForDeleteKey();
    void removeCommand(const EditCommand &);
    void typingAddedToOpenCommand();
    
    bool m_openForMoreTyping;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif

