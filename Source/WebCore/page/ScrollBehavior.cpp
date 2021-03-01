/*
 * Copyright (C) 2019 Igalia S.L. All rights reserved.
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

#include "config.h"
#include "ScrollBehavior.h"

#include "Element.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "Settings.h"

namespace WebCore {

bool useSmoothScrolling(ScrollBehavior behavior, Element* associatedElement)
{
    if (!associatedElement)
        return false;

    // FIXME: Should we use document()->scrollingElement()?
    // See https://bugs.webkit.org/show_bug.cgi?id=205059
    if (associatedElement == associatedElement->document().scrollingElement())
        associatedElement = associatedElement->document().documentElement();

    if (!associatedElement->renderer() || !associatedElement->document().settings().CSSOMViewSmoothScrollingEnabled())
        return false;

    // https://drafts.csswg.org/cssom-view/#scrolling
    switch (behavior) {
    case ScrollBehavior::Auto:
        return associatedElement->renderer()->style().useSmoothScrolling();
    case ScrollBehavior::Instant:
        return false;
    case ScrollBehavior::Smooth:
        return true;
    }
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore
