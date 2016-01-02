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

#ifndef ReplaceInsertIntoTextNodeCommand_h
#define ReplaceInsertIntoTextNodeCommand_h

#include "InsertIntoTextNodeCommand.h"

namespace WebCore {

class ReplaceInsertIntoTextNodeCommand final : public InsertIntoTextNodeCommand {
public:
    static Ref<ReplaceInsertIntoTextNodeCommand> create(RefPtr<Text>&& node, unsigned offset, const String& text, const String& deletedText, EditAction editingAction)
    {
        return adoptRef(*new ReplaceInsertIntoTextNodeCommand(WTFMove(node), offset, text, deletedText, editingAction));
    }

private:
    ReplaceInsertIntoTextNodeCommand(RefPtr<Text>&&, unsigned, const String&, const String&, EditAction);
    virtual void notifyAccessibilityForTextChange(Node*, AXTextEditType, const String&, const VisiblePosition&) override;

    String m_deletedText;
};

} // namespace WebCore

#endif // ReplaceInsertIntoTextNodeCommand_h
