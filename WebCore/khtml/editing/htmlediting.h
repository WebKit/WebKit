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

class AppendNodeCommandImpl;
class CompositeEditCommandImpl;
class DeleteKeyCommandImpl;
class DeleteSelectionCommandImpl;
class DeleteTextCommandImpl;
class EditCommandImpl;
class InputTextCommandImpl;
class InsertNodeBeforeCommandImpl;
class InsertTextCommandImpl;
class JoinTextNodesCommandImpl;
class ModifyTextNodeCommandImpl;
class RemoveNodeCommandImpl;
class MoveSelectionToCommandImpl;
class PasteHTMLCommandImpl;
class SplitTextNodeCommandImpl;

//------------------------------------------------------------------------------------------
// EditCommand classes

enum ECommandID { 
    EditCommandID, // leave the base class first, others in alpha order
    AppendNodeCommandID,
    CompositeEditCommandID,
    DeleteKeyCommandID,
    DeleteSelectionCommandID,
    DeleteTextCommandID,
    InputTextCommandID,
    InsertNodeBeforeCommandID,
    InsertTextCommandID,
    JoinTextNodesCommandID,
    ModifyTextNodeCommandID,
    RemoveNodeCommandID,
    MoveSelectionToCommandID,
    PasteHTMLCommandID,
    SplitTextNodeCommandID,
};

class EditCommand;

class SharedCommandImpl : public Shared<SharedCommandImpl>
{
public:
	SharedCommandImpl() {}
	virtual ~SharedCommandImpl() {}

    virtual int commandID() const = 0;
	virtual QString name() const = 0;

	virtual void apply() = 0;	
	virtual void unapply() = 0;
	virtual void reapply() = 0;

    virtual bool coalesce(const EditCommand &) = 0;

    virtual bool groupForUndo(const EditCommand &) const = 0;
    virtual bool groupForRedo(const EditCommand &) const = 0;
            
    virtual DOM::DocumentImpl * const document() const = 0;

    virtual KHTMLSelection startingSelection() const = 0;
    virtual KHTMLSelection endingSelection() const = 0;
    virtual KHTMLSelection currentSelection() const = 0;
};

class EditCommand : public SharedPtr<SharedCommandImpl>
{
public:
	EditCommand();
	EditCommand(EditCommandImpl *);
	EditCommand(const EditCommand &);
	virtual ~EditCommand();

    int commandID() const;
	QString name() const;

	void apply();	
	void unapply();
	void reapply();

    bool coalesce(const EditCommand &);

    bool groupForUndo(const EditCommand &) const;
    bool groupForRedo(const EditCommand &) const;
            
    DOM::DocumentImpl * const document() const;

    KHTMLSelection startingSelection() const;
    KHTMLSelection endingSelection() const;
    KHTMLSelection currentSelection() const;
};

class CompositeEditCommand : public EditCommand
{
public:
	CompositeEditCommand();
	CompositeEditCommand(CompositeEditCommandImpl *);
	CompositeEditCommand(const CompositeEditCommand &);
	virtual ~CompositeEditCommand();

private:
    inline CompositeEditCommandImpl *impl() const;
};

class InsertNodeBeforeCommand : public EditCommand
{
public:
    InsertNodeBeforeCommand();
    InsertNodeBeforeCommand(DOM::DocumentImpl *, DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    InsertNodeBeforeCommand(const InsertNodeBeforeCommand &);
    virtual ~InsertNodeBeforeCommand();

    DOM::NodeImpl *insertChild() const;
    DOM::NodeImpl *refChild() const;
    
private:
    inline InsertNodeBeforeCommandImpl *impl() const;
};

class AppendNodeCommand : public EditCommand
{
public:
    AppendNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *parent, DOM::NodeImpl *appendChild);
	virtual ~AppendNodeCommand();

    DOM::NodeImpl *parent() const;
    DOM::NodeImpl *appendChild() const;
    
private:
    inline AppendNodeCommandImpl *impl() const;
};

class RemoveNodeCommand : public EditCommand
{
public:
	RemoveNodeCommand(DOM::DocumentImpl *, DOM::NodeImpl *node);
	virtual ~RemoveNodeCommand();

    DOM::NodeImpl *node() const;
    
private:
    inline RemoveNodeCommandImpl *impl() const;
};

class ModifyTextNodeCommand : public EditCommand
{
public:
    ModifyTextNodeCommand(ModifyTextNodeCommandImpl *);
	virtual ~ModifyTextNodeCommand();
};

class SplitTextNodeCommand : public ModifyTextNodeCommand
{
public:
	SplitTextNodeCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);
	virtual ~SplitTextNodeCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    
private:
    inline SplitTextNodeCommandImpl *impl() const;
};

class JoinTextNodesCommand : public ModifyTextNodeCommand
{
public:
	JoinTextNodesCommand(DOM::DocumentImpl *, DOM::TextImpl *, DOM::TextImpl *);
	virtual ~JoinTextNodesCommand();

    DOM::TextImpl *firstNode() const;
    DOM::TextImpl *secondNode() const;
    
private:
    inline JoinTextNodesCommandImpl *impl() const;
};

class InsertTextCommand : public EditCommand
{
public:
	InsertTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
	virtual ~InsertTextCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    DOM::DOMString text() const;

private:
    inline InsertTextCommandImpl *impl() const;
};

class DeleteTextCommand : public EditCommand
{
public:
	DeleteTextCommand(DOM::DocumentImpl *document, DOM::TextImpl *, long offset, long count);
	virtual ~DeleteTextCommand();

    DOM::TextImpl *node() const;
    long offset() const;
    long count() const;

private:
    inline DeleteTextCommandImpl *impl() const;
};

class MoveSelectionToCommand : public EditCommand
{
public:
	MoveSelectionToCommand(DOM::DocumentImpl *document, const KHTMLSelection &selection);
	MoveSelectionToCommand(DOM::DocumentImpl *document, DOM::NodeImpl *, long);
	MoveSelectionToCommand(DOM::DocumentImpl *document, const DOM::DOMPosition &);
	virtual ~MoveSelectionToCommand();

private:
    inline MoveSelectionToCommandImpl *impl() const;
};

class DeleteSelectionCommand : public CompositeEditCommand
{
public:
	DeleteSelectionCommand(DOM::DocumentImpl *document);
	virtual ~DeleteSelectionCommand();

private:
    inline DeleteSelectionCommandImpl *impl() const;
};

class InputTextCommand : public CompositeEditCommand
{
public:
    InputTextCommand(DOM::DocumentImpl *document, const DOM::DOMString &text);
    virtual ~InputTextCommand();

    DOM::DOMString text() const;

private:
    inline InputTextCommandImpl *impl() const;
};

class DeleteKeyCommand : public CompositeEditCommand
{
public:
    DeleteKeyCommand(DOM::DocumentImpl *document);
    virtual ~DeleteKeyCommand();

private:
    inline DeleteKeyCommandImpl *impl() const;
};

class PasteHTMLCommand : public CompositeEditCommand
{
public:
    PasteHTMLCommand(DOM::DocumentImpl *document, const DOM::DOMString &HTMLString);
    virtual ~PasteHTMLCommand();

    DOM::DOMString HTMLString() const;

private:
    inline PasteHTMLCommandImpl *impl() const;
};

}; // end namespace khtml

#endif
