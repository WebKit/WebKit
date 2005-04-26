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
#include "editing/edit_actions.h"
#include "qmap.h"
#include "qptrlist.h"
#include "qvaluelist.h"
#include "selection.h"
#include "shared.h"

namespace DOM {
    class CSSMutableStyleDeclarationImpl;
    class CSSProperty;
    class CSSStyleDeclarationImpl;
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

    EditAction editingAction() const;

    DOM::DocumentImpl * const document() const;

    Selection startingSelection() const;
    Selection endingSelection() const;

    void setStartingSelection(const Selection &s) const;
    void setStartingSelection(const VisiblePosition &p) const;
    void setStartingSelection(const DOM::Position &p, EAffinity affinity) const;
    void setEndingSelection(const Selection &s) const;
    void setEndingSelection(const VisiblePosition &p) const;
    void setEndingSelection(const DOM::Position &p, EAffinity affinity) const;

    DOM::CSSMutableStyleDeclarationImpl *typingStyle() const;
    void setTypingStyle(DOM::CSSMutableStyleDeclarationImpl *) const;

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

    static ELegacyHTMLStyles styleModeForParseMode(bool);

    DOM::DOMString cssStyle() const { return m_cssStyle; }
    bool applyBold() const { return m_applyBold; }
    bool applyItalic() const { return m_applyItalic; }
    bool applyFontColor() const { return m_applyFontColor.length() > 0; }
    bool applyFontFace() const { return m_applyFontFace.length() > 0; }
    bool applyFontSize() const { return m_applyFontSize.length() > 0; }

    DOM::DOMString fontColor() { return m_applyFontColor; }
    DOM::DOMString fontFace() { return m_applyFontFace; }
    DOM::DOMString fontSize() { return m_applyFontSize; }

    bool usesLegacyStyles() const { return m_usesLegacyStyles; }

private:
    void init(DOM::CSSStyleDeclarationImpl *, const DOM::Position &);
    bool checkForLegacyHTMLStyleChange(const DOM::CSSProperty *);
    static bool currentlyHasStyle(const DOM::Position &, const DOM::CSSProperty *);
    
    DOM::DOMString m_cssStyle;
    bool m_applyBold;
    bool m_applyItalic;
    DOM::DOMString m_applyFontColor;
    DOM::DOMString m_applyFontFace;
    DOM::DOMString m_applyFontSize;
    bool m_usesLegacyStyles;
};

//------------------------------------------------------------------------------------------
// EditCommand

class EditCommand : public Shared<EditCommand>
{
public:
    EditCommand(DOM::DocumentImpl *);
    virtual ~EditCommand();

    bool isCompositeStep() const { return m_parent != 0; }
    EditCommand *parent() const { return m_parent; }
    void setParent(EditCommand *parent) { m_parent = parent; }

    enum ECommandState { NotApplied, Applied };
    
    void apply();	
    void unapply();
    void reapply();

    virtual void doApply() = 0;
    virtual void doUnapply() = 0;
    virtual void doReapply();  // calls doApply()

    virtual EditAction editingAction() const;

    virtual DOM::DocumentImpl * const document() const { return m_document; }

    Selection startingSelection() const { return m_startingSelection; }
    Selection endingSelection() const { return m_endingSelection; }

    void setEndingSelectionNeedsLayout(bool flag=true) { m_endingSelection.setNeedsLayout(flag); }
        
    ECommandState state() const { return m_state; }
    void setState(ECommandState state) { m_state = state; }

    void setStartingSelection(const Selection &s);
    void setStartingSelection(const VisiblePosition &p);
    void setStartingSelection(const DOM::Position &p, EAffinity affinity);
    void setEndingSelection(const Selection &s);
    void setEndingSelection(const VisiblePosition &p);
    void setEndingSelection(const DOM::Position &p, EAffinity affinity);

    DOM::CSSMutableStyleDeclarationImpl *typingStyle() const { return m_typingStyle; };
    void setTypingStyle(DOM::CSSMutableStyleDeclarationImpl *);
    
