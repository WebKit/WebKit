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

#include "config.h"
#include "RenderMultiColumnSpannerPlaceholder.h"

#include "RenderMultiColumnFlow.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderMultiColumnSpannerPlaceholder);

RenderPtr<RenderMultiColumnSpannerPlaceholder> RenderMultiColumnSpannerPlaceholder::createAnonymous(RenderMultiColumnFlow& fragmentedFlow, RenderBox& spanner, const RenderStyle& parentStyle)
{
    auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(parentStyle, DisplayType::Block);
    newStyle.setClear(Clear::Both); // We don't want floats in the row preceding the spanner to continue on the other side.
    auto placeholder = createRenderer<RenderMultiColumnSpannerPlaceholder>(fragmentedFlow, spanner, WTFMove(newStyle));
    placeholder->initializeStyle();
    return placeholder;
}

RenderMultiColumnSpannerPlaceholder::RenderMultiColumnSpannerPlaceholder(RenderMultiColumnFlow& fragmentedFlow, RenderBox& spanner, RenderStyle&& style)
    : RenderBox(fragmentedFlow.document(), WTFMove(style), RenderBoxModelObjectFlag)
    , m_spanner(makeWeakPtr(spanner))
    , m_fragmentedFlow(makeWeakPtr(fragmentedFlow))
{
}

const char* RenderMultiColumnSpannerPlaceholder::renderName() const
{
    return "RenderMultiColumnSpannerPlaceholder";
}

} // namespace WebCore
