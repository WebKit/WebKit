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

#include "htmlediting.h"
#include "htmlediting_impl.h"

#include "khtml_part.h"
#include "dom/dom_doc.h"
#include "dom/dom_position.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_selection.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#include "KWQLogging.h"
#endif

using DOM::CSSStyleDeclarationImpl;
using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::Position;
using DOM::DOMString;
using DOM::NodeImpl;
using DOM::NodeListImpl;
using DOM::Selection;
using DOM::TextImpl;

#if !APPLE_CHANGES
#define ASSERT(assertion) ((void)0)
#define ASSERT_WITH_MESSAGE(assertion, formatAndArgs...) ((void)0)
#define ASSERT_NOT_REACHED() ((void)0)
#define LOG(channel, formatAndArgs...) ((void)0)
#define ERROR(formatAndArgs...) ((void)0)
#endif

#define IF_IMPL_NULL_RETURN_ARG(arg) do { \
        if (isNull()) { return arg; } \
    } while (0)
        
#define IF_IMPL_NULL_RETURN do { \
        if (isNull()) { return; } \
    } while (0)


namespace khtml {

//------------------------------------------------------------------------------------------
// EditCommand

EditCommand::EditCommand() : SharedPtr<SharedCommandImpl>()
{
}

EditCommand::EditCommand(EditCommandImpl *impl) : SharedPtr<SharedCommandImpl>(impl)
{
}

EditCommand::EditCommand(const EditCommand &o) : SharedPtr<SharedCommandImpl>(o.get())
{
}

EditCommand::~EditCommand()
{
}

int EditCommand::commandID() const
{
    IF_IMPL_NULL_RETURN_ARG(0);        
    return get()->commandID();
}

bool EditCommand::isCompositeStep() const
{
    IF_IMPL_NULL_RETURN_ARG(false);        
    return get()->isCompositeStep();
}

bool EditCommand::isNull() const
{
    return get() == 0;
}

bool EditCommand::notNull() const
{
    return !isNull();
}

void EditCommand::apply()
{
    IF_IMPL_NULL_RETURN;
    get()->apply();
}

void EditCommand::unapply()
{
    IF_IMPL_NULL_RETURN;
    get()->unapply();
}

void EditCommand::reapply()
{
    IF_IMPL_NULL_RETURN;
    get()->reapply();
}

DocumentImpl * const EditCommand::document() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->document();
}

Selection EditCommand::startingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->startingSelection();
}

Selection EditCommand::endingSelection() const
{
    IF_IMPL_NULL_RETURN_ARG(Selection());
    return get()->endingSelection();
}

void EditCommand::setStartingSelection(const Selection &s)
{
    IF_IMPL_NULL_RETURN;
    get()->setStartingSelection(s);
}

void EditCommand::setEndingSelection(const Selection &s)
{
    IF_IMPL_NULL_RETURN;
    get()->setEndingSelection(s);
}

EditCommand EditCommand::parent() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return get()->parent();
}

void EditCommand::setParent(const EditCommand &cmd)
{
    IF_IMPL_NULL_RETURN;
    get()->setParent(cmd);
}

EditCommandImpl *EditCommand::handle() const
{
    return static_cast<EditCommandImpl *>(get());
}

EditCommand &EditCommand::emptyCommand()
{
    static EditCommand m_emptyCommand;
    return m_emptyCommand;
}

//------------------------------------------------------------------------------------------
// CompositeEditCommand

CompositeEditCommand::CompositeEditCommand() : EditCommand() 
{
}

CompositeEditCommand::CompositeEditCommand(CompositeEditCommandImpl *impl) : EditCommand(impl) 
{
}

CompositeEditCommand::CompositeEditCommand(const CompositeEditCommand &o) 
    : EditCommand(o.impl()) 
{
}

CompositeEditCommand::~CompositeEditCommand()
{
}

CompositeEditCommandImpl *CompositeEditCommand::impl() const
{
    return static_cast<CompositeEditCommandImpl *>(get());
}

//==========================================================================================
// Concrete commands
//------------------------------------------------------------------------------------------
// AppendNodeCommand

