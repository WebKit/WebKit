/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#include <khtml_selection.h>
#include <dom_position.h>
#include <dom_string.h>

#include <qptrlist.h>

class KHTMLSelection;

namespace DOM {
    class DocumentFragmentImpl;
    class DocumentImpl;
    class DOMPosition;
    class DOMString;
    class NodeImpl;
    class TextImpl;
};

namespace khtml {

//------------------------------------------------------------------------------------------
// Edit result codes

enum {
    EditResultOK                 = 0,
    EditResultFailed             = -1,
    EditResultNoActionTaken      = -2,
};

//------------------------------------------------------------------------------------------
// EditStep classes

class EditStep
{
public:
	EditStep(DOM::DocumentImpl *);
	virtual ~EditStep();
	
    enum EditStepState { NOT_APPLIED, APPLIED };
    
	virtual void apply() = 0;	
	virtual void unapply() = 0;
	virtual void reapply();  // calls apply()
        
    DOM::DocumentImpl * const document() const { return m_document; }
    EditStepState state() const { return m_state; }

    const KHTMLSelection &startingSelection() const { return m_startingSelection; }
    const KHTMLSelection &endingSelection() const { return m_endingSelection; }
    
protected:
    void beginApply();
    void endApply();
    void beginUnapply();
    void endUnapply();
    void beginReapply();
    void endReapply();

    void setStartingSelection(const KHTMLSelection &s) { m_startingSelection = s; }
    void setEndingSelection(const KHTMLSelection &s) { m_endingSelection = s; }

private:
    DOM::DocumentImpl *m_document;
    EditStepState m_state;
    KHTMLSelection m_startingSelection;
    KHTMLSelection m_endingSelection;
};

class CompositeEditStep : public EditStep
{
public:
	CompositeEditStep(DOM::DocumentImpl *);
	virtual ~CompositeEditStep();
	
	virtual void apply() = 0;	
	virtual void unapply();
	virtual void reapply();
    
protected:
    //
    // sugary-sweet convenience functions to help create and apply edit steps
    //
    void applyStep(EditStep *step);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void appendNode(DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
    void removeNode(DOM::NodeImpl *removeChild);
    void splitTextNode(DOM::TextImpl *text, long offset);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void deleteText(DOM::TextImpl *node, long offset, long count);
    void moveSelectionTo(const KHTMLSelection &selection);
	void moveSelectionTo(DOM::NodeImpl *, long);
	void moveSelectionTo(const DOM::DOMPosition &);
    void deleteSelection();
    void deleteSelection(const KHTMLSelection &selection);

    QPtrList<EditStep> m_steps;
};

class InsertNodeBeforeStep : public EditStep
{
public:
    InsertNodeBeforeStep(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
	virtual ~InsertNodeBeforeStep();

	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_insertChild;
    DOM::NodeImpl *m_refChild;    
};

class AppendNodeStep : public EditStep
{
public:
    AppendNodeStep(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeStep();

	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_appendChild;
};

class RemoveNodeStep : public EditStep
{
public:
	RemoveNodeStep(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodeStep();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_removeChild;
    DOM::NodeImpl *m_refChild;    
};

class ModifyTextNodeStep : public EditStep
{
public:
    // used by SplitTextNodeStep derived class
	ModifyTextNodeStep(DOM::DocumentImpl *, DOM::TextImpl *, long); 
    // used by JoinTextNodesStep derived class
    ModifyTextNodeStep(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~ModifyTextNodeStep();
	
protected:
    void splitTextNode();
    void joinTextNodes();

    virtual EditStepState joinState() = 0;
    virtual EditStepState splitState() = 0;

    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    long m_offset;
};

class SplitTextNodeStep : public ModifyTextNodeStep
{
public:
	SplitTextNodeStep(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeStep();
	
	virtual void apply();
	virtual void unapply();

    virtual EditStepState joinState() { return APPLIED; }
    virtual EditStepState splitState() { return NOT_APPLIED; }
};

class JoinTextNodesStep : public ModifyTextNodeStep
{
public:
	JoinTextNodesStep(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesStep();
	
	virtual void apply();
	virtual void unapply();

    virtual EditStepState joinState() { return NOT_APPLIED; }
    virtual EditStepState splitState() { return APPLIED; }
};

class InsertTextStep : public EditStep
{
public:
	InsertTextStep(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextStep();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::TextImpl *m_node;
    long m_offset;
    DOM::DOMString m_text;
};

class DeleteTextStep : public EditStep
{
public:
	DeleteTextStep(DOM::DocumentImpl *document, DOM::TextImpl *, long offset, long count);
	virtual ~DeleteTextStep();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::TextImpl *m_node;
    long m_offset;
    long m_count;
    DOM::DOMString m_text;
};

class MoveSelectionToStep : public EditStep
{
public:
	MoveSelectionToStep(DOM::DocumentImpl *document, const KHTMLSelection &selection);
	MoveSelectionToStep(DOM::DocumentImpl *document, DOM::NodeImpl *, long);
	MoveSelectionToStep(DOM::DocumentImpl *document, const DOM::DOMPosition &);
	virtual ~MoveSelectionToStep() {};
	
	virtual void apply();
	virtual void unapply();
};

class DeleteSelectionStep : public CompositeEditStep
{
public:
	DeleteSelectionStep(DOM::DocumentImpl *document);
	DeleteSelectionStep(DOM::DocumentImpl *document, const KHTMLSelection &);
	virtual ~DeleteSelectionStep();
	
	virtual void apply();
};

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public CompositeEditStep
{
public:    
    EditCommand(DOM::DocumentImpl *document);
    virtual ~EditCommand();

    const KHTMLSelection &currentSelection() const;

    virtual void apply() = 0;

    int cookie() const { return m_cookie; }
    
private:
    int m_cookie;
};

class InputTextCommand : public EditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommand() {};
    
    virtual void apply();

    DOM::DOMString text() const { return m_text; }
    bool isLineBreak() const;
    bool isSpace() const;
    
private:
    DOM::DOMString m_text;
};

class DeleteTextCommand : public EditCommand
{
public:
    DeleteTextCommand(DOM::DocumentImpl *document);
    virtual ~DeleteTextCommand() {};
    
    virtual void apply();
};

class PasteHTMLCommand : public EditCommand
{
public:
    PasteHTMLCommand(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommand() {};
    
    virtual void apply();
private:
    DOM::DOMString m_HTMLString;
};

}; // end namespace khtml

#endif
