/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#include "GraphicsTypes.h"
#include "LengthSize.h"
#include "RenderStyleConstants.h"
#include "StyleImage.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class RenderElement;

struct FillSize {
    FillSize()
        : type(FillSizeType::Size)
    {
    }

    FillSize(FillSizeType type, const LengthSize& size)
        : type(type)
        , size(size)
    {
    }

    FillSizeType type;
    LengthSize size;
};

inline bool operator==(const FillSize& a, const FillSize& b)
{
    return a.type == b.type && a.size == b.size;
}

inline bool operator!=(const FillSize& a, const FillSize& b)
{
    return !(a == b);
}

class FillLayer {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FillLayer(FillLayerType);
    ~FillLayer();

    StyleImage* image() const { return m_image.get(); }
    const Length& xPosition() const { return m_xPosition; }
    const Length& yPosition() const { return m_yPosition; }
    Edge backgroundXOrigin() const { return static_cast<Edge>(m_backgroundXOrigin); }
    Edge backgroundYOrigin() const { return static_cast<Edge>(m_backgroundYOrigin); }
    FillAttachment attachment() const { return static_cast<FillAttachment>(m_attachment); }
    FillBox clip() const { return static_cast<FillBox>(m_clip); }
    FillBox origin() const { return static_cast<FillBox>(m_origin); }
    FillRepeat repeatX() const { return static_cast<FillRepeat>(m_repeatX); }
    FillRepeat repeatY() const { return static_cast<FillRepeat>(m_repeatY); }
    CompositeOperator composite() const { return static_cast<CompositeOperator>(m_composite); }
    BlendMode blendMode() const { return static_cast<BlendMode>(m_blendMode); }
    const LengthSize& sizeLength() const { return m_sizeLength; }
    FillSizeType sizeType() const { return static_cast<FillSizeType>(m_sizeType); }
    FillSize size() const { return FillSize(static_cast<FillSizeType>(m_sizeType), m_sizeLength); }
    MaskSourceType maskSourceType() const { return static_cast<MaskSourceType>(m_maskSourceType); }

    const FillLayer* next() const { return m_next.get(); }
    FillLayer* next() { return m_next.get(); }

    bool isImageSet() const { return m_imageSet; }
    bool isXPositionSet() const { return m_xPosSet; }
    bool isYPositionSet() const { return m_yPosSet; }
    bool isBackgroundXOriginSet() const { return m_backgroundXOriginSet; }
    bool isBackgroundYOriginSet() const { return m_backgroundYOriginSet; }
    bool isAttachmentSet() const { return m_attachmentSet; }
    bool isClipSet() const { return m_clipSet; }
    bool isOriginSet() const { return m_originSet; }
    bool isRepeatXSet() const { return m_repeatXSet; }
    bool isRepeatYSet() const { return m_repeatYSet; }
    bool isCompositeSet() const { return m_compositeSet; }
    bool isBlendModeSet() const { return m_blendModeSet; }
    bool isSizeSet() const { return static_cast<FillSizeType>(m_sizeType) != FillSizeType::None; }
    bool isMaskSourceTypeSet() const { return m_maskSourceTypeSet; }

    bool isEmpty() const { return (sizeType() == FillSizeType::Size && m_sizeLength.isEmpty()) || sizeType() == FillSizeType::None; }

    void setImage(RefPtr<StyleImage>&& image) { m_image = WTFMove(image); m_imageSet = true; }
    void setXPosition(Length length) { m_xPosition = WTFMove(length); m_xPosSet = true; }
    void setYPosition(Length length) { m_yPosition = WTFMove(length); m_yPosSet = true; }
    void setBackgroundXOrigin(Edge o) { m_backgroundXOrigin = static_cast<unsigned>(o); m_backgroundXOriginSet = true; }
    void setBackgroundYOrigin(Edge o) { m_backgroundYOrigin = static_cast<unsigned>(o); m_backgroundYOriginSet = true; }
    void setAttachment(FillAttachment attachment) { m_attachment = static_cast<unsigned>(attachment); m_attachmentSet = true; }
    void setClip(FillBox b) { m_clip = static_cast<unsigned>(b); m_clipSet = true; }
    void setOrigin(FillBox b) { m_origin = static_cast<unsigned>(b); m_originSet = true; }
    void setRepeatX(FillRepeat r) { m_repeatX = static_cast<unsigned>(r); m_repeatXSet = true; }
    void setRepeatY(FillRepeat r) { m_repeatY = static_cast<unsigned>(r); m_repeatYSet = true; }
    void setComposite(CompositeOperator c) { m_composite = static_cast<unsigned>(c); m_compositeSet = true; }
    void setBlendMode(BlendMode b) { m_blendMode = static_cast<unsigned>(b); m_blendModeSet = true; }
    void setSizeType(FillSizeType b) { m_sizeType = static_cast<unsigned>(b); }
    void setSizeLength(LengthSize l) { m_sizeLength = l; }
    void setSize(FillSize f) { m_sizeType = static_cast<unsigned>(f.type); m_sizeLength = f.size; }
    void setMaskSourceType(MaskSourceType m) { m_maskSourceType = static_cast<unsigned>(m); m_maskSourceTypeSet = true; }