AppendNodeCommand::AppendNodeCommand(DocumentImpl *document, NodeImpl *parentNode, NodeImpl *appendChild)
    : EditCommand(new AppendNodeCommandImpl(document, parentNode, appendChild))
{
}

AppendNodeCommand::~AppendNodeCommand()
{
}

AppendNodeCommandImpl *AppendNodeCommand::impl() const
{
    return static_cast<AppendNodeCommandImpl *>(get());
}

NodeImpl *AppendNodeCommand::parentNode() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->parentNode();
}

NodeImpl *AppendNodeCommand::appendChild() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->appendChild();
}

//------------------------------------------------------------------------------------------
// ApplyStyleCommand

ApplyStyleCommand::ApplyStyleCommand(DocumentImpl *document, CSSStyleDeclarationImpl *style)
    : CompositeEditCommand(new ApplyStyleCommandImpl(document, style))
{
}

ApplyStyleCommand::~ApplyStyleCommand()
{
}

ApplyStyleCommandImpl *ApplyStyleCommand::impl() const
{
    return static_cast<ApplyStyleCommandImpl *>(get());
}

CSSStyleDeclarationImpl *ApplyStyleCommand::style() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->style();
}

//------------------------------------------------------------------------------------------
// DeleteCollapsibleWhitespaceCommand

DeleteCollapsibleWhitespaceCommand::DeleteCollapsibleWhitespaceCommand(DocumentImpl *document)
    : CompositeEditCommand(new DeleteCollapsibleWhitespaceCommandImpl(document))
{
}

DeleteCollapsibleWhitespaceCommand::DeleteCollapsibleWhitespaceCommand(DocumentImpl *document, const Selection &selection)
    : CompositeEditCommand(new DeleteCollapsibleWhitespaceCommandImpl(document, selection))
{
}

DeleteCollapsibleWhitespaceCommand::~DeleteCollapsibleWhitespaceCommand()
{
}
	
DeleteCollapsibleWhitespaceCommandImpl *DeleteCollapsibleWhitespaceCommand::impl() const
{
    return static_cast<DeleteCollapsibleWhitespaceCommandImpl *>(get());
}

//------------------------------------------------------------------------------------------
// DeleteSelectionCommand

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document)
    : CompositeEditCommand(new DeleteSelectionCommandImpl(document))
{
}

DeleteSelectionCommand::DeleteSelectionCommand(DocumentImpl *document, const Selection &selection)
    : CompositeEditCommand(new DeleteSelectionCommandImpl(document, selection))
{
}

DeleteSelectionCommand::~DeleteSelectionCommand()
{
}
	
DeleteSelectionCommandImpl *DeleteSelectionCommand::impl() const
{
    return static_cast<DeleteSelectionCommandImpl *>(get());
}

//------------------------------------------------------------------------------------------
// DeleteTextCommand

DeleteTextCommand::DeleteTextCommand(DocumentImpl *document, TextImpl *node, long offset, long count)
    : EditCommand(new DeleteTextCommandImpl(document, node, offset, count))
{
}

DeleteTextCommand::DeleteTextCommand(const DeleteTextCommand &o)
    : EditCommand(o.impl())
{
}

DeleteTextCommand::~DeleteTextCommand()
{
}

DeleteTextCommandImpl *DeleteTextCommand::impl() const
{
    return static_cast<DeleteTextCommandImpl *>(get());
}

TextImpl *DeleteTextCommand::node() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->node();
}

long DeleteTextCommand::offset() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->offset();
}

long DeleteTextCommand::count() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->count();
}

//------------------------------------------------------------------------------------------
// InputNewlineCommand

InputNewlineCommand::InputNewlineCommand(DocumentImpl *document) 
    : CompositeEditCommand(new InputNewlineCommandImpl(document))
{
}

InputNewlineCommand::~InputNewlineCommand() 
{
}

InputNewlineCommandImpl *InputNewlineCommand::impl() const
{
    return static_cast<InputNewlineCommandImpl *>(get());
}

//------------------------------------------------------------------------------------------
// InputTextCommand

