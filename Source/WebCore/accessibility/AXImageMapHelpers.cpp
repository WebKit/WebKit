/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXImageMapHelpers.h"

#include "AXCoreObject.h"
#include "ContainerNodeInlines.h"
#include "ElementAncestorIteratorInlines.h"
#include "HTMLAreaElement.h"
#include "HTMLMapElement.h"

namespace WebCore {

using namespace HTMLNames;

void AXImageMapHelpers::accessibilityText(const HTMLAreaElement& areaElement, String&& description, Vector<AccessibilityText>& textOrder)
{
    if (!description.isEmpty())
        textOrder.append(AccessibilityText(WTFMove(description), AccessibilityTextSource::Alternative));

    const AtomString& titleText = areaElement.getAttribute(titleAttr);
    if (!titleText.isEmpty())
        textOrder.append(AccessibilityText(titleText, AccessibilityTextSource::TitleTag));

    const AtomString& summary = areaElement.getAttribute(summaryAttr);
    if (!summary.isEmpty())
        textOrder.append(AccessibilityText(summary, AccessibilityTextSource::Summary));
}

RenderElement* AXImageMapHelpers::rendererFromAreaElement(const HTMLAreaElement& element)
{
    RefPtr map = ancestorsOfType<HTMLMapElement>(element).first();
    return map ? map->renderer() : nullptr;
}

} // namespace WebCore
