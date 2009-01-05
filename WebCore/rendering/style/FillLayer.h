/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef FillLayer_h
#define FillLayer_h

#include "GraphicsTypes.h"
#include "Length.h"
#include "LengthSize.h"
#include "RenderStyleConstants.h"
#include "StyleImage.h"
#include <wtf/RefPtr.h>

namespace WebCore {

struct FillLayer {
public:
    FillLayer(EFillLayerType);
    ~FillLayer();

    StyleImage* image() const { return m_image.get(); }
    Length xPosition() const { return m_xPosition; }
    Length yPosition() const { return m_yPosition; }
    bool attachment() const { return m_attachment; }
    EFillBox clip() const { return static_cast<EFillBox>(m_clip); }
    EFillBox origin() const { return static_cast<EFillBox>(m_origin); }
    EFillRepeat repeat() const { return static_cast<EFillRepeat>(m_repeat); }
    CompositeOperator composite() const { return static_cast<CompositeOperator>(m_composite); }
    LengthSize size() const { return m_size; }

    const FillLayer* next() const { return m_next; }
    FillLayer* next() { return m_next; }

    bool isImageSet() const { return m_imageSet; }
    bool isXPositionSet() const { return m_xPosSet; }
    bool isYPositionSet() const { return m_yPosSet; }
    bool isAttachmentSet() const { return m_attachmentSet; }
    bool isClipSet() const { return m_clipSet; }
    bool isOriginSet() const { return m_originSet; }
    bool isRepeatSet() const { return m_repeatSet; }
    bool isCompositeSet() const { return m_compositeSet; }
    bool isSizeSet() const { return m_sizeSet; }
    
    void setImage(StyleImage* i) { m_image = i; m_imageSet = true; }
    void setXPosition(const Length& l) { m_xPosition = l; m_xPosSet = true; }
    void setYPosition(const Length& l) { m_yPosition = l; m_yPosSet = true; }
    void setAttachment(bool b) { m_attachment = b; m_attachmentSet = true; }
    void setClip(EFillBox b) { m_clip = b; m_clipSet = true; }
    void setOrigin(EFillBox b) { m_origin = b; m_originSet = true; }
    void setRepeat(EFillRepeat r) { m_repeat = r; m_repeatSet = true; }
    void setComposite(CompositeOperator c) { m_composite = c; m_compositeSet = true; }
    void setSize(const LengthSize& b) { m_size = b; m_sizeSet = true; }
    
    void clearImage() { m_imageSet = false; }
    void clearXPosition() { m_xPosSet = false; }
    void clearYPosition() { m_yPosSet = false; }
    void clearAttachment() { m_attachmentSet = false; }
    void clearClip() { m_clipSet = false; }
    void clearOrigin() { m_originSet = false; }
    void clearRepeat() { m_repeatSet = false; }
    void clearComposite() { m_compositeSet = false; }
    void clearSize() { m_sizeSet = false; }

    void setNext(FillLayer* n) { if (m_next != n) { delete m_next; m_next = n; } }

    FillLayer& operator=(const FillLayer& o);    
    FillLayer(const FillLayer& o);

    bool operator==(const FillLayer& o) const;
    bool operator!=(const FillLayer& o) const
    {
        return !(*this == o);
    }

    bool containsImage(StyleImage*) const;

    bool hasImage() const
    {
        if (m_image)
            return true;
        return m_next ? m_next->hasImage() : false;
    }

    bool hasFixedImage() const
    {
        if (m_image && !m_attachment)
            return true;
        return m_next ? m_next->hasFixedImage() : false;
    }

    EFillLayerType type() const { return static_cast<EFillLayerType>(m_type); }

    void fillUnsetProperties();
    void cullEmptyLayers();

    static bool initialFillAttachment(EFillLayerType) { return true; }
    static EFillBox initialFillClip(EFillLayerType) { return BorderFillBox; }
    static EFillBox initialFillOrigin(EFillLayerType type) { return type == BackgroundFillLayer ? PaddingFillBox : BorderFillBox; }
    static EFillRepeat initialFillRepeat(EFillLayerType) { return RepeatFill; }
    static CompositeOperator initialFillComposite(EFillLayerType) { return CompositeSourceOver; }
    static LengthSize initialFillSize(EFillLayerType) { return LengthSize(); }
    static Length initialFillXPosition(EFillLayerType) { return Length(0.0, Percent); }
    static Length initialFillYPosition(EFillLayerType) { return Length(0.0, Percent); }
    static StyleImage* initialFillImage(EFillLayerType) { return 0; }

private:
    FillLayer() { }

public:
    RefPtr<StyleImage> m_image;

    Length m_xPosition;
    Length m_yPosition;

    bool m_attachment : 1;
    unsigned m_clip : 2; // EFillBox
    unsigned m_origin : 2; // EFillBox
    unsigned m_repeat : 2; // EFillRepeat
    unsigned m_composite : 4; // CompositeOperator

    LengthSize m_size;

    bool m_imageSet : 1;
    bool m_attachmentSet : 1;
    bool m_clipSet : 1;
    bool m_originSet : 1;
    bool m_repeatSet : 1;
    bool m_xPosSet : 1;
    bool m_yPosSet : 1;
    bool m_compositeSet : 1;
    bool m_sizeSet : 1;
    
    unsigned m_type : 1; // EFillLayerType

    FillLayer* m_next;
};

} // namespace WebCore

#endif // FillLayer_h