InputTextCommand::InputTextCommand(DocumentImpl *document) 
    : CompositeEditCommand(new InputTextCommandImpl(document))
{
}

InputTextCommand::~InputTextCommand() 
{
}

InputTextCommandImpl *InputTextCommand::impl() const
{
    return static_cast<InputTextCommandImpl *>(get());
}

void InputTextCommand::deleteCharacter()
{
    IF_IMPL_NULL_RETURN;
    impl()->deleteCharacter();
}

void InputTextCommand::input(const DOM::DOMString &text)
{
    IF_IMPL_NULL_RETURN;
    impl()->input(text);
}

unsigned long InputTextCommand::charactersAdded() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->charactersAdded();
}

//------------------------------------------------------------------------------------------
// InsertNodeBeforeCommand

InsertNodeBeforeCommand::InsertNodeBeforeCommand() : EditCommand()
{
}

InsertNodeBeforeCommand::InsertNodeBeforeCommand(DocumentImpl *document, NodeImpl *insertChild, NodeImpl *refChild)
    : EditCommand(new InsertNodeBeforeCommandImpl(document, insertChild, refChild))
{
}

InsertNodeBeforeCommand::InsertNodeBeforeCommand(const InsertNodeBeforeCommand &o)
    : EditCommand(new InsertNodeBeforeCommandImpl(o.document(), o.insertChild(), o.refChild()))
{
}

InsertNodeBeforeCommand::~InsertNodeBeforeCommand()
{
}

InsertNodeBeforeCommandImpl *InsertNodeBeforeCommand::impl() const
{
    return static_cast<InsertNodeBeforeCommandImpl *>(get());
}

NodeImpl *InsertNodeBeforeCommand::insertChild() const
{
    IF_IMPL_NULL_RETURN_ARG(0);        
    return impl()->insertChild();
}

NodeImpl *InsertNodeBeforeCommand::refChild() const
{
    IF_IMPL_NULL_RETURN_ARG(0);        
    return impl()->refChild();
}

//------------------------------------------------------------------------------------------
// InsertTextCommand

InsertTextCommand::InsertTextCommand(DocumentImpl *document, TextImpl *node, long offset, const DOMString &text)
    : EditCommand(new InsertTextCommandImpl(document, node, offset, text))
{
}

InsertTextCommand::~InsertTextCommand()
{
}

InsertTextCommandImpl *InsertTextCommand::impl() const
{
    return static_cast<InsertTextCommandImpl *>(get());
}

TextImpl *InsertTextCommand::node() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->node();
}

long InsertTextCommand::offset() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->offset();
}

DOMString InsertTextCommand::text() const
{
    IF_IMPL_NULL_RETURN_ARG(DOMString());
    return impl()->text();
}

//------------------------------------------------------------------------------------------
// JoinTextNodesCommand

JoinTextNodesCommand::JoinTextNodesCommand(DocumentImpl *document, TextImpl *text1, TextImpl *text2)
    : EditCommand(new JoinTextNodesCommandImpl(document, text1, text2))
{
}

JoinTextNodesCommand::~JoinTextNodesCommand()
{
}

JoinTextNodesCommandImpl *JoinTextNodesCommand::impl() const
{
    return static_cast<JoinTextNodesCommandImpl *>(get());
}

TextImpl *JoinTextNodesCommand::firstNode() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->firstNode();
}

TextImpl *JoinTextNodesCommand::secondNode() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->secondNode();
}

//------------------------------------------------------------------------------------------
// ReplaceSelectionCommand

ReplaceSelectionCommand::ReplaceSelectionCommand(DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement) 
    : CompositeEditCommand(new ReplaceSelectionCommandImpl(document, fragment, selectReplacement))
{
}

ReplaceSelectionCommand::~ReplaceSelectionCommand() 
{
}

ReplaceSelectionCommandImpl *ReplaceSelectionCommand::impl() const
{
    return static_cast<ReplaceSelectionCommandImpl *>(get());
}

//------------------------------------------------------------------------------------------
// MoveSelectionCommand

MoveSelectionCommand::MoveSelectionCommand(DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position) 
: CompositeEditCommand(new MoveSelectionCommandImpl(document, fragment, position))
{
}

