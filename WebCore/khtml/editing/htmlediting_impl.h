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

#ifndef __htmleditingimpl_h__
#define __htmleditingimpl_h__

#include "htmlediting.h"

#include <khtml_selection.h>
#include <dom_position.h>
#include <dom_string.h>

#include <qvaluelist.h>
#include <shared.h>

class KHTMLSelection;

namespace DOM {
    class DocumentImpl;
    class DOMPosition;
    class DOMString;
    class NodeImpl;
    class TextImpl;
};

namespace khtml {

//------------------------------------------------------------------------------------------
// EditCommandImpl classes

class EditCommandImpl : public SharedCommandImpl
{
public:
	EditCommandImpl(DOM::DocumentImpl *);
	virtual ~EditCommandImpl();

    virtual int commandID() const;
	virtual QString name() const;

    enum ECommandState { NotApplied, Applied };
    
	virtual void apply() = 0;	
	virtual void unapply() = 0;
	virtual void reapply();  // calls apply()

    virtual bool coalesce(const EditCommand &);

    virtual bool groupForUndo(const EditCommand &) const;
    virtual bool groupForRedo(const EditCommand &) const;
            
    virtual DOM::DocumentImpl * const document() const { return m_document; }

    virtual KHTMLSelection startingSelection() const { return m_startingSelection; }
    virtual KHTMLSelection endingSelection() const { return m_endingSelection; }
    virtual KHTMLSelection currentSelection() const;
        
protected:
    void beginApply();
    void endApply();
    void beginUnapply();
    void endUnapply();
    void beginReapply();
    void endReapply();
    
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const KHTMLSelection &s) { m_startingSelection = s; }
    void setEndingSelection(const KHTMLSelection &s) { m_endingSelection = s; }

    void moveToStartingSelection();
    void moveToEndingSelection();

private:
    DOM::DocumentImpl *m_document;
    ECommandState m_state;
    KHTMLSelection m_startingSelection;
    KHTMLSelection m_endingSelection;
};

class CompositeEditCommandImpl : public EditCommandImpl
{
public:
	CompositeEditCommandImpl(DOM::DocumentImpl *);
	virtual ~CompositeEditCommandImpl();
	
	virtual void apply() = 0;	
	virtual void unapply();
	virtual void reapply();
    
protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void applyCommand(EditCommand &);
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

    QValueList<EditCommand> m_cmds;
};

class InsertNodeBeforeCommandImpl : public EditCommandImpl
{
public:
    InsertNodeBeforeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
	virtual ~InsertNodeBeforeCommandImpl();

	virtual void apply();
	virtual void unapply();

    DOM::NodeImpl *insertChild() const { return m_insertChild; }
    DOM::NodeImpl *refChild() const { return m_refChild; }

private:
    DOM::NodeImpl *m_insertChild;
    DOM::NodeImpl *m_refChild; 
};

class AppendNodeCommandImpl : public EditCommandImpl
{
public:
    AppendNodeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommandImpl();

	virtual void apply();
	virtual void unapply();

    DOM::NodeImpl *parent() const { return m_parent; }
    DOM::NodeImpl *appendChild() const { return m_appendChild; }

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_appendChild;
};

class RemoveNodeCommandImpl : public EditCommandImpl
{
public:
	RemoveNodeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodeCommandImpl();
	
	virtual void apply();
	virtual void unapply();

    DOM::NodeImpl *node() const { return m_removeChild; }

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_removeChild;
    DOM::NodeImpl *m_refChild;    
};

class ModifyTextNodeCommandImpl : public EditCommandImpl
{
public:
    // used by SplitTextNodeCommandImpl derived class
	ModifyTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, long); 
    // used by JoinTextNodesCommandImpl derived class
    ModifyTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~ModifyTextNodeCommandImpl();
	
protected:
    void splitTextNode();
    void joinTextNodes();

    virtual ECommandState joinState() = 0;
    virtual ECommandState splitState() = 0;

    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    long m_offset;
};

