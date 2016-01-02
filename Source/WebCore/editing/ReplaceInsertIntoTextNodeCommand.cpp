/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ReplaceInsertIntoTextNodeCommand.h"

#include "AXObjectCache.h"
#include "Document.h"
#include "Text.h"

namespace WebCore {

ReplaceInsertIntoTextNodeCommand::ReplaceInsertIntoTextNodeCommand(RefPtr<Text>&& node, unsigned offset, const String& text, const String& deletedText, EditAction editingAction)
    : InsertIntoTextNodeCommand(WTFMove(node), offset, text, editingAction)
    , m_deletedText(deletedText)
{
}

void ReplaceInsertIntoTextNodeCommand::notifyAccessibilityForTextChange(Node* node, AXTextEditType type, const String& text, const VisiblePosition& position)
{
    if (!shouldPostAccessibilityNotification())
        return;
    AXObjectCache* cache = document().existingAXObjectCache();
    if (!cache)
        return;
    switch (type) {
    case AXTextEditTypeAttributesChange:
    case AXTextEditTypeCut:
    case AXTextEditTypeUnknown:
        break;
    case AXTextEditTypeDelete:
        cache->postTextReplacementNotification(node, AXTextEditTypeDelete, text, AXTextEditTypeInsert, m_deletedText, position);
        break;
    case AXTextEditTypeDictation:
    case AXTextEditTypeInsert:
    case AXTextEditTypePaste:
    case AXTextEditTypeTyping:
        cache->postTextReplacementNotification(node, AXTextEditTypeDelete, m_deletedText, type, text, position);
        break;
    }
}

} // namespace WebCore
