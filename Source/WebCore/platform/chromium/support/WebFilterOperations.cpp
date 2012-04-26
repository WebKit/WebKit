/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "FilterOperations.h"

#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <wtf/RefPtr.h>

using namespace WebCore;

namespace WebKit {

void WebFilterOperations::initialize()
{
    m_private.reset(new FilterOperations());
}

void WebFilterOperations::append(const WebFilterOperation& filter)
{
    switch (filter.type) {
    case WebFilterOperation::FilterTypeBasicColorMatrix: {
        const WebBasicColorMatrixFilterOperation& colorFilter = static_cast<const WebBasicColorMatrixFilterOperation&>(filter);
        m_private->operations().append(BasicColorMatrixFilterOperation::create(colorFilter.amount, static_cast<FilterOperation::OperationType>(colorFilter.subtype)));
        break;
    }
    case WebFilterOperation::FilterTypeBasicComponentTransfer: {
        const WebBasicComponentTransferFilterOperation& componentFilter = static_cast<const WebBasicComponentTransferFilterOperation&>(filter);
        m_private->operations().append(BasicComponentTransferFilterOperation::create(componentFilter.amount, static_cast<FilterOperation::OperationType>(componentFilter.subtype)));
        break;
    }
    case WebFilterOperation::FilterTypeBlur: {
        const WebBlurFilterOperation& blurFilter = static_cast<const WebBlurFilterOperation&>(filter);
        m_private->operations().append(BlurFilterOperation::create(Length(blurFilter.pixelRadius, Fixed), FilterOperation::BLUR));
        break;
    }
    case WebFilterOperation::FilterTypeDropShadow: {
        const WebDropShadowFilterOperation& shadowFilter = static_cast<const WebDropShadowFilterOperation&>(filter);
        m_private->operations().append(DropShadowFilterOperation::create(IntPoint(shadowFilter.x, shadowFilter.y), shadowFilter.stdDeviation, shadowFilter.color, FilterOperation::DROP_SHADOW));
        break;
    }
    }
}

void WebFilterOperations::clear()
{
    m_private->operations().clear();
}

const FilterOperations& WebFilterOperations::toFilterOperations() const
{
    return *m_private.get();
}

} // namespace WebKit
