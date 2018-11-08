/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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

class HTMLElement;
class HTMLQualifiedName;

class InsertListCommand final : public CompositeEditCommand {
public:
    enum class Type : uint8_t { OrderedList, UnorderedList };

    static Ref<InsertListCommand> create(Document& document, Type listType)
    {
        return adoptRef(*new InsertListCommand(document, listType));
    }

    static RefPtr<HTMLElement> insertList(Document&, Type);
    
    bool preservesTypingStyle() const final { return true; }

private:
    InsertListCommand(Document&, Type);

    void doApply() final;
    EditAction editingAction() const final;

    HTMLElement& fixOrphanedListChild(Node&);
    bool selectionHasListOfType(const VisibleSelection& selection, const QualifiedName&);
    Ref<HTMLElement> mergeWithNeighboringLists(HTMLElement&);
    void doApplyForSingleParagraph(bool forceCreateList, const HTMLQualifiedName&, Range* currentSelection);
    void unlistifyParagraph(const VisiblePosition& originalStart, HTMLElement* listNode, Node* listChildNode);
    RefPtr<HTMLElement> listifyParagraph(const VisiblePosition& originalStart, const QualifiedName& listTag);
    RefPtr<HTMLElement> m_listElement;
    Type m_type;
};

} // namespace WebCore
