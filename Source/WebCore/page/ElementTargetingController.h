/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "ElementIdentifier.h"
#include "ElementTargetingTypes.h"
#include "EventTarget.h"
#include "IntRect.h"
#include "Region.h"
#include "ScriptExecutionContextIdentifier.h"
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/Ref.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Page;

class ElementTargetingController : public CanMakeCheckedPtr {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ElementTargetingController(Page&);

    WEBCORE_EXPORT Vector<TargetedElementInfo> findTargets(TargetedElementRequest&&);

    WEBCORE_EXPORT bool adjustVisibility(const Vector<std::pair<ElementIdentifier, ScriptExecutionContextIdentifier>>&);
    void adjustVisibilityInRepeatedlyTargetedRegions(Document&);

    void resetAdjustmentRegions();

    WEBCORE_EXPORT uint64_t numberOfVisibilityAdjustmentRects() const;
    WEBCORE_EXPORT bool resetVisibilityAdjustments(const Vector<std::pair<ElementIdentifier, ScriptExecutionContextIdentifier>>&);

private:
    SingleThreadWeakPtr<Page> m_page;
    HashMap<ElementIdentifier, IntRect> m_pendingAdjustmentClientRects;
    Region m_adjustmentClientRegion;
    Region m_repeatedAdjustmentClientRegion;
    WeakHashSet<Element, WeakPtrImplWithEventTargetData> m_adjustedElements;
    FloatSize m_viewportSizeForVisibilityAdjustment;
};

} // namespace WebCore
