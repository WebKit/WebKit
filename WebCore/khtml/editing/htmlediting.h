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

#include <qvaluelist.h>
#include <shared.h>

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
// EditCommand classes

class EditCommand;

typedef SharedPtr<EditCommand> EditCommandPtr;

enum EditCommandID { 
    EditCommandID, 
    InputTextCommandID, 
    DeleteKeyCommandID,
    MoveSelectionToCommandID, 
};

class EditCommand : public Shared<EditCommand>
{
public:
	EditCommand(DOM::DocumentImpl *);
	virtual ~EditCommand();

    virtual int commandID() const;
	QString name() const;

    enum EditCommandState { NOT_APPLIED, APPLIED };
    
	virtual void apply() = 0;	
	virtual void unapply() = 0;
	virtual void reapply();  // calls apply()

    virtual bool coalesce(const EditCommandPtr &);

    virtual bool groupForUndo(const EditCommandPtr &) const;
    virtual bool groupForRedo(const EditCommandPtr &) const;
            
    DOM::DocumentImpl * const document() const { return m_document; }

    const KHTMLSelection &startingSelection() const { return m_startingSelection; }
    const KHTMLSelection &endingSelection() const { return m_endingSelection; }
    const KHTMLSelection &currentSelection() const;
        
protected:
    void beginApply();
    void endApply();
    void beginUnapply();
    void endUnapply();
    void beginReapply();
    void endReapply();
    
    EditCommandState state() const { return m_state; }
    void setState(EditCommandState state) { m_state = state; }

    void setStartingSelection(const KHTMLSelection &s) { m_startingSelection = s; }
    void setEndingSelection(const KHTMLSelection &s) { m_endingSelection = s; }

    void moveToStartingSelection();
    void moveToEndingSelection();

private:
    DOM::DocumentImpl *m_document;
    EditCommandState m_state;
    KHTMLSelection m_startingSelection;
    KHTMLSelection m_endingSelection;
};

class CompositeEditCommand : public EditCommand
{
public:
	CompositeEditCommand(DOM::DocumentImpl *);
	virtual ~CompositeEditCommand();
	
	virtual void apply() = 0;	
	virtual void unapply();
	virtual void reapply();
    
protected:
    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void applyCommand(const EditCommandPtr &step);
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

    QValueList<EditCommandPtr> m_cmds;
};

class InsertNodeBeforeCommand : public EditCommand
{
public:
    InsertNodeBeforeCommand(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
	virtual ~InsertNodeBeforeCommand();

	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_insertChild;
    DOM::NodeImpl *m_refChild;    
};

class AppendNodeCommand : public EditCommand
{
public:
    AppendNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommand();

	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_appendChild;
};

class RemoveNodeCommand : public EditCommand
{
public:
	RemoveNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *);
	virtual ~RemoveNodeCommand();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::NodeImpl *m_parent;    
    DOM::NodeImpl *m_removeChild;
    DOM::NodeImpl *m_refChild;    
};

class ModifyTextNodeCommand : public EditCommand
{
public:
    // used by SplitTextNodeCommand derived class
	ModifyTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long); 
    // used by JoinTextNodesCommand derived class
    ModifyTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~ModifyTextNodeCommand();
	
protected:
    void splitTextNode();
    void joinTextNodes();

    virtual EditCommandState joinState() = 0;
    virtual EditCommandState splitState() = 0;

    DOM::TextImpl *m_text1;
    DOM::TextImpl *m_text2;
    long m_offset;
};

class SplitTextNodeCommand : public ModifyTextNodeCommand
{
public:
	SplitTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommand();
	
	virtual void apply();
	virtual void unapply();

    virtual EditCommandState joinState() { return APPLIED; }
    virtual EditCommandState splitState() { return NOT_APPLIED; }
};

class JoinTextNodesCommand : public ModifyTextNodeCommand
{
public:
	JoinTextNodesCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommand();
	
	virtual void apply();
	virtual void unapply();

    virtual EditCommandState joinState() { return NOT_APPLIED; }
    virtual EditCommandState splitState() { return APPLIED; }
};

class InsertTextCommand : public EditCommand
{
public:
	InsertTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextCommand();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::TextImpl *m_node;
    long m_offset;
    DOM::DOMString m_text;
};

class DeleteTextCommand : public EditCommand
{
public:
	DeleteTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long offset, long count);
	virtual ~DeleteTextCommand();
	
	virtual void apply();
	virtual void unapply();

private:
    DOM::TextImpl *m_node;
    long m_offset;
    long m_count;
    DOM::DOMString m_text;
};

class MoveSelectionToCommand : public EditCommand
{
public:
	MoveSelectionToCommand(DOM::DocumentImpl *document, const KHTMLSelection &selection);
	MoveSelectionToCommand(DOM::DocumentImpl *document, DOM::NodeImpl *, long);
	MoveSelectionToCommand(DOM::DocumentImpl *document, const DOM::DOMPosition &);
	virtual ~MoveSelectionToCommand();

    virtual int commandID() const;
	
	virtual void apply();
	virtual void unapply();
};

class DeleteSelectionCommand : public CompositeEditCommand
{
public:
	DeleteSelectionCommand(DOM::DocumentImpl *document);
	DeleteSelectionCommand(DOM::DocumentImpl *document, const KHTMLSelection &);
	virtual ~DeleteSelectionCommand();
	
	virtual void apply();
};

class InputTextCommand : public CompositeEditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommand();

    virtual int commandID() const;

    virtual void apply();

    virtual bool coalesce(const EditCommandPtr &);

    virtual bool groupForUndo(const EditCommandPtr &) const;
    virtual bool groupForRedo(const EditCommandPtr &) const;

    DOM::DOMString text() const { return m_text; }
    bool isLineBreak(const DOM::DOMString &text) const;
    bool isSpace(const DOM::DOMString &text) const;
    
private:
    void execute(const DOM::DOMString &text);

    DOM::DOMString m_text;
};

class DeleteKeyCommand : public CompositeEditCommand
{
public:
    DeleteKeyCommand(DOM::DocumentImpl *document);
    virtual ~DeleteKeyCommand();
    
    virtual int commandID() const;

    virtual void apply();

    virtual bool groupForUndo(const EditCommandPtr &) const;
    virtual bool groupForRedo(const EditCommandPtr &) const;
};

class PasteHTMLCommand : public CompositeEditCommand
{
public:
    PasteHTMLCommand(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommand();
    
    virtual void apply();

private:
    DOM::DOMString m_HTMLString;
};

}; // end namespace khtml

#endif
