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
#include "dom_string.h"
#include "khtml_selection.h"
#include "qvaluelist.h"
#include "shared.h"

class KHTMLSelection;

namespace DOM {
    class DocumentImpl;
    class DOMPosition;
    class DOMString;
    class ElementImpl;
    class NodeImpl;
    class TextImpl;
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

    KHTMLSelection startingSelection() const { return m_startingSelection; }
    KHTMLSelection endingSelection() const { return m_endingSelection; }
        
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const KHTMLSelection &s);
    void setEndingSelection(const KHTMLSelection &s);

    void moveToStartingSelection();
    void moveToEndingSelection();

private:
    DOM::DocumentImpl *m_document;
    ECommandState m_state;
    KHTMLSelection m_startingSelection;
    KHTMLSelection m_endingSelection;
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
    void applyCommandToComposite(EditCommand &);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void appendNode(DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodeAndPrune(DOM::NodeImpl *removeChild);
    void splitTextNode(DOM::TextImpl *text, long offset);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void inputText(const DOM::DOMString &text);
    void deleteText(DOM::TextImpl *node, long offset, long count);
    void replaceText(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText);
    void deleteSelection();
    void deleteSelection(const KHTMLSelection &selection);
    void deleteKeyPressed();
    void deleteCollapsibleWhitespace();
    void deleteCollapsibleWhitespace(const KHTMLSelection &selection);

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
// DeleteCollapsibleWhitespaceCommandImpl

class DeleteCollapsibleWhitespaceCommandImpl : public CompositeEditCommandImpl
{ 
public:
	DeleteCollapsibleWhitespaceCommandImpl(DOM::DocumentImpl *document);
	DeleteCollapsibleWhitespaceCommandImpl(DOM::DocumentImpl *document, const KHTMLSelection &selection);
    
	virtual ~DeleteCollapsibleWhitespaceCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();

private:
    DOM::DOMPosition deleteWhitespace(const DOM::DOMPosition &pos);

    KHTMLSelection m_selectionToCollapse;
    unsigned long m_charactersDeleted;
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommandImpl

class DeleteSelectionCommandImpl : public CompositeEditCommandImpl
{ 
public:
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document);
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document, const KHTMLSelection &selection);
    
	virtual ~DeleteSelectionCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
    
private:
    void deleteDownstreamWS(const DOM::DOMPosition &start);
    void joinTextNodesWithSameStyle();

    KHTMLSelection m_selectionToDelete;
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
    DOM::DOMPosition prepareForTextInsertion(bool adjustDownstream);
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
// PasteMarkupCommandImpl

class PasteMarkupCommandImpl : public CompositeEditCommandImpl
{
public:
    PasteMarkupCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &markupString, const DOM::DOMString &baseURL);
    virtual ~PasteMarkupCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();

    DOM::DOMString markupString() const { return m_markupString; }

private:
    DOM::DOMString m_markupString;
    DOM::DOMString m_baseURL;
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
    
    bool m_openForMoreTyping;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif

