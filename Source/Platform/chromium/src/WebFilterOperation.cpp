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
#include <public/WebFilterOperation.h>

#include <string.h>

namespace WebKit {

bool WebFilterOperation::equals(const WebFilterOperation& other) const
{
    if (m_type != other.m_type)
        return false;
    if (m_type == FilterTypeColorMatrix)
        return !memcmp(m_matrix, other.m_matrix, sizeof(m_matrix));
    if (m_type == FilterTypeDropShadow) {
        return m_amount == other.m_amount
            && m_dropShadowOffset == other.m_dropShadowOffset
            && m_dropShadowColor == other.m_dropShadowColor;
    } else
        return m_amount == other.m_amount;
}

WebFilterOperation::WebFilterOperation(FilterType type, SkScalar matrix[20])
    : m_type(type)
    , m_amount(0)
    , m_dropShadowOffset(0, 0)
    , m_dropShadowColor(0)
    , m_zoomInset(0)
{
    WEBKIT_ASSERT(m_type == FilterTypeColorMatrix);
    memcpy(m_matrix, matrix, sizeof(m_matrix));
}

} // namespace WebKit
