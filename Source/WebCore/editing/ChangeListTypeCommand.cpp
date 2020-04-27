/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ChangeListTypeCommand.h"

#include "Editing.h"
#include "ElementAncestorIterator.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLElement.h"
#include "HTMLOListElement.h"
#include "HTMLUListElement.h"
#include <wtf/Optional.h>
#include <wtf/RefPtr.h>

namespace WebCore {

static Optional<std::pair<ChangeListTypeCommand::Type, Ref<HTMLElement>>> listConversionTypeForSelection(const VisibleSelection& selection)
{
    auto startNode = selection.start().containerNode();
    auto endNode = selection.end().containerNode();
    if (!startNode || !endNode)
        return { };
    auto commonAncestor = commonInclusiveAncestor(*startNode, *endNode);

    RefPtr<HTMLElement> listToReplace;
    if (is<HTMLUListElement>(commonAncestor) || is<HTMLOListElement>(commonAncestor))
        listToReplace = downcast<HTMLElement>(commonAncestor.get());
    else
        listToReplace = enclosingList(commonAncestor.get());

    if (is<HTMLUListElement>(listToReplace))
        return {{ ChangeListTypeCommand::Type::ConvertToOrderedList, listToReplace.releaseNonNull() }};

    if (is<HTMLOListElement>(listToReplace))
        return {{ ChangeListTypeCommand::Type::ConvertToUnorderedList, listToReplace.releaseNonNull() }};

    return WTF::nullopt;
}

Optional<ChangeListTypeCommand::Type> ChangeListTypeCommand::listConversionType(Document& document)
{
    if (auto frame = makeRefPtr(document.frame())) {
        if (auto typeAndElement = listConversionTypeForSelection(frame->selection().selection()))
            return typeAndElement->first;
    }
    return WTF::nullopt;
}

Ref<HTMLElement> ChangeListTypeCommand::createNewList(const HTMLElement& listToReplace)
{
    RefPtr<HTMLElement> list;
    if (m_type == Type::ConvertToOrderedList)
        list = HTMLOListElement::create(document());
    else
        list = HTMLUListElement::create(document());
    list->cloneDataFromElement(listToReplace);
    return list.releaseNonNull();
}

void ChangeListTypeCommand::doApply()
{
    auto typeAndElement = listConversionTypeForSelection(endingSelection());
    if (!typeAndElement || typeAndElement->first != m_type)
        return;

    auto listToReplace = WTFMove(typeAndElement->second);
    auto newList = createNewList(listToReplace);
    insertNodeBefore(newList.copyRef(), listToReplace);
    moveRemainingSiblingsToNewParent(listToReplace->firstChild(), nullptr, newList);
    removeNode(listToReplace);
    setEndingSelection({ Position { newList.ptr(), Position::PositionIsAfterChildren }});
}

} // namespace WebCore
