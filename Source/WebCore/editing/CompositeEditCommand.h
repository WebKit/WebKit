/*
 * Copyright (C) 2005-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "AXObjectCache.h"
#include "EditCommand.h"
#include "CSSPropertyNames.h"
#include "UndoStep.h"
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class EditingStyle;
class DataTransfer;
class HTMLElement;
class StaticRange;
class StyledElement;
class Text;

class AccessibilityUndoReplacedText {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AccessibilityUndoReplacedText() { }
    void configureRangeDeletedByReapplyWithStartingSelection(const VisibleSelection&);
    void configureRangeDeletedByReapplyWithEndingSelection(const VisibleSelection&);
    void setRangeDeletedByUnapply(const VisiblePositionIndexRange&);

    void captureTextForUnapply();
    void captureTextForReapply();

    void postTextStateChangeNotificationForUnapply(AXObjectCache*);
    void postTextStateChangeNotificationForReapply(AXObjectCache*);

private:
    int indexForVisiblePosition(const VisiblePosition&, RefPtr<ContainerNode>&) const;
    String textDeletedByUnapply();
    String textDeletedByReapply();

    String m_replacedText;
    VisiblePositionIndexRange m_rangeDeletedByUnapply;
    VisiblePositionIndexRange m_rangeDeletedByReapply;
};

class EditCommandComposition : public UndoStep {
public:
    static Ref<EditCommandComposition> create(Document&, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction);

    void unapply() override;
    void reapply() override;
    EditAction editingAction() const override { return m_editAction; }
    void append(SimpleEditCommand*);
    bool wasCreateLinkCommand() const { return m_editAction == EditAction::CreateLink; }

    const VisibleSelection& startingSelection() const { return m_startingSelection; }
    const VisibleSelection& endingSelection() const { return m_endingSelection; }
    void setStartingSelection(const VisibleSelection&);
    void setEndingSelection(const VisibleSelection&);
    Element* startingRootEditableElement() const { return m_startingRootEditableElement.get(); }
    Element* endingRootEditableElement() const { return m_endingRootEditableElement.get(); }
    void setRangeDeletedByUnapply(const VisiblePositionIndexRange&);

#ifndef NDEBUG
    virtual void getNodesInCommand(HashSet<Ref<Node>>&);
#endif

private:
    EditCommandComposition(Document&, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction);

    String label() const final;
    void didRemoveFromUndoManager() final { }
    bool areRootEditabledElementsConnected();

    RefPtr<Document> m_document;
    VisibleSelection m_startingSelection;
    VisibleSelection m_endingSelection;
    Vector<RefPtr<SimpleEditCommand>> m_commands;
    RefPtr<Element> m_startingRootEditableElement;
    RefPtr<Element> m_endingRootEditableElement;
    AccessibilityUndoReplacedText m_replacedText;
    EditAction m_editAction;
};

class CompositeEditCommand : public EditCommand, public CanMakeWeakPtr<CompositeEditCommand> {
public:
    virtual ~CompositeEditCommand();

    void apply();
    bool isFirstCommand(EditCommand* command) { return !m_commands.isEmpty() && m_commands.first() == command; }
    EditCommandComposition* composition() const;
    EditCommandComposition& ensureComposition();

    virtual bool isCreateLinkCommand() const;
    virtual bool isTypingCommand() const;
    virtual bool isDictationCommand() const { return false; }
    virtual bool preservesTypingStyle() const;
    virtual bool shouldRetainAutocorrectionIndicator() const;
    virtual void setShouldRetainAutocorrectionIndicator(bool);
    virtual bool shouldStopCaretBlinking() const { return false; }
    virtual AtomString inputEventTypeName() const;
    virtual bool isInputMethodComposing() const;
    virtual String inputEventData() const { return { }; }
    virtual bool isBeforeInputEventCancelable() const { return true; }
    virtual bool shouldDispatchInputEvents() const { return true; }
    Vector<RefPtr<StaticRange>> targetRangesForBindings() const;
    virtual RefPtr<DataTransfer> inputEventDataTransfer() const;

protected:
    explicit CompositeEditCommand(Document&, EditAction = EditAction::Unspecified);

    // If willApplyCommand returns false, we won't proceed with applying the command.
    virtual bool willApplyCommand();
    virtual void didApplyCommand();

    virtual Vector<RefPtr<StaticRange>> targetRanges() const;

    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(Ref<Node>&&, Ref<ContainerNode>&& parent);
    void applyCommandToComposite(Ref<EditCommand>&&);
    void applyCommandToComposite(Ref<CompositeEditCommand>&&, const VisibleSelection&);
    void applyStyle(const EditingStyle*, EditAction = EditAction::ChangeAttributes);
    void applyStyle(const EditingStyle*, const Position& start, const Position& end, EditAction = EditAction::ChangeAttributes);
    void applyStyledElement(Ref<Element>&&);
    void removeStyledElement(Ref<Element>&&);
    void deleteSelection(bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    void deleteSelection(const VisibleSelection&, bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    virtual void deleteTextFromNode(Text&, unsigned offset, unsigned count);
    void inputText(const String&, bool selectInsertedText = false);
    bool isRemovableBlock(const Node*);
    void insertNodeAfter(Ref<Node>&&, Node& refChild);
    void insertNodeAt(Ref<Node>&&, const Position&);
    void insertNodeAtTabSpanPosition(Ref<Node>&&, const Position&);
    bool insertNodeBefore(Ref<Node>&&, Node& refChild, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void insertParagraphSeparatorAtPosition(const Position&, bool useDefaultParagraphElement = false, bool pasteBlockqutoeIntoUnquotedArea = false);
    void insertParagraphSeparator(bool useDefaultParagraphElement = false, bool pasteBlockqutoeIntoUnquotedArea = false);
    void insertLineBreak();
    void insertTextIntoNode(Text&, unsigned offset, const String& text);
    void mergeIdenticalElements(Element&, Element&);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const Position&);
    void rebalanceWhitespaceOnTextSubstring(Text&, int startOffset, int endOffset);
    void prepareWhitespaceAtPositionForSplit(Position&);
    void replaceCollapsibleWhitespaceWithNonBreakingSpaceIfNeeded(const VisiblePosition&);
    RefPtr<Text> textNodeForRebalance(const Position&) const;
    bool shouldRebalanceLeadingWhitespaceFor(const String&) const;
    void removeNodeAttribute(Element&, const QualifiedName& attribute);
    void removeChildrenInRange(Node&, unsigned from, unsigned to);
    virtual void removeNode(Node&, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    HTMLElement* replaceElementWithSpanPreservingChildrenAndAttributes(HTMLElement&);
    void removeNodePreservingChildren(Node&, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void removeNodeAndPruneAncestors(Node&);
    void moveRemainingSiblingsToNewParent(Node*, Node* pastLastNodeToMove, Element& newParent);
    void updatePositionForNodeRemovalPreservingChildren(Position&, Node&);
    void prune(Node*);
    void replaceTextInNode(Text&, unsigned offset, unsigned count, const String& replacementText);
    Position replaceSelectedTextInNode(const String&);
    void replaceTextInNodePreservingMarkers(Text&, unsigned offset, unsigned count, const String& replacementText);
    Position positionOutsideTabSpan(const Position&);
    void setNodeAttribute(Element&, const QualifiedName& attribute, const AtomString& value);
    void splitElement(Element&, Node& atChild);
    void splitTextNode(Text&, unsigned offset);
    void splitTextNodeContainingElement(Text&, unsigned offset);
    void wrapContentsInDummySpan(Element&);

    void deleteInsignificantText(Text&, unsigned start, unsigned end);
    void deleteInsignificantText(const Position& start, const Position& end);
    void deleteInsignificantTextDownstream(const Position&);

    RefPtr<Element> appendBlockPlaceholder(Ref<Element>&&);
    RefPtr<Node> insertBlockPlaceholder(const Position&);
    RefPtr<Node> addBlockPlaceholderIfNeeded(Element*);
    void removePlaceholderAt(const Position&);

    Ref<HTMLElement> insertNewDefaultParagraphElementAt(const Position&);

    RefPtr<Node> moveParagraphContentsToNewBlockIfNecessary(const Position&);
    
    void pushAnchorElementDown(Element&);
    
    void moveParagraph(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    void moveParagraphs(const VisiblePosition&, const VisiblePosition&, const VisiblePosition&, bool preserveSelection = false, bool preserveStyle = true);
    void moveParagraphWithClones(const VisiblePosition& startOfParagraphToMove, const VisiblePosition& endOfParagraphToMove, Element* blockElement, Node* outerNode);
    void cloneParagraphUnderNewElement(const Position& start, const Position& end, Node* outerNode, Element* blockElement);
    void cleanupAfterDeletion(VisiblePosition destination = VisiblePosition());
    
    VisibleSelection shouldBreakOutOfEmptyListItem() const;
    bool breakOutOfEmptyListItem();
    bool breakOutOfEmptyMailBlockquotedParagraph();
    
    Position positionAvoidingSpecialElementBoundary(const Position&);
    
    RefPtr<Node> splitTreeToNode(Node&, Node&, bool splitAncestor = false);

    Vector<RefPtr<EditCommand>> m_commands;

private:
    bool isCompositeEditCommand() const override { return true; }

    RefPtr<EditCommandComposition> m_composition;
};

} // namespace WebCore
