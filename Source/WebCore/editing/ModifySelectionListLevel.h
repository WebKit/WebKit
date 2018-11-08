/*
 * Copyright (C) 2006, 2010 Apple Inc. All rights reserved.
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

#include "CompositeEditCommand.h"

namespace WebCore {

// ModifySelectionListLevelCommand provides functions useful for both increasing and decreasing the list level.
// It is the base class of IncreaseSelectionListLevelCommand and DecreaseSelectionListLevelCommand.
// It is not used on its own.
class ModifySelectionListLevelCommand : public CompositeEditCommand {
protected:
    explicit ModifySelectionListLevelCommand(Document&);
    
    void appendSiblingNodeRange(Node* startNode, Node* endNode, Element* newParent);
    void insertSiblingNodeRangeBefore(Node* startNode, Node* endNode, Node* refNode);
    void insertSiblingNodeRangeAfter(Node* startNode, Node* endNode, Node* refNode);

private:
    bool preservesTypingStyle() const override;
};

// IncreaseSelectionListLevelCommand moves the selected list items one level deeper.
class IncreaseSelectionListLevelCommand : public ModifySelectionListLevelCommand {
public:
    enum class Type : uint8_t { InheritedListType, OrderedList, UnorderedList };
    static Ref<IncreaseSelectionListLevelCommand> create(Document& document, Type type)
    {
        return adoptRef(*new IncreaseSelectionListLevelCommand(document, type));
    }

    static bool canIncreaseSelectionListLevel(Document*);
    static RefPtr<Node> increaseSelectionListLevel(Document*);
    static RefPtr<Node> increaseSelectionListLevelOrdered(Document*);
    static RefPtr<Node> increaseSelectionListLevelUnordered(Document*);

private:
    static RefPtr<Node> increaseSelectionListLevel(Document*, Type);
    
    IncreaseSelectionListLevelCommand(Document&, Type);

    void doApply() override;

    Type m_listType;
    RefPtr<Node> m_listElement;
};

// DecreaseSelectionListLevelCommand moves the selected list items one level shallower.
class DecreaseSelectionListLevelCommand : public ModifySelectionListLevelCommand {
public:
    static bool canDecreaseSelectionListLevel(Document*);
    static void decreaseSelectionListLevel(Document*);

private:
    static Ref<DecreaseSelectionListLevelCommand> create(Document& document)
    {
        return adoptRef(*new DecreaseSelectionListLevelCommand(document));
    }

    explicit DecreaseSelectionListLevelCommand(Document&);

    void doApply() override;
};

} // namespace WebCore
