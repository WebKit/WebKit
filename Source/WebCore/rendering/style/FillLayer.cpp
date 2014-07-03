/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FillLayer.h"

namespace WebCore {

struct SameSizeAsFillLayer {
    FillLayer* next;

    RefPtr<StyleImage> image;

    Length x;
    Length y;

    LengthSize sizeLength;

    unsigned bitfields : 32;
    unsigned bitfields2 : 11;
};

COMPILE_ASSERT(sizeof(FillLayer) == sizeof(SameSizeAsFillLayer), FillLayer_should_stay_small);

FillLayer::FillLayer(EFillLayerType type)
    : m_image(FillLayer::initialFillImage(type))
    , m_xPosition(FillLayer::initialFillXPosition(type))
    , m_yPosition(FillLayer::initialFillYPosition(type))
    , m_sizeLength(FillLayer::initialFillSizeLength(type))
    , m_attachment(FillLayer::initialFillAttachment(type))
    , m_clip(FillLayer::initialFillClip(type))
    , m_origin(FillLayer::initialFillOrigin(type))
    , m_repeatX(FillLayer::initialFillRepeatX(type))
    , m_repeatY(FillLayer::initialFillRepeatY(type))
    , m_composite(FillLayer::initialFillComposite(type))
    , m_sizeType(FillLayer::initialFillSizeType(type))
    , m_blendMode(FillLayer::initialFillBlendMode(type))
    , m_maskSourceType(FillLayer::initialMaskSourceType(type))
    , m_imageSet(false)
    , m_attachmentSet(false)
    , m_clipSet(false)
    , m_originSet(false)
    , m_repeatXSet(false)
    , m_repeatYSet(false)
    , m_xPosSet(false)
    , m_yPosSet(false)
    , m_backgroundOriginSet(false)
    , m_backgroundXOrigin(LeftEdge)
    , m_backgroundYOrigin(TopEdge)
    , m_compositeSet(type == MaskFillLayer)
    , m_blendModeSet(false)
    , m_maskSourceTypeSet(false)
    , m_type(type)
{
}

FillLayer::FillLayer(const FillLayer& o)
    : m_next(o.m_next ? std::make_unique<FillLayer>(*o.m_next) : nullptr)
    , m_image(o.m_image)
    , m_xPosition(o.m_xPosition)
    , m_yPosition(o.m_yPosition)
    , m_sizeLength(o.m_sizeLength)
    , m_attachment(o.m_attachment)
    , m_clip(o.m_clip)
    , m_origin(o.m_origin)
    , m_repeatX(o.m_repeatX)
    , m_repeatY(o.m_repeatY)
    , m_composite(o.m_composite)
    , m_sizeType(o.m_sizeType)
    , m_blendMode(o.m_blendMode)
    , m_maskSourceType(o.m_maskSourceType)
    , m_imageSet(o.m_imageSet)
    , m_attachmentSet(o.m_attachmentSet)
    , m_clipSet(o.m_clipSet)
    , m_originSet(o.m_originSet)
    , m_repeatXSet(o.m_repeatXSet)
    , m_repeatYSet(o.m_repeatYSet)
    , m_xPosSet(o.m_xPosSet)
    , m_yPosSet(o.m_yPosSet)
    , m_backgroundOriginSet(o.m_backgroundOriginSet)
    , m_backgroundXOrigin(o.m_backgroundXOrigin)
    , m_backgroundYOrigin(o.m_backgroundYOrigin)
    , m_compositeSet(o.m_compositeSet)
    , m_blendModeSet(o.m_blendModeSet)
    , m_maskSourceTypeSet(o.m_maskSourceTypeSet)
    , m_type(o.m_type)
{
}

FillLayer::~FillLayer()
{
    // Delete the layers in a loop rather than allowing recursive calls to the destructors.
    for (std::unique_ptr<FillLayer> next = WTF::move(m_next); next; next = WTF::move(next->m_next)) { }
}

FillLayer& FillLayer::operator=(const FillLayer& o)
{
    m_next = o.m_next ? std::make_unique<FillLayer>(*o.m_next) : nullptr;

    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_backgroundXOrigin = o.m_backgroundXOrigin;
    m_backgroundYOrigin = o.m_backgroundYOrigin;
    m_backgroundOriginSet = o.m_backgroundOriginSet;
    m_sizeLength = o.m_sizeLength;
    m_attachment = o.m_attachment;
    m_clip = o.m_clip;
    m_composite = o.m_composite;
    m_blendMode = o.m_blendMode;
    m_origin = o.m_origin;
    m_repeatX = o.m_repeatX;
    m_repeatY = o.m_repeatY;
    m_sizeType = o.m_sizeType;
    m_maskSourceType = o.m_maskSourceType;

    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_clipSet = o.m_clipSet;
    m_compositeSet = o.m_compositeSet;
    m_blendModeSet = o.m_blendModeSet;
    m_originSet = o.m_originSet;
    m_repeatXSet = o.m_repeatXSet;
    m_repeatYSet = o.m_repeatYSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
    m_maskSourceTypeSet = o.m_maskSourceTypeSet;

    m_type = o.m_type;

    return *this;
}

bool FillLayer::operator==(const FillLayer& o) const
{
    // We do not check the "isSet" booleans for each property, since those are only used during initial construction
    // to propagate patterns into layers. All layer comparisons happen after values have all been filled in anyway.
    return StyleImage::imagesEquivalent(m_image.get(), o.m_image.get()) && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition
        && m_backgroundXOrigin == o.m_backgroundXOrigin && m_backgroundYOrigin == o.m_backgroundYOrigin
        && m_attachment == o.m_attachment && m_clip == o.m_clip && m_composite == o.m_composite
        && m_blendMode == o.m_blendMode && m_origin == o.m_origin && m_repeatX == o.m_repeatX
        && m_repeatY == o.m_repeatY && m_sizeType == o.m_sizeType && m_maskSourceType == o.m_maskSourceType
        && m_sizeLength == o.m_sizeLength && m_type == o.m_type
        && ((m_next && o.m_next) ? *m_next == *o.m_next : m_next == o.m_next);
}

void FillLayer::fillUnsetProperties()
{
    FillLayer* curr;
    for (curr = this; curr && curr->isXPositionSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_xPosition = pattern->m_xPosition;
            if (pattern->isBackgroundOriginSet()) {
                curr->m_backgroundXOrigin = pattern->m_backgroundXOrigin;
                curr->m_backgroundYOrigin = pattern->m_backgroundYOrigin;
            }
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isYPositionSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_yPosition = pattern->m_yPosition;
            if (pattern->isBackgroundOriginSet()) {
                curr->m_backgroundXOrigin = pattern->m_backgroundXOrigin;
                curr->m_backgroundYOrigin = pattern->m_backgroundYOrigin;
            }
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isAttachmentSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_attachment = pattern->m_attachment;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isClipSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_clip = pattern->m_clip;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isCompositeSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_composite = pattern->m_composite;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBlendModeSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_blendMode = pattern->m_blendMode;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isOriginSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_origin = pattern->m_origin;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isRepeatXSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_repeatX = pattern->m_repeatX;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isRepeatYSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_repeatY = pattern->m_repeatY;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isSizeSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_sizeType = pattern->m_sizeType;
            curr->m_sizeLength = pattern->m_sizeLength;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
}

void FillLayer::cullEmptyLayers()
{
    for (FillLayer* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_next && !layer->m_next->isImageSet()) {
            layer->m_next = nullptr;
            break;
        }
    }
}

static inline EFillBox clipMax(EFillBox clipA, EFillBox clipB)
{
    if (clipA == BorderFillBox || clipB == BorderFillBox)
        return BorderFillBox;
    if (clipA == PaddingFillBox || clipB == PaddingFillBox)
        return PaddingFillBox;
    if (clipA == ContentFillBox || clipB == ContentFillBox)
        return ContentFillBox;
    return TextFillBox;
}

void FillLayer::computeClipMax() const
{
    Vector<const FillLayer*, 4> layers;
    for (auto* layer = this; layer; layer = layer->m_next.get())
        layers.append(layer);
    EFillBox computedClipMax = TextFillBox;
    for (unsigned i = layers.size(); i; --i) {
        auto& layer = *layers[i - 1];
        computedClipMax = clipMax(computedClipMax, layer.clip());
        layer.m_clipMax = computedClipMax;
    }
}

bool FillLayer::clipOccludesNextLayers(bool firstLayer) const
{
    if (firstLayer)
        computeClipMax();
    return m_clip == m_clipMax;
}

bool FillLayer::containsImage(StyleImage& image) const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_image && image == *layer->m_image)
            return true;
    }
    return false;
}

bool FillLayer::imagesAreLoaded() const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_image && !layer->m_image->isLoaded())
            return false;
    }
    return true;
}

bool FillLayer::hasOpaqueImage(const RenderElement& renderer) const
{
    if (!m_image)
        return false;

    if (m_composite == CompositeClear || m_composite == CompositeCopy)
        return true;

    return m_blendMode == BlendModeNormal && m_composite == CompositeSourceOver && m_image->knownToBeOpaque(&renderer);
}

bool FillLayer::hasRepeatXY() const
{
    return m_repeatX == RepeatFill && m_repeatY == RepeatFill;
}

bool FillLayer::hasImage() const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_image)
            return true;
    }
    return false;
}

bool FillLayer::hasFixedImage() const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_image && layer->m_attachment == FixedBackgroundAttachment)
            return true;
    }
    return false;
}

} // namespace WebCore
