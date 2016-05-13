/*
 * Copyright (C) 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CompositeEditCommand_h
#define CompositeEditCommand_h

#include "AXObjectCache.h"
#include "EditCommand.h"
#include "CSSPropertyNames.h"
#include "UndoStep.h"
#include <wtf/Vector.h>

namespace WebCore {

class EditingStyle;
class HTMLElement;
class StyledElement;
class Text;

class AccessibilityUndoReplacedText {
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
    bool wasCreateLinkCommand() const { return m_editAction == EditActionCreateLink; }

    const VisibleSelection& startingSelection() const { return m_startingSelection; }
    const VisibleSelection& endingSelection() const { return m_endingSelection; }
    void setStartingSelection(const VisibleSelection&);
    void setEndingSelection(const VisibleSelection&);
    Element* startingRootEditableElement() const { return m_startingRootEditableElement.get(); }
    Element* endingRootEditableElement() const { return m_endingRootEditableElement.get(); }
    void setRangeDeletedByUnapply(const VisiblePositionIndexRange&);

#ifndef NDEBUG
    virtual void getNodesInCommand(HashSet<Node*>&);
#endif

private:
    EditCommandComposition(Document&, const VisibleSelection& startingSelection, const VisibleSelection& endingSelection, EditAction);

    RefPtr<Document> m_document;
    VisibleSelection m_startingSelection;
    VisibleSelection m_endingSelection;
    Vector<RefPtr<SimpleEditCommand>> m_commands;
    RefPtr<Element> m_startingRootEditableElement;
    RefPtr<Element> m_endingRootEditableElement;
    AccessibilityUndoReplacedText m_replacedText;
    EditAction m_editAction;
};

class CompositeEditCommand : public EditCommand {
public:
    virtual ~CompositeEditCommand();

    void apply();
    bool isFirstCommand(EditCommand* command) { return !m_commands.isEmpty() && m_commands.first() == command; }
    EditCommandComposition* composition() { return m_composition.get(); }
    EditCommandComposition* ensureComposition();

    virtual bool isCreateLinkCommand() const;
    virtual bool isTypingCommand() const;
    virtual bool isDictationCommand() const { return false; }
    virtual bool preservesTypingStyle() const;
    virtual bool shouldRetainAutocorrectionIndicator() const;
    virtual void setShouldRetainAutocorrectionIndicator(bool);
    virtual bool shouldStopCaretBlinking() const { return false; }

protected:
    explicit CompositeEditCommand(Document&, EditAction = EditActionUnspecified);

    //
    // sugary-sweet convenience functions to help create and apply edit commands in composite commands
    //
    void appendNode(PassRefPtr<Node>, PassRefPtr<ContainerNode> parent);
    void applyCommandToComposite(PassRefPtr<EditCommand>);
    void applyCommandToComposite(PassRefPtr<CompositeEditCommand>, const VisibleSelection&);
    void applyStyle(const EditingStyle*, EditAction = EditActionChangeAttributes);
    void applyStyle(const EditingStyle*, const Position& start, const Position& end, EditAction = EditActionChangeAttributes);
    void applyStyledElement(PassRefPtr<Element>);
    void removeStyledElement(PassRefPtr<Element>);
    void deleteSelection(bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    void deleteSelection(const VisibleSelection&, bool smartDelete = false, bool mergeBlocksAfterDelete = true, bool replace = false, bool expandForSpecialElements = true, bool sanitizeMarkup = true);
    virtual void deleteTextFromNode(PassRefPtr<Text>, unsigned offset, unsigned count);
    void inputText(const String&, bool selectInsertedText = false);
    bool isRemovableBlock(const Node*);
    void insertNodeAfter(PassRefPtr<Node>, PassRefPtr<Node> refChild);
    void insertNodeAt(PassRefPtr<Node>, const Position&);
    void insertNodeAtTabSpanPosition(PassRefPtr<Node>, const Position&);
    void insertNodeBefore(PassRefPtr<Node>, PassRefPtr<Node> refChild, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void insertParagraphSeparator(bool useDefaultParagraphElement = false, bool pasteBlockqutoeIntoUnquotedArea = false);
    void insertLineBreak();
    void insertTextIntoNode(PassRefPtr<Text>, unsigned offset, const String& text);
    void mergeIdenticalElements(PassRefPtr<Element>, PassRefPtr<Element>);
    void rebalanceWhitespace();
    void rebalanceWhitespaceAt(const Position&);
    void rebalanceWhitespaceOnTextSubstring(PassRefPtr<Text>, int startOffset, int endOffset);
    void prepareWhitespaceAtPositionForSplit(Position&);
    bool canRebalance(const Position&) const;
    bool shouldRebalanceLeadingWhitespaceFor(const String&) const;
    void removeCSSProperty(PassRefPtr<StyledElement>, CSSPropertyID);
    void removeNodeAttribute(PassRefPtr<Element>, const QualifiedName& attribute);
    void removeChildrenInRange(PassRefPtr<Node>, unsigned from, unsigned to);
    virtual void removeNode(PassRefPtr<Node>, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    HTMLElement* replaceElementWithSpanPreservingChildrenAndAttributes(PassRefPtr<HTMLElement>);
    void removeNodePreservingChildren(PassRefPtr<Node>, ShouldAssumeContentIsAlwaysEditable = DoNotAssumeContentIsAlwaysEditable);
    void removeNodeAndPruneAncestors(PassRefPtr<Node>);
    void moveRemainingSiblingsToNewParent(Node*, Node* pastLastNodeToMove, PassRefPtr<Element> prpNewParent);
    void updatePositionForNodeRemovalPreservingChildren(Position&, Node&);
    void prune(PassRefPtr<Node>);
    void replaceTextInNode(PassRefPtr<Text>, unsigned offset, unsigned count, const String& replacementText);
    Position replaceSelectedTextInNode(const String&);
    void replaceTextInNodePreservingMarkers(PassRefPtr<Text>, unsigned offset, unsigned count, const String& replacementText);
    Position positionOutsideTabSpan(const Position&);
    void setNodeAttribute(PassRefPtr<Element>, const QualifiedName& attribute, const AtomicString& value);
    void splitElement(PassRefPtr<Element>, PassRefPtr<Node> atChild);
    void splitTextNode(PassRefPtr<Text>, unsigned offset);
    void splitTextNodeContainingElement(PassRefPtr<Text>, unsigned offset);
    void wrapContentsInDummySpan(Element&);

    void deleteInsignificantText(PassRefPtr<Text>, unsigned start, unsigned end);
    void deleteInsignificantText(const Position& start, const Position& end);
    void deleteInsignificantTextDownstream(const Position&);

    RefPtr<Node> appendBlockPlaceholder(PassRefPtr<Element>);
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
    
    bool breakOutOfEmptyListItem();
    bool breakOutOfEmptyMailBlockquotedParagraph();
    
    Position positionAvoidingSpecialElementBoundary(const Position&);
    
    RefPtr<Node> splitTreeToNode(Node*, Node*, bool splitAncestor = false);

    Vector<RefPtr<EditCommand>> m_commands;

private:
    bool isCompositeEditCommand() const override { return true; }

    RefPtr<EditCommandComposition> m_composition;
};
    
void applyCommand(PassRefPtr<CompositeEditCommand>);

inline CompositeEditCommand* toCompositeEditCommand(EditCommand* command)
{
    ASSERT(command);
    ASSERT_WITH_SECURITY_IMPLICATION(command->isCompositeEditCommand());
    return static_cast<CompositeEditCommand*>(command);
}

} // namespace WebCore

#endif // CompositeEditCommand_h