MoveSelectionCommand::~MoveSelectionCommand() 
{
}

MoveSelectionCommandImpl *MoveSelectionCommand::impl() const
{
    return static_cast<MoveSelectionCommandImpl *>(get());
}

//------------------------------------------------------------------------------------------
// RemoveCSSPropertyCommand

RemoveCSSPropertyCommand::RemoveCSSPropertyCommand(DocumentImpl *document, CSSStyleDeclarationImpl *decl, int property)
    : EditCommand(new RemoveCSSPropertyCommandImpl(document, decl, property))
{
}

RemoveCSSPropertyCommand::~RemoveCSSPropertyCommand()
{
}

RemoveCSSPropertyCommandImpl *RemoveCSSPropertyCommand::impl() const
{
    return static_cast<RemoveCSSPropertyCommandImpl *>(get());
}

CSSStyleDeclarationImpl *RemoveCSSPropertyCommand::styleDeclaration() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->styleDeclaration();
}

int RemoveCSSPropertyCommand::property() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->property();
}

//------------------------------------------------------------------------------------------
// RemoveNodeAttributeCommand

RemoveNodeAttributeCommand::RemoveNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute)
    : EditCommand(new RemoveNodeAttributeCommandImpl(document, element, attribute))
{
}

RemoveNodeAttributeCommand::~RemoveNodeAttributeCommand()
{
}

RemoveNodeAttributeCommandImpl *RemoveNodeAttributeCommand::impl() const
{
    return static_cast<RemoveNodeAttributeCommandImpl *>(get());
}

ElementImpl *RemoveNodeAttributeCommand::element() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->element();
}

NodeImpl::Id RemoveNodeAttributeCommand::attribute() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->attribute();
}

//------------------------------------------------------------------------------------------
// RemoveNodeCommand

RemoveNodeCommand::RemoveNodeCommand(DocumentImpl *document, NodeImpl *node)
    : EditCommand(new RemoveNodeCommandImpl(document, node))
{
}

RemoveNodeCommand::~RemoveNodeCommand()
{
}

RemoveNodeCommandImpl *RemoveNodeCommand::impl() const
{
    return static_cast<RemoveNodeCommandImpl *>(get());
}

NodeImpl *RemoveNodeCommand::node() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->node();
}

//------------------------------------------------------------------------------------------
// RemoveNodeAndPruneCommand

RemoveNodeAndPruneCommand::RemoveNodeAndPruneCommand(DocumentImpl *document, NodeImpl *pruneNode, NodeImpl *stopNode)
    : CompositeEditCommand(new RemoveNodeAndPruneCommandImpl(document, pruneNode, stopNode))
{
}

RemoveNodeAndPruneCommand::~RemoveNodeAndPruneCommand()
{
}

RemoveNodeAndPruneCommandImpl *RemoveNodeAndPruneCommand::impl() const
{
    return static_cast<RemoveNodeAndPruneCommandImpl *>(get());
}

NodeImpl *RemoveNodeAndPruneCommand::pruneNode() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->pruneNode();
}

NodeImpl *RemoveNodeAndPruneCommand::stopNode() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->stopNode();
}

//------------------------------------------------------------------------------------------
// RemoveNodePreservingChildrenCommand

RemoveNodePreservingChildrenCommand::RemoveNodePreservingChildrenCommand(DocumentImpl *document, NodeImpl *node)
    : CompositeEditCommand(new RemoveNodePreservingChildrenCommandImpl(document, node))
{
}

RemoveNodePreservingChildrenCommand::~RemoveNodePreservingChildrenCommand()
{
}

RemoveNodePreservingChildrenCommandImpl *RemoveNodePreservingChildrenCommand::impl() const
{
    return static_cast<RemoveNodePreservingChildrenCommandImpl *>(get());
}

NodeImpl *RemoveNodePreservingChildrenCommand::node() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->node();
}

//------------------------------------------------------------------------------------------
// SetNodeAttributeCommand

SetNodeAttributeCommand::SetNodeAttributeCommand(DocumentImpl *document, ElementImpl *element, NodeImpl::Id attribute, const DOM::DOMString &value)
    : EditCommand(new SetNodeAttributeCommandImpl(document, element, attribute, value))
{
}

