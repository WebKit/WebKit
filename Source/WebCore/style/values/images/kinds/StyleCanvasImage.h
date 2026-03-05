/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CanvasObserver.h"
#include "HTMLCanvasElement.h"
#include "StyleGeneratedImage.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;

namespace Style {

class CanvasImage final : public GeneratedImage, public CanvasObserver, public CanMakeCheckedPtr<CanvasImage> {
    WTF_MAKE_TZONE_ALLOCATED(CanvasImage);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(CanvasImage);
public:
    static Ref<CanvasImage> create(String name)
    {
        return adoptRef(*new CanvasImage(WTF::move(name)));
    }
    virtual ~CanvasImage();

    bool operator==(const Image&) const final;
    bool equals(const CanvasImage&) const;

    static constexpr bool isFixedSize = true;

    OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(CanMakeCheckedPtr);

private:
    explicit CanvasImage(String&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;
    bool isPending() const final;
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    RefPtr<WebCore::Image> image(const RenderElement*, const FloatSize&, const GraphicsContext& destinationContext, bool isForFirstLine) const final;
    bool knownToBeOpaque(const RenderElement&) const final;
    FloatSize fixedSize(const RenderElement&) const final;
    void didAddClient(RenderElement&) final;
    void didRemoveClient(RenderElement&) final;

    // CanvasObserver.
    bool isStyleCanvasImage() const final { return true; }
    void canvasChanged(CanvasBase&, const FloatRect&) final;
    void canvasResized(CanvasBase&) final;
    void canvasDestroyed(CanvasBase&) final;

    HTMLCanvasElement* element(Document&) const;

    // The name of the canvas.
    String m_name;
    // The document supplies the element and owns it.
    mutable WeakPtr<HTMLCanvasElement, WeakPtrImplWithEventTargetData> m_element;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Style::CanvasImage)
    static bool isType(const WebCore::Style::Image& image) { return image.isCanvasImage(); }
    static bool isType(const WebCore::CanvasObserver& canvasObserver) { return canvasObserver.isStyleCanvasImage(); }
SPECIALIZE_TYPE_TRAITS_END()

