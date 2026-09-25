/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#include <WebCore/FloatSize.h>
#include <WebCore/StyleImage.h>
#include <wtf/WeakHashCountedSet.h>

namespace WebCore {

class CSSValue;
class CachedImage;
class CachedResourceLoader;
class GeneratedImage;
class RenderElement;
struct ResourceLoaderOptions;

namespace Style {

class GeneratedImage : public Image {
public:
    const SingleThreadWeakHashCountedSet<RenderElement>& clients() const LIFETIME_BOUND { return m_clients; }

protected:
    explicit GeneratedImage(Image::Type);
    virtual ~GeneratedImage();

    WrappedImagePtr data() const final { return this; }

    NaturalDimensions naturalDimensions(const RenderElement&, const ImageSizingContext&) const override;

    ImageDrawResult drawTiled(GraphicsContext&, const RenderElement&, ConcreteObjectSize, const FloatRect& destination, const FloatPoint& phase, const FloatSize& tileSize, const FloatSize& spacing, ImagePaintingOptions, bool isForFirstLine) const override;

    void addClient(RenderElement&) final;
    void removeClient(RenderElement&) final;
    bool hasClient(RenderElement&) const final;

    // Allow subclasses to react to clients being added/removed.
    virtual void didAddClient(RenderElement&) = 0;
    virtual void didRemoveClient(RenderElement&) = 0;

    SingleThreadWeakHashCountedSet<RenderElement> m_clients;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(GeneratedImage, isGeneratedImage)