SetNodeAttributeCommand::~SetNodeAttributeCommand()
{
}

SetNodeAttributeCommandImpl *SetNodeAttributeCommand::impl() const
{
    return static_cast<SetNodeAttributeCommandImpl *>(get());
}

ElementImpl *SetNodeAttributeCommand::element() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->element();
}

NodeImpl::Id SetNodeAttributeCommand::attribute() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->attribute();
}

DOMString SetNodeAttributeCommand::value() const
{
    IF_IMPL_NULL_RETURN_ARG("");
    return impl()->value();
}

//------------------------------------------------------------------------------------------
// SplitTextNodeCommand

SplitTextNodeCommand::SplitTextNodeCommand(DocumentImpl *document, TextImpl *text, long offset)
    : EditCommand(new SplitTextNodeCommandImpl(document, text, offset))
{
}

SplitTextNodeCommand::~SplitTextNodeCommand()
{
}

SplitTextNodeCommandImpl *SplitTextNodeCommand::impl() const
{
    return static_cast<SplitTextNodeCommandImpl *>(get());
}

TextImpl *SplitTextNodeCommand::node() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->node();
}

long SplitTextNodeCommand::offset() const
{
    IF_IMPL_NULL_RETURN_ARG(0);
    return impl()->offset();
}

//------------------------------------------------------------------------------------------
// TypingCommand

TypingCommand::TypingCommand(DocumentImpl *document) 
    : CompositeEditCommand(new TypingCommandImpl(document))
{
}

TypingCommand::~TypingCommand() 
{
}

TypingCommandImpl *TypingCommand::impl() const
{
    return static_cast<TypingCommandImpl *>(get());
}

void TypingCommand::insertText(DocumentImpl *document, const DOMString &text)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommand lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand &>(lastEditCommand).insertText(text);
    }
    else {
        TypingCommand typingCommand(document);
        typingCommand.apply();
        static_cast<TypingCommand &>(typingCommand).insertText(text);
    }
}

void TypingCommand::insertNewline(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommand lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand &>(lastEditCommand).insertNewline();
    }
    else {
        TypingCommand typingCommand(document);
        typingCommand.apply();
        static_cast<TypingCommand &>(typingCommand).insertNewline();
    }
}

void TypingCommand::deleteKeyPressed(DocumentImpl *document)
{
    ASSERT(document);
    
    KHTMLPart *part = document->part();
    ASSERT(part);
    
    EditCommand lastEditCommand = part->lastEditCommand();
    if (isOpenForMoreTypingCommand(lastEditCommand)) {
        static_cast<TypingCommand &>(lastEditCommand).deleteKeyPressed();
    }
    else {
        TypingCommand typingCommand(document);
        typingCommand.apply();
        static_cast<TypingCommand &>(typingCommand).deleteKeyPressed();
    }
}

bool TypingCommand::isOpenForMoreTypingCommand(const EditCommand &cmd)
{
    return cmd.commandID() == TypingCommandID && 
        static_cast<const TypingCommand &>(cmd).openForMoreTyping();
}

void TypingCommand::closeTyping(EditCommand cmd)
{
    if (isOpenForMoreTypingCommand(cmd))
        static_cast<TypingCommand &>(cmd).closeTyping();
}

void TypingCommand::closeTyping()
{
    IF_IMPL_NULL_RETURN;
    return impl()->closeTyping();
}

bool TypingCommand::openForMoreTyping() const
{
    IF_IMPL_NULL_RETURN_ARG(false);
    return impl()->openForMoreTyping();
}

void TypingCommand::insertText(const DOMString &text)
{
    IF_IMPL_NULL_RETURN;
    return impl()->insertText(text);
}

void TypingCommand::insertNewline()
{
    IF_IMPL_NULL_RETURN;
    return impl()->insertNewline();
}

void TypingCommand::deleteKeyPressed()
{
    IF_IMPL_NULL_RETURN;
    return impl()->deleteKeyPressed();
}

} // namespace khtml
