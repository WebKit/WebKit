/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "CSSBorderImageSliceValue.h"

#include "PlatformString.h"
#include "Rect.h"

namespace WebCore {

CSSBorderImageSliceValue::CSSBorderImageSliceValue(PassRefPtr<Rect> sliceRect, bool fill)
    : m_rect(sliceRect)
    , m_fill(fill)
{
}

CSSBorderImageSliceValue::~CSSBorderImageSliceValue()
{
}

String CSSBorderImageSliceValue::cssText() const
{
    // This isn't really a CSS rect, so we dump manually
    String text = m_rect->top()->cssText();
    if (m_rect->right() != m_rect->top() || m_rect->bottom() != m_rect->top() || m_rect->left() != m_rect->top()) {
        text += " ";
        text += m_rect->right()->cssText();
        if (m_rect->bottom() != m_rect->top() || m_rect->right() != m_rect->left()) {
            text += " ";
            text += m_rect->bottom()->cssText();
            if (m_rect->left() != m_rect->right()) {
                text += " ";
                text += m_rect->left()->cssText();
            }
        }
    }

    // Now the fill keywords if it is present.
    if (m_fill)
        text += " fill";
    return text;
}

} // namespace WebCore
