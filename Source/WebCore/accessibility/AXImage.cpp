/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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
#include "AXImage.h"

#include "AXLogger.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentInlines.h"
#include "TextRecognitionOptions.h"

namespace WebCore {

AXImage::AXImage(RenderImage* renderer)
    : AccessibilityRenderObject(renderer)
{
}

Ref<AXImage> AXImage::create(RenderImage* renderer)
{
    return adoptRef(*new AXImage(renderer));
}

AccessibilityRole AXImage::determineAccessibilityRole()
{
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;
    return AccessibilityRole::Image;
}

std::optional<AXCoreObject::AccessibilityChildrenVector> AXImage::imageOverlayElements()
{
    AXTRACE("AXImage::imageOverlayElements"_s);

    auto& children = this->children();
    if (children.size())
        return children;

#if ENABLE(IMAGE_ANALYSIS)
    auto* page = this->page();
    if (!page)
        return std::nullopt;

    auto* element = this->element();
    if (!element)
        return std::nullopt;

    page->chrome().client().requestTextRecognition(*element, { }, [] (RefPtr<Element>&& imageOverlayHost) {
        if (!imageOverlayHost)
            return;

        if (auto* axObjectCache = imageOverlayHost->document().existingAXObjectCache())
            axObjectCache->postNotification(imageOverlayHost.get(), AXObjectCache::AXImageOverlayChanged);
    });
#endif

    return std::nullopt;
}

} // namespace WebCore
