/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "UndoManager.h"

#include "CustomUndoStep.h"
#include "Document.h"
#include "Editor.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "UndoItem.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(UndoManager);

UndoManager::UndoManager(Document& document)
    : m_document(document)
{
}

UndoManager::~UndoManager() = default;

ExceptionOr<void> UndoManager::addItem(Ref<UndoItem>&& item)
{
    if (item->undoManager())
        return Exception { InvalidModificationError, "This item has already been added to an UndoManager"_s };

    RefPtr frame = m_document.frame();
    if (!frame)
        return Exception { SecurityError, "A browsing context is required to add an UndoItem"_s };

    item->setUndoManager(this);
    frame->editor().registerCustomUndoStep(CustomUndoStep::create(item));
    m_items.add(WTFMove(item));
    return { };
}

void UndoManager::removeItem(UndoItem& item)
{
    if (auto foundItem = m_items.take(&item))
        foundItem->setUndoManager(nullptr);
}

void UndoManager::removeAllItems()
{
    for (auto& item : m_items)
        item->setUndoManager(nullptr);
    m_items.clear();
}

} // namespace WebCore
