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
#include "qptrlist.h"
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
class VisiblePosition;

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

    bool isInsertTextCommand() const;
    bool isInsertLineBreakCommand() const;
    bool isTypingCommand() const;

    static EditCommandPtr &emptyCommand();
};

//------------------------------------------------------------------------------------------
// StyleChange

class StyleChange {
public:
    enum ELegacyHTMLStyles { DoNotUseLegacyHTMLStyles, UseLegacyHTMLStyles };

    explicit StyleChange(DOM::CSSStyleDeclarationImpl *, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);
    StyleChange(DOM::CSSStyleDeclarationImpl *, const DOM::Position &, ELegacyHTMLStyles usesLegacyStyles=UseLegacyHTMLStyles);

    DOM::DOMString cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }
    bool usesLegacyStyles() const { return m_usesLegacyStyles; }

private:
    void init(DOM::CSSStyleDeclarationImpl *, const DOM::Position &);
    bool checkForLegacyHTMLStyleChange(const DOM::CSSProperty *);
    static bool currentlyHasStyle(const DOM::Position &, const DOM::CSSProperty *);
    
    DOM::DOMString m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
    bool m_usesLegacyStyles;
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

    void setEndingSelectionNeedsLayout(bool flag=true) { m_endingSelection.setNeedsLayout(flag); }
        
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const khtml::Selection &s);
    void setEndingSelection(const khtml::Selection &s);

    DOM::CSSStyleDeclarationImpl *typingStyle() const { return m_typingStyle; };
    void setTypingStyle(DOM::CSSStyleDeclarationImpl *);
    
    virtual bool isInsertTextCommand() const;
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
    void deleteSelection(bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteSelection(const khtml::Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteTextFromNode(DOM::TextImpl *node, long offset, long count);
    void inputText(const DOM::DOMString &text, bool selectInsertedText = false);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertTextIntoNode(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property);
    void removeFullySelectedNode(DOM::NodeImpl *);
    void removeNodeAttribute(DOM::ElementImpl *, int attribute);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodePreservingChildren(DOM::NodeImpl *node);
    void replaceTextInNode(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText);
    void setNodeAttribute(DOM::ElementImpl *, int attribute, const DOM::DOMString &);
    void splitTextNode(DOM::TextImpl *text, long offset);

    DOM::NodeImpl *applyTypingStyle(DOM::NodeImpl *) const;

    void deleteInsignificantText(DOM::TextImpl *, int start, int end);
    void deleteInsignificantText(const DOM::Position &start, const DOM::Position &end);
    void deleteInsignificantTextDownstream(const DOM::Position &);

    void insertBlockPlaceholderIfNeeded(DOM::NodeImpl *);
    bool removeBlockPlaceholderIfNeeded(DOM::NodeImpl *);

    bool isLastVisiblePositionInNode(const VisiblePosition &, const DOM::NodeImpl *) const;

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
    bool isHTMLStyleNode(DOM::CSSStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeCSSStyle(DOM::CSSStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeBlockStyle(DOM::CSSStyleDeclarationImpl *, const DOM::Position &start, const DOM::Position &end);
    void removeInlineStyle(DOM::CSSStyleDeclarationImpl *, const DOM::Position &start, const DOM::Position &end);
    bool nodeFullySelected(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end) const;

    // style-application helpers
    void applyBlockStyle(DOM::CSSStyleDeclarationImpl *);
    void applyInlineStyle(DOM::CSSStyleDeclarationImpl *);
    void addBlockStyleIfNeeded(DOM::CSSStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void addInlineStyleIfNeeded(DOM::CSSStyleDeclarationImpl *, DOM::NodeImpl *start, DOM::NodeImpl *end);
    bool splitTextAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    DOM::NodeImpl *splitTextAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    void surroundNodeRangeWithElement(DOM::NodeImpl *start, DOM::NodeImpl *end, DOM::ElementImpl *element);
    DOM::Position positionInsertionPoint(DOM::Position);
    
    DOM::CSSStyleDeclarationImpl *m_style;
};

//------------------------------------------------------------------------------------------
// DeleteFromTextNodeCommand

class DeleteFromTextNodeCommand : public EditCommand
{
public:
    DeleteFromTextNodeCommand(DOM::DocumentImpl *document, DOM::TextImpl *node, long offset, long count);
    virtual ~DeleteFromTextNodeCommand();
	
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
// DeleteSelectionCommand

class DeleteSelectionCommand : public CompositeEditCommand
{ 
public:
    DeleteSelectionCommand(DOM::DocumentImpl *document, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    DeleteSelectionCommand(DOM::DocumentImpl *document, const khtml::Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
	
    virtual void doApply();
    
private:
    virtual bool preservesTypingStyle() const;

    void initializePositionData();
    void saveTypingStyleState();
    bool handleSpecialCaseAllContentDelete();
    bool handleSpecialCaseBRDelete();
    void handleGeneralDelete();
    void fixupWhitespace();
    void moveNodesAfterNode();
    void calculateEndingPosition();
    void calculateTypingStyleAfterDelete();
    void clearTransientState();

    bool m_hasSelectionToDelete;
    bool m_smartDelete;
    bool m_mergeBlocksAfterDelete;
    bool m_trailingWhitespaceValid;

    // This data is transient and should be cleared at the end of the doApply function.
    khtml::Selection m_selectionToDelete;
    DOM::Position m_upstreamStart;
    DOM::Position m_downstreamStart;
    DOM::Position m_upstreamEnd;
    DOM::Position m_downstreamEnd;
    DOM::Position m_endingPosition;
    DOM::Position m_leadingWhitespace;
    DOM::Position m_trailingWhitespace;
    DOM::NodeImpl *m_startBlock;
    DOM::NodeImpl *m_endBlock;
    DOM::NodeImpl *m_startNode;
    DOM::CSSStyleDeclarationImpl *m_typingStyle;
};

//------------------------------------------------------------------------------------------
// InsertIntoTextNode

class InsertIntoTextNode : public EditCommand
{
public:
    InsertIntoTextNode(DOM::DocumentImpl *document, DOM::TextImpl *, long, const DOM::DOMString &);
    virtual ~InsertIntoTextNode();
	
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
// InsertLineBreakCommand

class InsertLineBreakCommand : public CompositeEditCommand
{
public:
    InsertLineBreakCommand(DOM::DocumentImpl *document);

    virtual void doApply();

private:
    void insertNodeAfterPosition(DOM::NodeImpl *node, const DOM::Position &pos);
    void insertNodeBeforePosition(DOM::NodeImpl *node, const DOM::Position &pos);
};

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorCommand

class InsertParagraphSeparatorCommand : public CompositeEditCommand
{
public:
    InsertParagraphSeparatorCommand(DOM::DocumentImpl *document);

    virtual void doApply();

private:
    QPtrList<DOM::NodeImpl> ancestors;
    QPtrList<DOM::NodeImpl> clonedNodes;
};

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorInQuotedContentCommand

class InsertParagraphSeparatorInQuotedContentCommand : public CompositeEditCommand
{
public:
    InsertParagraphSeparatorInQuotedContentCommand(DOM::DocumentImpl *);
    virtual ~InsertParagraphSeparatorInQuotedContentCommand();
	
    virtual void doApply();
    
private:
    bool isMailBlockquote(const DOM::NodeImpl *) const;

    QPtrList<DOM::NodeImpl> ancestors;
    QPtrList<DOM::NodeImpl> clonedNodes;
    DOM::ElementImpl *m_breakNode;
};

//------------------------------------------------------------------------------------------
// InsertTextCommand

class InsertTextCommand : public CompositeEditCommand
{
public:
    InsertTextCommand(DOM::DocumentImpl *document);

    virtual void doApply();

    void deleteCharacter();
    void input(const DOM::DOMString &text, bool selectInsertedText = false);
    
    unsigned long charactersAdded() const { return m_charactersAdded; }
    
private:
    virtual bool isInsertTextCommand() const;

    DOM::Position prepareForTextInsertion(bool adjustDownstream);
    void insertSpace(DOM::TextImpl *textNode, unsigned long offset);

    unsigned long m_charactersAdded;
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
    enum ETypingCommand { 
        DeleteKey, 
        InsertText, 
        InsertLineBreak, 
        InsertParagraphSeparator,
        InsertParagraphSeparatorInQuotedContent,
    };

    TypingCommand(DOM::DocumentImpl *document, ETypingCommand, const DOM::DOMString &text = "", bool selectInsertedText = false);

    static void deleteKeyPressed(DOM::DocumentImpl *);
    static void insertText(DOM::DocumentImpl *, const DOM::DOMString &, bool selectInsertedText = false);
    static void insertLineBreak(DOM::DocumentImpl *);
    static void insertParagraphSeparator(DOM::DocumentImpl *);
    static void insertParagraphSeparatorInQuotedContent(DOM::DocumentImpl *);
    static bool isOpenForMoreTypingCommand(const EditCommandPtr &);
    static void closeTyping(const EditCommandPtr &);
    
    virtual void doApply();

    bool openForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const DOM::DOMString &text, bool selectInsertedText);
    void insertLineBreak();
    void insertParagraphSeparatorInQuotedContent();
    void insertParagraphSeparator();
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