    void clearImage() { m_image = nullptr; m_imageSet = false; }

    void clearXPosition() { m_xPosSet = false; m_backgroundXOriginSet = false; }
    void clearYPosition() { m_yPosSet = false; m_backgroundYOriginSet = false; }

    void clearAttachment() { m_attachmentSet = false; }
    void clearClip() { m_clipSet = false; }
    void clearOrigin() { m_originSet = false; }
    void clearRepeatX() { m_repeatXSet = false; }
    void clearRepeatY() { m_repeatYSet = false; }
    void clearComposite() { m_compositeSet = false; }
    void clearBlendMode() { m_blendModeSet = false; }
    void clearSize() { m_sizeType = static_cast<unsigned>(FillSizeType::None); }
    void clearMaskSourceType() { m_maskSourceTypeSet = false; }

    void setNext(std::unique_ptr<FillLayer> next) { m_next = WTFMove(next); }

    FillLayer& operator=(const FillLayer&);
    FillLayer(const FillLayer&);

    bool operator==(const FillLayer&) const;
    bool operator!=(const FillLayer& other) const { return !(*this == other); }

    bool containsImage(StyleImage&) const;
    bool imagesAreLoaded() const;
    bool hasImage() const;
    bool hasFixedImage() const;
    bool hasOpaqueImage(const RenderElement&) const;
    bool hasRepeatXY() const;
    bool clipOccludesNextLayers(bool firstLayer) const;

    FillLayerType type() const { return static_cast<FillLayerType>(m_type); }

    void fillUnsetProperties();
    void cullEmptyLayers();

    static bool imagesIdentical(const FillLayer*, const FillLayer*);

    static FillAttachment initialFillAttachment(FillLayerType) { return FillAttachment::ScrollBackground; }
    static FillBox initialFillClip(FillLayerType) { return FillBox::Border; }
    static FillBox initialFillOrigin(FillLayerType type) { return type == FillLayerType::Background ? FillBox::Padding : FillBox::Border; }
    static FillRepeat initialFillRepeatX(FillLayerType) { return FillRepeat::Repeat; }
    static FillRepeat initialFillRepeatY(FillLayerType) { return FillRepeat::Repeat; }
    static CompositeOperator initialFillComposite(FillLayerType) { return CompositeSourceOver; }
    static BlendMode initialFillBlendMode(FillLayerType) { return BlendMode::Normal; }
    static FillSize initialFillSize(FillLayerType) { return { }; }
    static Length initialFillXPosition(FillLayerType) { return Length(0.0f, Percent); }
    static Length initialFillYPosition(FillLayerType) { return Length(0.0f, Percent); }
    static StyleImage* initialFillImage(FillLayerType) { return nullptr; }
    static MaskSourceType initialFillMaskSourceType(FillLayerType) { return MaskSourceType::Alpha; }

private:
    friend class RenderStyle;

    void computeClipMax() const;

    std::unique_ptr<FillLayer> m_next;

    RefPtr<StyleImage> m_image;

    Length m_xPosition;
    Length m_yPosition;

    LengthSize m_sizeLength;

    unsigned m_attachment : 2; // FillAttachment
    unsigned m_clip : 2; // FillBox
    unsigned m_origin : 2; // FillBox
    unsigned m_repeatX : 3; // FillRepeat
    unsigned m_repeatY : 3; // FillRepeat
    unsigned m_composite : 4; // CompositeOperator
    unsigned m_sizeType : 2; // FillSizeType
    unsigned m_blendMode : 5; // BlendMode
    unsigned m_maskSourceType : 1; // MaskSourceType

    unsigned m_imageSet : 1;
    unsigned m_attachmentSet : 1;
    unsigned m_clipSet : 1;
    unsigned m_originSet : 1;
    unsigned m_repeatXSet : 1;
    unsigned m_repeatYSet : 1;
    unsigned m_xPosSet : 1;
    unsigned m_yPosSet : 1;
    unsigned m_backgroundXOriginSet : 1;
    unsigned m_backgroundYOriginSet : 1;
    unsigned m_backgroundXOrigin : 2; // Edge
    unsigned m_backgroundYOrigin : 2; // Edge
    unsigned m_compositeSet : 1;
    unsigned m_blendModeSet : 1;
    unsigned m_maskSourceTypeSet : 1;

    unsigned m_type : 1; // FillLayerType

    mutable unsigned m_clipMax : 2; // FillBox, maximum m_clip value from this to bottom layer
};

WTF::TextStream& operator<<(WTF::TextStream&, FillSize);
WTF::TextStream& operator<<(WTF::TextStream&, const FillLayer&);

} // namespace WebCore
