/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "StyleGeneratedImage.h"

namespace WebCore {

class CSSInvalidImageValue;

class StyleInvalidImage final : public StyleGeneratedImage {
public:
    static Ref<StyleInvalidImage> create();

    virtual ~StyleInvalidImage();

    bool operator==(const StyleImage&) const final { return false; }
    bool equals(const StyleInvalidImage&) const { return false; }
    bool canRender(const RenderElement*, float) const final { return false; }

    static constexpr bool isFixedSize = true;

protected:
    void didAddClient(RenderElement&) final { }
    void didRemoveClient(RenderElement&) final { }

    FloatSize fixedSize(const RenderElement&) const final { return { }; }

private:
    StyleInvalidImage();
    
    bool isPending() const final { return false; }
    void load(CachedResourceLoader&, const ResourceLoaderOptions&) final;
    bool knownToBeOpaque(const RenderElement&) const { return false; }

    RefPtr<Image> image(const RenderElement*, const FloatSize&, bool isForFirstLine) const final;
    Ref<CSSValue> computedStyleValue(const RenderStyle&) const;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleInvalidImage, isInvalidImage)
