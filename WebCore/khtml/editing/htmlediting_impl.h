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
    bool isCompositeStep() const { return m_isCompositeStep; }
    void setIsCompositeStep(bool flag=true) { m_isCompositeStep = flag; }

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
    KHTMLSelection currentSelection() const;
        
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
    bool m_isCompositeStep;
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
    void splitTextNode(DOM::TextImpl *text, long offset);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void insertText(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void deleteText(DOM::TextImpl *node, long offset, long count);
    void deleteSelection();
    void deleteKeyPressed();

    QValueList<EditCommand> m_cmds;
};

//------------------------------------------------------------------------------------------
// ModifyTextNodeCommandImpl

class ModifyTextNodeCommandImpl : public EditCommandImpl
{
public:
    // used by SplitTextNodeCommandImpl derived class
	ModifyTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, long); 
    // used by JoinTextNodesCommandImpl derived class
    ModifyTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~ModifyTextNodeCommandImpl();
	
    virtual int commandID() const;

protected:
    void splitTextNode();
    void joinTextNodes();

    virtual ECommandState joinState() = 0;
    virtual ECommandState splitState() = 0;

    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    long m_offset;
};

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommandImpl

class AppendNodeCommandImpl : public EditCommandImpl
{
public:
    AppendNodeCommandImpl(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommandImpl();

    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    DOM::NodeImpl *parent() const { return m_parent; }
    DOM::NodeImpl *appendChild() const { return m_appendChild; }

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_appendChild;
};

//------------------------------------------------------------------------------------------
// DeleteKeyCommandImpl

class DeleteKeyCommandImpl : public CompositeEditCommandImpl
{
public:
    DeleteKeyCommandImpl(DOM::DocumentImpl *document);
    virtual ~DeleteKeyCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();
};

//------------------------------------------------------------------------------------------
// DeleteSelectionCommandImpl

class DeleteSelectionCommandImpl : public CompositeEditCommandImpl
{
public:
	DeleteSelectionCommandImpl(DOM::DocumentImpl *document);
	virtual ~DeleteSelectionCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
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
    InputTextCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommandImpl();

    virtual int commandID() const;

    virtual void doApply();

    DOM::DOMString text() const { return m_text; }

    void deleteCharacter();
    void coalesce(const DOM::DOMString &text);
    
private:
    void execute(const DOM::DOMString &text);

    DOM::DOMString m_text;
    DOM::DOMString nbsp;
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

class JoinTextNodesCommandImpl : public ModifyTextNodeCommandImpl
{
public:
	JoinTextNodesCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    virtual ECommandState joinState() { return NotApplied; }
    virtual ECommandState splitState() { return Applied; }

    DOM::TextImpl *firstNode() const { return m_text1; }
    DOM::TextImpl *secondNode() const { return m_text2; }
};

//------------------------------------------------------------------------------------------
// PasteHTMLCommandImpl

class PasteHTMLCommandImpl : public CompositeEditCommandImpl
{
public:
    PasteHTMLCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();

    DOM::DOMString HTMLString() const { return m_HTMLString; }

private:
    DOM::DOMString m_HTMLString;
};

//------------------------------------------------------------------------------------------
// PasteImageCommandImpl

class PasteImageCommandImpl : public CompositeEditCommandImpl
{
public:
    PasteImageCommandImpl(DOM::DocumentImpl *document, const DOM::DOMString &src);
    virtual ~PasteImageCommandImpl();
    
    virtual int commandID() const;

    virtual void doApply();
    
private:
    DOM::DOMString m_src;
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
// SplitTextNodeCommandImpl

class SplitTextNodeCommandImpl : public ModifyTextNodeCommandImpl
{
public:
	SplitTextNodeCommandImpl(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommandImpl();
	
    virtual int commandID() const;

	virtual void doApply();
	virtual void doUnapply();

    virtual ECommandState joinState() { return Applied; }
    virtual ECommandState splitState() { return NotApplied; }

    DOM::TextImpl *node() const { return m_text2; }
    long offset() const { return m_offset; }
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
    void removeCommand(const EditCommand &);
    
    bool m_openForMoreTyping;
};

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif

