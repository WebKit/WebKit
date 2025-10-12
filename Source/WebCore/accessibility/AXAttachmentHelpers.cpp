/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(ATTACHMENT_ELEMENT)
#include "AXAttachmentHelpers.h"

#include "AXCoreObject.h"
#include "HTMLAttachmentElement.h"

namespace WebCore {

using namespace HTMLNames;

bool AXAttachmentHelpers::hasProgress(const HTMLAttachmentElement& attachmentElement, float* progress)
{
    auto& progressString = attachmentElement.getAttribute(progressAttr);
    bool validProgress = false;
    float result = std::max<float>(std::min<float>(progressString.toFloat(&validProgress), 1), 0);
    if (progress)
        *progress = result;
    return validProgress;
}

void AXAttachmentHelpers::accessibilityText(const HTMLAttachmentElement& attachmentElement, Vector<AccessibilityText>& textOrder)
{
    auto title = attachmentElement.attachmentTitle();
    auto& subtitle = attachmentElement.attachmentSubtitle();
    auto& action = attachmentElement.getAttribute(actionAttr);

    if (action.length())
        textOrder.append(AccessibilityText(WTFMove(action), AccessibilityTextSource::Action));

    if (title.length())
        textOrder.append(AccessibilityText(WTFMove(title), AccessibilityTextSource::Title));

    if (subtitle.length())
        textOrder.append(AccessibilityText(WTFMove(subtitle), AccessibilityTextSource::Subtitle));
}

} // namespace WebCore

#endif // ENABLE(ATTACHMENT_ELEMENT)