    DOM::CSSMutableStyleDeclarationImpl *styleAtPosition(const DOM::Position &pos);
    
    virtual bool isInsertTextCommand() const;
    virtual bool isTypingCommand() const;
    
private:
    void assignTypingStyle(DOM::CSSMutableStyleDeclarationImpl *);

    virtual bool preservesTypingStyle() const;

    DOM::DocumentImpl *m_document;
    ECommandState m_state;
    Selection m_startingSelection;
    Selection m_endingSelection;
    DOM::CSSMutableStyleDeclarationImpl *m_typingStyle;
    EditCommand *m_parent;
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
    void applyStyle(DOM::CSSStyleDeclarationImpl *style, EditAction editingAction=EditActionChangeAttributes);
    void deleteKeyPressed();
    void deleteSelection(bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteSelection(const Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
    void deleteTextFromNode(DOM::TextImpl *node, long offset, long count);
    void inputText(const DOM::DOMString &text, bool selectInsertedText = false);
    void insertNodeAfter(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAt(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBefore(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertParagraphSeparator();
    void insertTextIntoNode(DOM::TextImpl *node, long offset, const DOM::DOMString &text);
    void joinTextNodes(DOM::TextImpl *text1, DOM::TextImpl *text2);
    void rebalanceWhitespace();
    void removeCSSProperty(DOM::CSSStyleDeclarationImpl *, int property);
    void removeFullySelectedNode(DOM::NodeImpl *node);
    void removeNodeAttribute(DOM::ElementImpl *, int attribute);
    void removeChildrenInRange(DOM::NodeImpl *node, int from, int to);
    void removeNode(DOM::NodeImpl *removeChild);
    void removeNodePreservingChildren(DOM::NodeImpl *node);
    void replaceTextInNode(DOM::TextImpl *node, long offset, long count, const DOM::DOMString &replacementText);
    void setNodeAttribute(DOM::ElementImpl *, int attribute, const DOM::DOMString &);
    void splitTextNode(DOM::TextImpl *text, long offset);
    void splitElement(DOM::ElementImpl *element, DOM::NodeImpl *atChild);
    void mergeIdenticalElements(DOM::ElementImpl *first, DOM::ElementImpl *second);
    void wrapContentsInDummySpan(DOM::ElementImpl *element);
    void splitTextNodeContainingElement(DOM::TextImpl *text, long offset);

    void deleteInsignificantText(DOM::TextImpl *, int start, int end);
    void deleteInsignificantText(const DOM::Position &start, const DOM::Position &end);
    void deleteInsignificantTextDownstream(const DOM::Position &);

    DOM::NodeImpl *appendBlockPlaceholder(DOM::NodeImpl *);
    DOM::NodeImpl *insertBlockPlaceholder(const DOM::Position &pos);
    DOM::NodeImpl *addBlockPlaceholderIfNeeded(DOM::NodeImpl *);
    bool removeBlockPlaceholder(DOM::NodeImpl *);
    DOM::NodeImpl *findBlockPlaceholder(DOM::NodeImpl *);

    void moveParagraphContentsToNewBlockIfNecessary(const DOM::Position &);

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
    enum EPropertyLevel { PropertyDefault, ForceBlockProperties };

    ApplyStyleCommand(DOM::DocumentImpl *, DOM::CSSStyleDeclarationImpl *style, EditAction editingAction=EditActionChangeAttributes, EPropertyLevel=PropertyDefault);
    virtual ~ApplyStyleCommand();
	
    virtual void doApply();
    virtual EditAction editingAction() const;

    DOM::CSSMutableStyleDeclarationImpl *style() const { return m_style; }

private:
    // style-removal helpers
    bool isHTMLStyleNode(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeHTMLStyleNode(DOM::HTMLElementImpl *);
    void removeHTMLFontStyle(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeCSSStyle(DOM::CSSMutableStyleDeclarationImpl *, DOM::HTMLElementImpl *);
    void removeBlockStyle(DOM::CSSMutableStyleDeclarationImpl *, const DOM::Position &start, const DOM::Position &end);
    void removeInlineStyle(DOM::CSSMutableStyleDeclarationImpl *, const DOM::Position &start, const DOM::Position &end);
    bool nodeFullySelected(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end) const;
    bool nodeFullyUnselected(DOM::NodeImpl *node, const DOM::Position &start, const DOM::Position &end) const;
    DOM::CSSMutableStyleDeclarationImpl *extractTextDecorationStyle(DOM::NodeImpl *node);
    DOM::CSSMutableStyleDeclarationImpl *extractAndNegateTextDecorationStyle(DOM::NodeImpl *node);
    void applyTextDecorationStyle(DOM::NodeImpl *node, DOM::CSSMutableStyleDeclarationImpl *style);
    void pushDownTextDecorationStyleAroundNode(DOM::NodeImpl *node, const DOM::Position &start, const DOM::Position &end, bool force);
    void pushDownTextDecorationStyleAtBoundaries(const DOM::Position &start, const DOM::Position &end);
    
    // style-application helpers
    void applyBlockStyle(DOM::CSSMutableStyleDeclarationImpl *);
    void applyRelativeFontStyleChange(DOM::CSSMutableStyleDeclarationImpl *);
    void applyInlineStyle(DOM::CSSMutableStyleDeclarationImpl *);
    void addBlockStyleIfNeeded(DOM::CSSMutableStyleDeclarationImpl *, DOM::NodeImpl *);
    void addInlineStyleIfNeeded(DOM::CSSMutableStyleDeclarationImpl *, DOM::NodeImpl *start, DOM::NodeImpl *end);
    bool splitTextAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextElementAtStartIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool splitTextElementAtEndIfNeeded(const DOM::Position &start, const DOM::Position &end);
    bool mergeStartWithPreviousIfIdentical(const DOM::Position &start, const DOM::Position &end);
    bool mergeEndWithNextIfIdentical(const DOM::Position &start, const DOM::Position &end);
    void cleanUpEmptyStyleSpans(const DOM::Position &start, const DOM::Position &end);

    void surroundNodeRangeWithElement(DOM::NodeImpl *start, DOM::NodeImpl *end, DOM::ElementImpl *element);
    float computedFontSize(const DOM::NodeImpl *);
    void joinChildTextNodes(DOM::NodeImpl *, const DOM::Position &start, const DOM::Position &end);
    
    DOM::CSSMutableStyleDeclarationImpl *m_style;
    EditAction m_editingAction;
    EPropertyLevel m_propertyLevel;
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
    DeleteSelectionCommand(DOM::DocumentImpl *document, const Selection &selection, bool smartDelete=false, bool mergeBlocksAfterDelete=true);
	
    virtual void doApply();
    virtual EditAction editingAction() const;
    
private:
    virtual bool preservesTypingStyle() const;

    void initializePositionData();
    void saveTypingStyleState();
    void insertPlaceholderForAncestorBlockContent();
    bool handleSpecialCaseBRDelete();
    void handleGeneralDelete();
    void fixupWhitespace();
    void moveNodesAfterNode();
    void calculateEndingPosition();
    void calculateTypingStyleAfterDelete(DOM::NodeImpl *insertedPlaceholder);
    void clearTransientState();

    void setStartNode(DOM::NodeImpl *);

    bool m_hasSelectionToDelete;
    bool m_smartDelete;
    bool m_mergeBlocksAfterDelete;
    bool m_trailingWhitespaceValid;

    // This data is transient and should be cleared at the end of the doApply function.
    Selection m_selectionToDelete;
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
    DOM::CSSMutableStyleDeclarationImpl *m_typingStyle;
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
    virtual bool preservesTypingStyle() const;
    void insertNodeAfterPosition(DOM::NodeImpl *node, const DOM::Position &pos);
    void insertNodeBeforePosition(DOM::NodeImpl *node, const DOM::Position &pos);
};

//------------------------------------------------------------------------------------------
// InsertParagraphSeparatorCommand

class InsertParagraphSeparatorCommand : public CompositeEditCommand
{
public:
    InsertParagraphSeparatorCommand(DOM::DocumentImpl *document);
    virtual ~InsertParagraphSeparatorCommand();

    virtual void doApply();

private:
    DOM::ElementImpl *createParagraphElement();
    void calculateStyleBeforeInsertion(const DOM::Position &);
    void applyStyleAfterInsertion();

    virtual bool preservesTypingStyle() const;

    QPtrList<DOM::NodeImpl> ancestors;
    QPtrList<DOM::NodeImpl> clonedNodes;
    DOM::CSSMutableStyleDeclarationImpl *m_style;
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
// MoveSelectionCommand

class MoveSelectionCommand : public CompositeEditCommand
{
public:
    MoveSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, DOM::Position &position, bool smartMove=false);
    virtual ~MoveSelectionCommand();
    
    virtual void doApply();
    virtual EditAction editingAction() const;
    
private:
    DOM::DocumentFragmentImpl *m_fragment;
    DOM::Position m_position;
    bool m_smartMove;
};

//------------------------------------------------------------------------------------------
// RebalanceWhitespaceCommand

class RebalanceWhitespaceCommand : public EditCommand
{
public:
    RebalanceWhitespaceCommand(DOM::DocumentImpl *, const DOM::Position &);
    virtual ~RebalanceWhitespaceCommand();

    virtual void doApply();
    virtual void doUnapply();

private:
    enum { InvalidOffset = -1 };

    virtual bool preservesTypingStyle() const;

    DOM::DOMString m_beforeString;
    DOM::DOMString m_afterString;
    DOM::Position m_position;
    long m_upstreamOffset;
    long m_downstreamOffset;
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

    DOM::CSSMutableStyleDeclarationImpl *styleDeclaration() const { return m_decl; }
    int property() const { return m_property; }
    
private:
    DOM::CSSMutableStyleDeclarationImpl *m_decl;
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
// ReplaceSelectionCommand

// --- NodeDesiredStyle helper class

class NodeDesiredStyle
{
public:
    NodeDesiredStyle(DOM::NodeImpl *, DOM::CSSMutableStyleDeclarationImpl *);
    NodeDesiredStyle(const NodeDesiredStyle &);
    ~NodeDesiredStyle();
    
    DOM::NodeImpl *node() const { return m_node; }
    DOM::CSSMutableStyleDeclarationImpl *style() const { return m_style; }

    NodeDesiredStyle &operator=(const NodeDesiredStyle &);

private:
    DOM::NodeImpl *m_node;
    DOM::CSSMutableStyleDeclarationImpl *m_style;
};

// --- ReplacementFragment helper class

class ReplacementFragment
{
public:
    ReplacementFragment(DOM::DocumentImpl *, DOM::DocumentFragmentImpl *, bool matchStyle);
    ~ReplacementFragment();

    enum EFragmentType { EmptyFragment, SingleTextNodeFragment, TreeFragment };

    DOM::DocumentFragmentImpl *root() const { return m_fragment; }
    DOM::NodeImpl *firstChild() const;
    DOM::NodeImpl *lastChild() const;

    DOM::NodeImpl *mergeStartNode() const;

    const QValueList<NodeDesiredStyle> &desiredStyles() { return m_styles; }
        
    void pruneEmptyNodes();

    EFragmentType type() const { return m_type; }
    bool isEmpty() const { return m_type == EmptyFragment; }
    bool isSingleTextNode() const { return m_type == SingleTextNodeFragment; }
    bool isTreeFragment() const { return m_type == TreeFragment; }

    bool hasMoreThanOneBlock() const { return m_hasMoreThanOneBlock; }
    bool hasInterchangeNewlineAtStart() const { return m_hasInterchangeNewlineAtStart; }
    bool hasInterchangeNewlineAtEnd() const { return m_hasInterchangeNewlineAtEnd; }

private:
    // no copy construction or assignment
    ReplacementFragment(const ReplacementFragment &);
    ReplacementFragment &operator=(const ReplacementFragment &);
    
    static bool isInterchangeNewlineNode(const DOM::NodeImpl *);
    static bool isInterchangeConvertedSpaceSpan(const DOM::NodeImpl *);

    DOM::NodeImpl *insertFragmentForTestRendering();
    void restoreTestRenderingNodesToFragment(DOM::NodeImpl *);
    void computeStylesUsingTestRendering(DOM::NodeImpl *);
    void removeUnrenderedNodesUsingTestRendering(DOM::NodeImpl *);
    int countRenderedBlocks(DOM::NodeImpl *holder);
    void removeStyleNodes();

    // A couple simple DOM helpers
    DOM::NodeImpl *enclosingBlock(DOM::NodeImpl *) const;
    void removeNode(DOM::NodeImpl *);
    void removeNodePreservingChildren(DOM::NodeImpl *);
    void insertNodeBefore(DOM::NodeImpl *node, DOM::NodeImpl *refNode);

    EFragmentType m_type;
    DOM::DocumentImpl *m_document;
    DOM::DocumentFragmentImpl *m_fragment;
    QValueList<NodeDesiredStyle> m_styles;
    bool m_matchStyle;
    bool m_hasInterchangeNewlineAtStart;
    bool m_hasInterchangeNewlineAtEnd;
    bool m_hasMoreThanOneBlock;
};

class ReplaceSelectionCommand : public CompositeEditCommand
{
public:
    ReplaceSelectionCommand(DOM::DocumentImpl *document, DOM::DocumentFragmentImpl *fragment, bool selectReplacement=true, bool smartReplace=false, bool matchStyle=false);
    virtual ~ReplaceSelectionCommand();
    
    virtual void doApply();
    virtual EditAction editingAction() const;

private:
    void completeHTMLReplacement(const DOM::Position &lastPositionToSelect);

    void insertNodeAfterAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);
    void insertNodeAtAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild, long offset);
    void insertNodeBeforeAndUpdateNodesInserted(DOM::NodeImpl *insertChild, DOM::NodeImpl *refChild);

    void updateNodesInserted(DOM::NodeImpl *);
    void fixupNodeStyles(const QValueList<NodeDesiredStyle> &);
    void removeLinePlaceholderIfNeeded(DOM::NodeImpl *);

    ReplacementFragment m_fragment;
    DOM::NodeImpl *m_firstNodeInserted;
    DOM::NodeImpl *m_lastNodeInserted;
    DOM::NodeImpl *m_lastTopNodeInserted;
    DOM::CSSMutableStyleDeclarationImpl *m_insertionStyle;
    bool m_selectReplacement;
    bool m_smartReplace;
    bool m_matchStyle;
};

void computeAndStoreNodeDesiredStyle(DOM::NodeImpl *, QValueList<NodeDesiredStyle> &);

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
// WrapContentsInDummySpanCommand

class WrapContentsInDummySpanCommand : public EditCommand
{
public:
    WrapContentsInDummySpanCommand(DOM::DocumentImpl *, DOM::ElementImpl *);
    virtual ~WrapContentsInDummySpanCommand();
	
    virtual void doApply();
    virtual void doUnapply();

private:
    DOM::ElementImpl *m_element;
    DOM::ElementImpl *m_dummySpan;
};

//------------------------------------------------------------------------------------------
// SplitElementCommand

class SplitElementCommand : public EditCommand
{
public:
    SplitElementCommand(DOM::DocumentImpl *, DOM::ElementImpl *element, DOM::NodeImpl *atChild);
    virtual ~SplitElementCommand();
	
    virtual void doApply();
    virtual void doUnapply();

private:
    DOM::ElementImpl *m_element1;
    DOM::ElementImpl *m_element2;
    DOM::NodeImpl *m_atChild;
};

//------------------------------------------------------------------------------------------
// MergeIdenticalElementsCommand

class MergeIdenticalElementsCommand : public EditCommand
{
public:
    MergeIdenticalElementsCommand(DOM::DocumentImpl *, DOM::ElementImpl *first, DOM::ElementImpl *second);
    virtual ~MergeIdenticalElementsCommand();
	
    virtual void doApply();
    virtual void doUnapply();

private:
    DOM::ElementImpl *m_element1;
    DOM::ElementImpl *m_element2;
    DOM::NodeImpl *m_atChild;
};

//------------------------------------------------------------------------------------------
// SplitTextNodeContainingElementCommand

class SplitTextNodeContainingElementCommand : public CompositeEditCommand
{
public:
    SplitTextNodeContainingElementCommand(DOM::DocumentImpl *, DOM::TextImpl *, long);
    virtual ~SplitTextNodeContainingElementCommand();
	
    virtual void doApply();

private:
    DOM::TextImpl *m_text;
    long m_offset;
};


//------------------------------------------------------------------------------------------
// TypingCommand

class TypingCommand : public CompositeEditCommand
{
public:
    enum ETypingCommand { 
        DeleteKey, 
        ForwardDeleteKey, 
        InsertText, 
        InsertLineBreak, 
        InsertParagraphSeparator,
        InsertParagraphSeparatorInQuotedContent,
    };

    TypingCommand(DOM::DocumentImpl *document, ETypingCommand, const DOM::DOMString &text = "", bool selectInsertedText = false);

    static void deleteKeyPressed(DOM::DocumentImpl *, bool smartDelete = false);
    static void forwardDeleteKeyPressed(DOM::DocumentImpl *, bool smartDelete = false);
    static void insertText(DOM::DocumentImpl *, const DOM::DOMString &, bool selectInsertedText = false);
    static void insertLineBreak(DOM::DocumentImpl *);
    static void insertParagraphSeparator(DOM::DocumentImpl *);
    static void insertParagraphSeparatorInQuotedContent(DOM::DocumentImpl *);
    static bool isOpenForMoreTypingCommand(const EditCommandPtr &);
    static void closeTyping(const EditCommandPtr &);
    
    virtual void doApply();
    virtual EditAction editingAction() const;

    bool openForMoreTyping() const { return m_openForMoreTyping; }
    void closeTyping() { m_openForMoreTyping = false; }

    void insertText(const DOM::DOMString &text, bool selectInsertedText);
    void insertTextRunWithoutNewlines(const DOM::DOMString &text, bool selectInsertedText);
    void insertLineBreak();
    void insertParagraphSeparatorInQuotedContent();
    void insertParagraphSeparator();
    void deleteKeyPressed();
    void forwardDeleteKeyPressed();

    bool smartDelete() { return m_smartDelete; }
    void setSmartDelete(bool smartDelete) { m_smartDelete = smartDelete; }

private:
    virtual bool isTypingCommand() const;
    virtual bool preservesTypingStyle() const;

    void markMisspellingsAfterTyping();
    void typingAddedToOpenCommand();
    
    ETypingCommand m_commandType;
    DOM::DOMString m_textToInsert;
    bool m_openForMoreTyping;
    bool m_applyEditing;
    bool m_selectInsertedText;
    bool m_smartDelete;
};

//------------------------------------------------------------------------------------------

DOM::ElementImpl *floatRefdElement(DOM::ElementImpl *element);
DOM::ElementImpl *createDefaultParagraphElement(DOM::DocumentImpl *document);
DOM::ElementImpl *createBlockPlaceholderElement(DOM::DocumentImpl *document);
DOM::ElementImpl *createBreakElement(DOM::DocumentImpl *document);
DOM::ElementImpl *createFontElement(DOM::DocumentImpl *document);
DOM::ElementImpl *createStyleSpanElement(DOM::DocumentImpl *document);

bool isNodeRendered(const DOM::NodeImpl *);
bool isProbablyBlock(const DOM::NodeImpl *);
bool isProbablyTableStructureNode(const DOM::NodeImpl *);
bool isMailBlockquote(const DOM::NodeImpl *);
DOM::NodeImpl *nearestMailBlockquote(const DOM::NodeImpl *);
bool isMailPasteAsQuotationNode(const DOM::NodeImpl *node);

//------------------------------------------------------------------------------------------

} // end namespace khtml

#endif
