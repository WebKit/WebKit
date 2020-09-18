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

#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

struct SameSizeAsFillLayer : RefCounted<SameSizeAsFillLayer> {
    FillLayer* next;

    RefPtr<StyleImage> image;

    Length x;
    Length y;

    LengthSize sizeLength;

    unsigned bitfields : 32;
    unsigned bitfields2 : 11;
};

COMPILE_ASSERT(sizeof(FillLayer) == sizeof(SameSizeAsFillLayer), FillLayer_should_stay_small);

Ref<FillLayer> FillLayer::create(FillLayerType type)
{
    return adoptRef(*new FillLayer(type));
}

Ref<FillLayer> FillLayer::create(const FillLayer& layer)
{
    return adoptRef(*new FillLayer(layer));
}

FillLayer::FillLayer(FillLayerType type)
    : m_image(FillLayer::initialFillImage(type))
    , m_xPosition(FillLayer::initialFillXPosition(type))
    , m_yPosition(FillLayer::initialFillYPosition(type))
    , m_attachment(static_cast<unsigned>(FillLayer::initialFillAttachment(type)))
    , m_clip(static_cast<unsigned>(FillLayer::initialFillClip(type)))
    , m_origin(static_cast<unsigned>(FillLayer::initialFillOrigin(type)))
    , m_repeatX(static_cast<unsigned>(FillLayer::initialFillRepeatX(type)))
    , m_repeatY(static_cast<unsigned>(FillLayer::initialFillRepeatY(type)))
    , m_composite(static_cast<unsigned>(FillLayer::initialFillComposite(type)))
    , m_sizeType(static_cast<unsigned>(FillSizeType::None))
    , m_blendMode(static_cast<unsigned>(FillLayer::initialFillBlendMode(type)))
    , m_maskSourceType(static_cast<unsigned>(FillLayer::initialFillMaskSourceType(type)))
    , m_imageSet(false)
    , m_attachmentSet(false)
    , m_clipSet(false)
    , m_originSet(false)
    , m_repeatXSet(false)
    , m_repeatYSet(false)
    , m_xPosSet(false)
    , m_yPosSet(false)
    , m_backgroundXOriginSet(false)
    , m_backgroundYOriginSet(false)
    , m_backgroundXOrigin(static_cast<unsigned>(Edge::Left))
    , m_backgroundYOrigin(static_cast<unsigned>(Edge::Top))
    , m_compositeSet(type == FillLayerType::Mask)
    , m_blendModeSet(false)
    , m_maskSourceTypeSet(false)
    , m_type(static_cast<unsigned>(type))
{
}

FillLayer::FillLayer(const FillLayer& o)
    : m_image(o.m_image)
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
    , m_backgroundXOriginSet(o.m_backgroundXOriginSet)
    , m_backgroundYOriginSet(o.m_backgroundYOriginSet)
    , m_backgroundXOrigin(o.m_backgroundXOrigin)
    , m_backgroundYOrigin(o.m_backgroundYOrigin)
    , m_compositeSet(o.m_compositeSet)
    , m_blendModeSet(o.m_blendModeSet)
    , m_maskSourceTypeSet(o.m_maskSourceTypeSet)
    , m_type(o.m_type)
{
    if (o.m_next)
        m_next = create(*o.m_next);
}

FillLayer::~FillLayer()
{
    // Delete the layers in a loop rather than allowing recursive calls to the destructors.
    for (auto next = WTFMove(m_next); next; next = WTFMove(next->m_next)) { }
}

