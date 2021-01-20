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

#include "ApplyBlockElementCommand.h"
#include "EditAction.h"

namespace WebCore {

class IndentOutdentCommand : public ApplyBlockElementCommand {
public:
    enum EIndentType { Indent, Outdent };
    static Ref<IndentOutdentCommand> create(Document& document, EIndentType type)
    {
        return adoptRef(*new IndentOutdentCommand(document, type));
    }

    bool preservesTypingStyle() const override { return true; }

private:
    IndentOutdentCommand(Document&, EIndentType);

    EditAction editingAction() const override { return m_typeOfAction == Indent ? EditAction::Indent : EditAction::Outdent; }

    void indentRegion(const VisiblePosition&, const VisiblePosition&);
    void outdentRegion(const VisiblePosition&, const VisiblePosition&);
    void outdentParagraph();
    bool tryIndentingAsListItem(const Position&, const Position&);
    void indentIntoBlockquote(const Position&, const Position&, RefPtr<Element>&);

    void formatSelection(const VisiblePosition& startOfSelection, const VisiblePosition& endOfSelection) override;
    void formatRange(const Position& start, const Position& end, const Position& endOfSelection, RefPtr<Element>& blockquoteForNextIndent) override;

    EIndentType m_typeOfAction;
};

} // namespace WebCore
