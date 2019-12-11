/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "CSSValue.h"
#include "FloatSize.h"
#include "Image.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class CachedImage;
class CachedResourceLoader;
class CSSValue;
class RenderElement;
class RenderObject;
struct ResourceLoaderOptions;

typedef const void* WrappedImagePtr;

class StyleImage : public RefCounted<StyleImage> {
public:
    virtual ~StyleImage() = default;

    virtual bool operator==(const StyleImage& other) const = 0;

    virtual Ref<CSSValue> cssValue() const = 0;

    virtual bool canRender(const RenderElement*, float /*multiplier*/) const { return true; }
    virtual bool isPending() const = 0;
    virtual void load(CachedResourceLoader&, const ResourceLoaderOptions&) = 0;
    virtual bool isLoaded() const { return true; }
    virtual bool errorOccurred() const { return false; }
    virtual FloatSize imageSize(const RenderElement*, float multiplier) const = 0;
    virtual void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) = 0;
    virtual bool imageHasRelativeWidth() const = 0;
    virtual bool imageHasRelativeHeight() const = 0;
    virtual bool usesImageContainerSize() const = 0;
    virtual void setContainerContextForRenderer(const RenderElement&, const FloatSize&, float) = 0;
    virtual void addClient(RenderElement*) = 0;
    virtual void removeClient(RenderElement*) = 0;
    virtual RefPtr<Image> image(RenderElement*, const FloatSize&) const = 0;
    virtual WrappedImagePtr data() const = 0;
    virtual float imageScaleFactor() const { return 1; }
    virtual bool knownToBeOpaque(const RenderElement*) const = 0;
    virtual CachedImage* cachedImage() const { return 0; }

    ALWAYS_INLINE bool isCachedImage() const { return m_isCachedImage; }
    ALWAYS_INLINE bool isGeneratedImage() const { return m_isGeneratedImage; }

protected:
    StyleImage()
        : m_isCachedImage(false)
        , m_isGeneratedImage(false)
    {
    }
    bool m_isCachedImage : 1;
    bool m_isGeneratedImage : 1;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(ToClassName, predicate) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::StyleImage& image) { return image.predicate(); } \
SPECIALIZE_TYPE_TRAITS_END()
