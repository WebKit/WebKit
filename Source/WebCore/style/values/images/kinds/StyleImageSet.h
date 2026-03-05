/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2020 Noam Rosenthal (noam@webkit.org)
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

#include "StyleMultiImage.h"
#include <wtf/TZoneMalloc.h>

namespace WebCore {
namespace Style {

class ImageSet final : public MultiImage {
    WTF_MAKE_TZONE_ALLOCATED(ImageSet);
public:
    static Ref<ImageSet> create(Vector<ImageWithScale>&&, Vector<size_t>&&);
    virtual ~ImageSet();

    bool operator==(const Image& other) const;
    bool equals(const ImageSet&) const;

    ImageWithScale selectBestFitImage(const Document&) final;

private:
    explicit ImageSet(Vector<ImageWithScale>&&, Vector<size_t>&&);

    Ref<CSSValue> computedStyleValue(const RenderStyle&) const final;

    ImageWithScale bestImageForScaleFactor();
    void updateDeviceScaleFactor(const Document&);

    bool m_accessedBestFitImage { false };
    ImageWithScale m_bestFitImage;
    float m_deviceScaleFactor { 1 };
    Vector<ImageWithScale> m_images;
    Vector<size_t> m_sortedIndices;
};

} // namespace Style
} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(ImageSet, isImageSet)
