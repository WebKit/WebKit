/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "RenderBlockFlow.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class HTMLSelectElement;
class RenderText;
class SelectFallbackButtonElement;

class RenderSelectFallbackButton final : public RenderBlockFlow {
    WTF_MAKE_TZONE_ALLOCATED(RenderSelectFallbackButton);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderSelectFallbackButton);
public:
    RenderSelectFallbackButton(SelectFallbackButtonElement&, RenderStyle&&);

    SelectFallbackButtonElement& selectFallbackButtonElement() const;

    // CheckedPtr interface.
    uint32_t checkedPtrCount() const { return RenderBlockFlow::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const { return RenderBlockFlow::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const { RenderBlockFlow::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const { RenderBlockFlow::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    void setText(const String&);
    void updateFromElement() final;
#if !PLATFORM(COCOA)
    void setTextFromOption(int optionIndex);
#endif

private:
    void insertedIntoTree() final;

    ASCIILiteral renderName() const final { return "RenderSelectFallbackButton"_s; }

    SingleThreadWeakPtr<RenderText> m_buttonText;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderSelectFallbackButton, isRenderSelectFallbackButton())
