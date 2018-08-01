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
#include "FloatingState.h"

#if ENABLE(LAYOUT_FORMATTING_CONTEXT)

#include "LayoutBox.h"
#include "LayoutContext.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {
namespace Layout {

WTF_MAKE_ISO_ALLOCATED_IMPL(FloatingState);

FloatingState::FloatingState(LayoutContext& layoutContext, const Box& formattingContextRoot)
    : m_layoutContext(layoutContext)
    , m_formattingContextRoot(makeWeakPtr(const_cast<Box&>(formattingContextRoot)))
{
}

#ifndef NDEBUG
static bool belongsToThisFloatingContext(const Box& layoutBox, const Box& floatingStateRoot)
{
    auto& formattingContextRoot = layoutBox.formattingContextRoot();
    if (&formattingContextRoot == &floatingStateRoot)
        return true;

    // Maybe the layout box belongs to an inline formatting context that inherits the floating state from the parent (block) formatting context. 
    if (!formattingContextRoot.establishesInlineFormattingContext())
        return false;

    return &formattingContextRoot.formattingContextRoot() == &floatingStateRoot;
}
#endif

void FloatingState::remove(const Box& layoutBox)
{
    for (size_t index = 0; index < m_floatings.size(); ++index) {
        if (m_floatings[index].get() == &layoutBox) {
            m_floatings.remove(index);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void FloatingState::append(const Box& layoutBox)
{
    ASSERT(is<Container>(*m_formattingContextRoot));
    ASSERT(belongsToThisFloatingContext(layoutBox, *m_formattingContextRoot));

    // Floating state should hold boxes with computed position/size.
    ASSERT(m_layoutContext.displayBoxForLayoutBox(layoutBox));
    m_floatings.append(makeWeakPtr(const_cast<Box&>(layoutBox)));
}

}
}
#endif
