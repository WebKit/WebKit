/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "PositionTryOrder.h"

namespace WebCore {
namespace Style {

LogicalBoxAxis boxAxisForPositionTryOrder(PositionTryOrder order, WritingMode writingMode)
{
    switch (order) {
    case PositionTryOrder::MostWidth:
        return mapAxisPhysicalToLogical(writingMode, BoxAxis::Horizontal);
    case PositionTryOrder::MostHeight:
        return mapAxisPhysicalToLogical(writingMode, BoxAxis::Vertical);
    case PositionTryOrder::MostBlockSize:
        return LogicalBoxAxis::Block;
    case PositionTryOrder::MostInlineSize:
        return LogicalBoxAxis::Inline;
    case PositionTryOrder::Normal:
        break;
    }
    ASSERT_NOT_REACHED();
    return LogicalBoxAxis::Inline;
}

WTF::TextStream& operator<<(WTF::TextStream& ts, PositionTryOrder order)
{
    switch (order) {
    case PositionTryOrder::Normal: ts << "normal"_s; break;
    case PositionTryOrder::MostWidth: ts << "most-width"_s; break;
    case PositionTryOrder::MostHeight: ts << "most-height"_s; break;
    case PositionTryOrder::MostBlockSize: ts << "most-block-size"_s; break;
    case PositionTryOrder::MostInlineSize: ts << "most-inline-size"_s; break;
    }

    return ts;
}

}
}