FillLayer& FillLayer::operator=(const FillLayer& o)
{
    if (o.m_next)
        m_next = create(*o.m_next);
    else
        m_next = nullptr;
    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_backgroundXOrigin = o.m_backgroundXOrigin;
    m_backgroundYOrigin = o.m_backgroundYOrigin;
    m_backgroundXOriginSet = o.m_backgroundXOriginSet;
    m_backgroundYOriginSet = o.m_backgroundYOriginSet;
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
    return arePointingToEqualData(m_image.get(), o.m_image.get()) && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition
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
            if (pattern->isBackgroundXOriginSet())
                curr->m_backgroundXOrigin = pattern->m_backgroundXOrigin;
            if (pattern->isBackgroundYOriginSet())
                curr->m_backgroundYOrigin = pattern->m_backgroundYOrigin;
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
            if (pattern->isBackgroundXOriginSet())
                curr->m_backgroundXOrigin = pattern->m_backgroundXOrigin;
            if (pattern->isBackgroundYOriginSet())
                curr->m_backgroundYOrigin = pattern->m_backgroundYOrigin;
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

static inline FillBox clipMax(FillBox clipA, FillBox clipB)
{
    if (clipA == FillBox::Border || clipB == FillBox::Border)
        return FillBox::Border;
    if (clipA == FillBox::Padding || clipB == FillBox::Padding)
        return FillBox::Padding;
    if (clipA == FillBox::Content || clipB == FillBox::Content)
        return FillBox::Content;
    return FillBox::Text;
}

void FillLayer::computeClipMax() const
{
    Vector<const FillLayer*, 4> layers;
    for (auto* layer = this; layer; layer = layer->m_next.get())
        layers.append(layer);
    FillBox computedClipMax = FillBox::Text;
    for (unsigned i = layers.size(); i; --i) {
        auto& layer = *layers[i - 1];
        computedClipMax = clipMax(computedClipMax, layer.clip());
        layer.m_clipMax = static_cast<unsigned>(computedClipMax);
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

    if (static_cast<CompositeOperator>(m_composite) == CompositeOperator::Clear || static_cast<CompositeOperator>(m_composite) == CompositeOperator::Copy)
        return true;

    return static_cast<BlendMode>(m_blendMode) == BlendMode::Normal && static_cast<CompositeOperator>(m_composite) == CompositeOperator::SourceOver && m_image->knownToBeOpaque(&renderer);
}

bool FillLayer::hasRepeatXY() const
{
    return repeatX() == FillRepeat::Repeat && repeatY() == FillRepeat::Repeat;
}

bool FillLayer::hasImageInAnyLayer() const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->image())
            return true;
    }
    return false;
}

bool FillLayer::hasFixedImage() const
{
    for (auto* layer = this; layer; layer = layer->m_next.get()) {
        if (layer->m_image && layer->attachment() == FillAttachment::FixedBackground)
            return true;
    }
    return false;
}

bool FillLayer::imagesIdentical(const FillLayer* layer1, const FillLayer* layer2)
{
    for (; layer1 && layer2; layer1 = layer1->next(), layer2 = layer2->next()) {
        if (!arePointingToEqualData(layer1->image(), layer2->image()))
            return false;
    }

    return !layer1 && !layer2;
}

TextStream& operator<<(TextStream& ts, FillSize fillSize)
{
    return ts << fillSize.type << " " << fillSize.size;
}

TextStream& operator<<(TextStream& ts, const FillLayer& layer)
{
    TextStream::GroupScope scope(ts);
    ts << "fill-layer";

    ts.startGroup();
    ts << "position " << layer.xPosition() << " " << layer.yPosition();
    ts.endGroup();

    ts.dumpProperty("size", layer.size());

    ts.startGroup();
    ts << "background-origin " << layer.backgroundXOrigin() << " " << layer.backgroundYOrigin();
    ts.endGroup();

    ts.startGroup();
    ts << "repeat " << layer.repeatX() << " " << layer.repeatY();
    ts.endGroup();

    ts.dumpProperty("clip", layer.clip());
    ts.dumpProperty("origin", layer.origin());

    ts.dumpProperty("composite", layer.composite());
    ts.dumpProperty("blend-mode", layer.blendMode());
    ts.dumpProperty("mask-type", layer.maskSourceType());

    if (layer.next())
        ts << *layer.next();

    return ts;
}

} // namespace WebCore
