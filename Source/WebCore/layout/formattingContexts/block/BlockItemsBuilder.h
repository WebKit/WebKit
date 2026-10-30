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

#include "BlockItem.h"
#include <wtf/CheckedRef.h>

namespace WebCore {

class RenderBlockFlow;

namespace Layout {

// FIXME: Build the list as part of render tree building and keep it until the tree or a box's formatting context participation changes.
class BlockItemsBuilder {
public:
    explicit BlockItemsBuilder(const RenderBlockFlow& root);

    BlockItemList build();
    // The items of a block-level box in the inline content of a block container (CSS 2.2 §9.2.1.1), for when the inline formatting context
    // reaches it.
    BlockItemList buildForBlockLevelBoxInInlineContent(const RenderBox&);

private:
    // From the first box to the end of the content of the root, or of the only box.
    BlockItemList build(const RenderBox& firstBox, const RenderBox* onlyBox);
    const RenderBlockFlow& root() const { return m_root; }

    const CheckedRef<const RenderBlockFlow> m_root;
};

}
}
