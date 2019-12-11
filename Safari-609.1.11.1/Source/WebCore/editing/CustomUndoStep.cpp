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
#include "CustomUndoStep.h"

#include "Document.h"
#include "UndoItem.h"
#include "UndoManager.h"
#include "VoidCallback.h"

namespace WebCore {

CustomUndoStep::CustomUndoStep(UndoItem& item)
    : m_undoItem(makeWeakPtr(item))
{
}

void CustomUndoStep::unapply()
{
    if (!isValid())
        return;

    // FIXME: It's currently unclear how input events should be dispatched when unapplying or reapplying custom
    // edit commands. Should the page be allowed to specify a target in the DOM for undo and redo?
    Ref<UndoItem> protectedUndoItem(*m_undoItem);
    protectedUndoItem->document()->updateLayoutIgnorePendingStylesheets();
    protectedUndoItem->undoHandler().handleEvent();
}

void CustomUndoStep::reapply()
{
    if (!isValid())
        return;

    Ref<UndoItem> protectedUndoItem(*m_undoItem);
    protectedUndoItem->document()->updateLayoutIgnorePendingStylesheets();
    protectedUndoItem->redoHandler().handleEvent();
}

bool CustomUndoStep::isValid() const
{
    return m_undoItem && m_undoItem->isValid();
}

String CustomUndoStep::label() const
{
    if (!isValid()) {
        ASSERT_NOT_REACHED();
        return emptyString();
    }
    return m_undoItem->label();
}

void CustomUndoStep::didRemoveFromUndoManager()
{
    if (auto undoItem = std::exchange(m_undoItem, nullptr))
        undoItem->invalidate();
}

} // namespace WebCore
