/*
 * Copyright (c) 2014, Opera Software ASA. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Opera Software ASA nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "RenderBox.h"
#include "RenderMultiColumnFlow.h"

namespace WebCore {

class RenderMultiColumnSpannerPlaceholder final : public RenderBox {
    WTF_MAKE_ISO_ALLOCATED(RenderMultiColumnSpannerPlaceholder);
public:
    static RenderPtr<RenderMultiColumnSpannerPlaceholder> createAnonymous(RenderMultiColumnFlow&, RenderBox& spanner, const RenderStyle& parentStyle);

    RenderBox* spanner() const { return m_spanner.get(); }
    RenderMultiColumnFlow* fragmentedFlow() const { return m_fragmentedFlow.get(); }

private:
    template<class T, class... Args> friend RenderPtr<T> createRenderer(Args&&...);

    RenderMultiColumnSpannerPlaceholder(RenderMultiColumnFlow&, RenderBox& spanner, RenderStyle&&);

    bool canHaveChildren() const override { return false; }
    void paint(PaintInfo&, const LayoutPoint&) override { }
    ASCIILiteral renderName() const override;

    SingleThreadWeakPtr<RenderBox> m_spanner;
    SingleThreadWeakPtr<RenderMultiColumnFlow> m_fragmentedFlow;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_RENDER_OBJECT(RenderMultiColumnSpannerPlaceholder, isRenderMultiColumnSpannerPlaceholder())