class SplitTextNodeCommandImpl : public ModifyTextNodeCommandImpl
{
public:
	SplitTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommandImpl();
	
	virtual void apply();
	virtual void unapply();

    virtual ECommandState joinState() { return Applied; }
    virtual ECommandState splitState() { return NotApplied; }

    DOM::TextImpl *node() const { return m_text2; }
    long offset() const { return m_offset; }
};

class JoinTextNodesCommandImpl : public ModifyTextNodeCommandImpl
{
public:
	JoinTextNodesCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommandImpl();
	
	virtual void apply();
	virtual void unapply();

    virtual ECommandState joinState() { return NotApplied; }
    virtual ECommandState splitState() { return Applied; }

    DOM::TextImpl *firstNode() const { return m_text1; }
    DOM::TextImpl *secondNode() const { return m_text2; }
};

class InsertTextCommandImpl : public EditCommandImpl
{
public:
	InsertTextCommandImpl(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextCommandImpl();
	
	virtual void apply();
	virtual void unapply();

    DOM::TextImpl *node() const { return m_node; }
    long offset() const { return m_offset; }
    DOM::DOMString text() const { return m_text; }

private:
    DOM::TextImpl *m_node;
    long m_offset;
    DOM::DOMString m_text;
};

class DeleteTextCommandImpl : public EditCommandImpl
{
public:
	DeleteTextCommandImpl(DOM::DocumentImpl *document, DOM::TextImpl *node, long offset, long count);
	virtual ~DeleteTextCommandImpl();
	
	virtual void apply();
	virtual void unapply();

    DOM::TextImpl *node() const { return m_node; }
    long offset() const { return m_offset; }
    long count() const { return m_count; }

private:
    DOM::TextImpl *m_node;
    long m_offset;
    long m_count;
    DOM::DOMString m_text;
};

class MoveSelectionToCommandImpl : public EditCommandImpl
{
public:
	MoveSelectionToCommandImpl(DOM::DocumentImpl *document, const KHTMLSelection &selection);
	MoveSelectionToCommandImpl(DOM::DocumentImpl *document, DOM::NodeImpl *, long);
	MoveSelectionToCommandImpl(DOM::DocumentImpl *document, const DOM::DOMPosition &);
	virtual ~MoveSelectionToCommandImpl();

	virtual void apply();
	virtual void unapply();
};

class DeleteSelectionCommandImpl : public CompositeEditCommandImpl
{
public:
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document);
	virtual ~DeleteSelectionCommandImpl();
	
	virtual void apply();
};

class InputTextCommandImpl : public CompositeEditCommandImpl
{
public:
    InputTextCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommandImpl();

    virtual int commandID() const;

    virtual void apply();

    virtual bool coalesce(const EditCommand &);

    virtual bool groupForUndo(const EditCommand &) const;
    virtual bool groupForRedo(const EditCommand &) const;

    DOM::DOMString text() const { return m_text; }
    bool isLineBreak(const DOM::DOMString &text) const;
    bool isSpace(const DOM::DOMString &text) const;
    
private:
    void execute(const DOM::DOMString &text);

    DOM::DOMString m_text;
};

class DeleteKeyCommandImpl : public CompositeEditCommandImpl
{
public:
    DeleteKeyCommandImpl(DOM::DocumentImpl *document);
    virtual ~DeleteKeyCommandImpl();
    
    virtual int commandID() const;

    virtual void apply();

    virtual bool groupForUndo(const EditCommand &) const;
    virtual bool groupForRedo(const EditCommand &) const;
};

class PasteHTMLCommandImpl : public CompositeEditCommandImpl
{
public:
    PasteHTMLCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommandImpl();
    
    virtual void apply();

    DOM::DOMString HTMLString() const { return m_HTMLString; }

private:
    DOM::DOMString m_HTMLString;
};

}; // end namespace khtml

#endif

