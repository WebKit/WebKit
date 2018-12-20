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

#pragma once

#include "CompositeEditCommand.h"
#include "EditAction.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class HTMLElement;

class ChangeListTypeCommand final : public CompositeEditCommand {
public:
    enum class Type : uint8_t { ConvertToOrderedList, ConvertToUnorderedList };
    static Optional<Type> listConversionType(Document&);
    static Ref<ChangeListTypeCommand> create(Document& document, Type type)
    {
        return adoptRef(*new ChangeListTypeCommand(document, type));
    }

private:
    ChangeListTypeCommand(Document& document, Type type)
        : CompositeEditCommand(document)
        , m_type(type)
    {
    }

    EditAction editingAction() const final
    {
        if (m_type == Type::ConvertToOrderedList)
            return EditAction::ConvertToOrderedList;

        return EditAction::ConvertToUnorderedList;
    }

    bool preservesTypingStyle() const final { return true; }
    void doApply() final;

    Ref<HTMLElement> createNewList(const HTMLElement& listToReplace);

    Type m_type;
};

} // namespace WebCore
