/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DeprecatedCSSOMPrimitiveValue.h"
#include "Rect.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class DeprecatedCSSOMRect final : public RefCounted<DeprecatedCSSOMRect> {
public:
    static Ref<DeprecatedCSSOMRect> create(const Rect& rect, CSSStyleDeclaration& owner)
    {
        return adoptRef(*new DeprecatedCSSOMRect(rect, owner));
    }

    DeprecatedCSSOMPrimitiveValue* top() const { return m_top.get(); }
    DeprecatedCSSOMPrimitiveValue* right() const { return m_right.get(); }
    DeprecatedCSSOMPrimitiveValue* bottom() const { return m_bottom.get(); }
    DeprecatedCSSOMPrimitiveValue* left() const { return m_left.get(); }
    
private:
    DeprecatedCSSOMRect(const Rect& rect, CSSStyleDeclaration& owner)
    {
        if (rect.top())
            m_top = rect.top()->createDeprecatedCSSOMPrimitiveWrapper(owner);
        if (rect.right())
            m_right = rect.right()->createDeprecatedCSSOMPrimitiveWrapper(owner);
        if (rect.bottom())
            m_bottom = rect.bottom()->createDeprecatedCSSOMPrimitiveWrapper(owner);
        if (rect.left())
            m_left = rect.left()->createDeprecatedCSSOMPrimitiveWrapper(owner);
    }
    
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_top;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_right;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_bottom;
    RefPtr<DeprecatedCSSOMPrimitiveValue> m_left;
};

} // namespace WebCore
