/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "RenderStyle.h"

#include "CachedImage.h"
#include "CSSStyleSelector.h"
#include "RenderArena.h"
#include "RenderObject.h"

namespace WebCore {

static RenderStyle* defaultStyle;

static bool imagesEquivalent(StyleImage* image1, StyleImage* image2)
{
    if (image1 != image2) {
        if (!image1 || !image2)
            return false;
        return *image1 == *image2;
    }
    return true;
}

bool NinePieceImage::operator==(const NinePieceImage& o) const
{
    return imagesEquivalent(m_image.get(), o.m_image.get()) && m_slices == o.m_slices && m_horizontalRule == o.m_horizontalRule &&
           m_verticalRule == o.m_verticalRule;
}

StyleSurroundData::StyleSurroundData()
    : margin(Fixed)
    , padding(Fixed)
{
}

StyleSurroundData::StyleSurroundData(const StyleSurroundData& o)
    : RefCounted<StyleSurroundData>()
    , offset(o.offset)
    , margin(o.margin)
    , padding(o.padding)
    , border(o.border)
{
}

bool StyleSurroundData::operator==(const StyleSurroundData& o) const
{
    return offset == o.offset && margin == o.margin && padding == o.padding && border == o.border;
}

StyleBoxData::StyleBoxData()
    : z_index(0)
    , z_auto(true)
    , boxSizing(CONTENT_BOX)
{
    // Initialize our min/max widths/heights.
    min_width = min_height = RenderStyle::initialMinSize();
    max_width = max_height = RenderStyle::initialMaxSize();
}

StyleBoxData::StyleBoxData(const StyleBoxData& o)
    : RefCounted<StyleBoxData>()
    , width(o.width)
    , height(o.height)
    , min_width(o.min_width)
    , max_width(o.max_width)
    , min_height(o.min_height)
    , max_height(o.max_height)
    , z_index(o.z_index)
    , z_auto(o.z_auto)
    , boxSizing(o.boxSizing)
{
}

bool StyleBoxData::operator==(const StyleBoxData& o) const
{
    return width == o.width &&
           height == o.height &&
           min_width == o.min_width &&
           max_width == o.max_width &&
           min_height == o.min_height &&
           max_height == o.max_height &&
           z_index == o.z_index &&
           z_auto == o.z_auto &&
           boxSizing == o.boxSizing;
}

StyleVisualData::StyleVisualData()
    : hasClip(false)
    , textDecoration(RenderStyle::initialTextDecoration())
    , counterIncrement(0)
    , counterReset(0)
    , m_zoom(RenderStyle::initialZoom())
{
}

StyleVisualData::~StyleVisualData()
{
}

StyleVisualData::StyleVisualData(const StyleVisualData& o)
    : RefCounted<StyleVisualData>()
    , clip(o.clip)
    , hasClip(o.hasClip)
    , textDecoration(o.textDecoration)
    , counterIncrement(o.counterIncrement)
    , counterReset(o.counterReset)
    , m_zoom(RenderStyle::initialZoom())
{
}

PassRefPtr<CSSValue> StyleCachedImage::cssValue()
{
    return CSSPrimitiveValue::create(m_image->url(), CSSPrimitiveValue::CSS_URI);
}

bool StyleCachedImage::canRender(float multiplier) const
{
    return m_image->canRender(multiplier);
}

bool StyleCachedImage::isLoaded() const
{
    return m_image->isLoaded();
}

bool StyleCachedImage::errorOccurred() const
{
    return m_image->errorOccurred();
}

IntSize StyleCachedImage::imageSize(const RenderObject* /*renderer*/, float multiplier) const
{
    return m_image->imageSize(multiplier);
}

bool StyleCachedImage::imageHasRelativeWidth() const
{
    return m_image->imageHasRelativeWidth();
}

bool StyleCachedImage::imageHasRelativeHeight() const
{
    return m_image->imageHasRelativeHeight();
}

bool StyleCachedImage::usesImageContainerSize() const
{
    return m_image->usesImageContainerSize();
}

void StyleCachedImage::setImageContainerSize(const IntSize& size)
{
    return m_image->setImageContainerSize(size);
}

void StyleCachedImage::addClient(RenderObject* renderer)
{
    return m_image->addClient(renderer);
}

void StyleCachedImage::removeClient(RenderObject* renderer)
{
    return m_image->removeClient(renderer);
}

Image* StyleCachedImage::image(RenderObject* renderer, const IntSize&) const
{
    return m_image->image();
}

PassRefPtr<CSSValue> StyleGeneratedImage::cssValue()
{
    return m_generator;
}

IntSize StyleGeneratedImage::imageSize(const RenderObject* renderer, float /* multiplier */) const
{
    // We can ignore the multiplier, since we always store a raw zoomed size.
    if (m_fixedSize)
        return m_generator->fixedSize(renderer);
    return m_containerSize;
}

void StyleGeneratedImage::setImageContainerSize(const IntSize& size)
{
    m_containerSize = size;
}

void StyleGeneratedImage::addClient(RenderObject* renderer)
{
    m_generator->addClient(renderer, IntSize());
}

void StyleGeneratedImage::removeClient(RenderObject* renderer)
{
    m_generator->removeClient(renderer);
}

Image* StyleGeneratedImage::image(RenderObject* renderer, const IntSize& size) const
{
    return m_generator->image(renderer, size);
}

FillLayer::FillLayer(EFillLayerType type)
    : m_image(FillLayer::initialFillImage(type))
    , m_xPosition(FillLayer::initialFillXPosition(type))
    , m_yPosition(FillLayer::initialFillYPosition(type))
    , m_attachment(FillLayer::initialFillAttachment(type))
    , m_clip(FillLayer::initialFillClip(type))
    , m_origin(FillLayer::initialFillOrigin(type))
    , m_repeat(FillLayer::initialFillRepeat(type))
    , m_composite(FillLayer::initialFillComposite(type))
    , m_size(FillLayer::initialFillSize(type))
    , m_imageSet(false)
    , m_attachmentSet(false)
    , m_clipSet(false)
    , m_originSet(false)
    , m_repeatSet(false)
    , m_xPosSet(false)
    , m_yPosSet(false)
    , m_compositeSet(type == MaskFillLayer)
    , m_sizeSet(false)
    , m_type(type)
    , m_next(0)
{
}

FillLayer::FillLayer(const FillLayer& o)
    : m_image(o.m_image)
    , m_xPosition(o.m_xPosition)
    , m_yPosition(o.m_yPosition)
    , m_attachment(o.m_attachment)
    , m_clip(o.m_clip)
    , m_origin(o.m_origin)
    , m_repeat(o.m_repeat)
    , m_composite(o.m_composite)
    , m_size(o.m_size)
    , m_imageSet(o.m_imageSet)
    , m_attachmentSet(o.m_attachmentSet)
    , m_clipSet(o.m_clipSet)
    , m_originSet(o.m_originSet)
    , m_repeatSet(o.m_repeatSet)
    , m_xPosSet(o.m_xPosSet)
    , m_yPosSet(o.m_yPosSet)
    , m_compositeSet(o.m_compositeSet)
    , m_sizeSet(o.m_sizeSet)
    , m_type(o.m_type)
    , m_next(o.m_next ? new FillLayer(*o.m_next) : 0)
{
}

FillLayer::~FillLayer()
{
    delete m_next;
}

FillLayer& FillLayer::operator=(const FillLayer& o)
{
    if (m_next != o.m_next) {
        delete m_next;
        m_next = o.m_next ? new FillLayer(*o.m_next) : 0;
    }

    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_attachment = o.m_attachment;
    m_clip = o.m_clip;
    m_composite = o.m_composite;
    m_origin = o.m_origin;
    m_repeat = o.m_repeat;
    m_size = o.m_size;

    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_clipSet = o.m_clipSet;
    m_compositeSet = o.m_compositeSet;
    m_originSet = o.m_originSet;
    m_repeatSet = o.m_repeatSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
    m_sizeSet = o.m_sizeSet;
    
    m_type = o.m_type;

    return *this;
}

bool FillLayer::operator==(const FillLayer& o) const
{
    // We do not check the "isSet" booleans for each property, since those are only used during initial construction
    // to propagate patterns into layers.  All layer comparisons happen after values have all been filled in anyway.
    return imagesEquivalent(m_image.get(), o.m_image.get()) && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition &&
           m_attachment == o.m_attachment && m_clip == o.m_clip && 
           m_composite == o.m_composite && m_origin == o.m_origin && m_repeat == o.m_repeat &&
           m_size.width == o.m_size.width && m_size.height == o.m_size.height && m_type == o.m_type &&
           ((m_next && o.m_next) ? *m_next == *o.m_next : m_next == o.m_next);
}

void FillLayer::fillUnsetProperties()
{
    FillLayer* curr;
    for (curr = this; curr && curr->isImageSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_image = pattern->m_image;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isXPositionSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_xPosition = pattern->m_xPosition;
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

    for (curr = this; curr && curr->isRepeatSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_repeat = pattern->m_repeat;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isSizeSet(); curr = curr->next()) { }
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (FillLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_size = pattern->m_size;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
}

void FillLayer::cullEmptyLayers()
{
    FillLayer* next;
    for (FillLayer* p = this; p; p = next) {
        next = p->m_next;
        if (next && !next->isImageSet() &&
            !next->isXPositionSet() && !next->isYPositionSet() &&
            !next->isAttachmentSet() && !next->isClipSet() &&
            !next->isCompositeSet() && !next->isOriginSet() &&
            !next->isRepeatSet() && !next->isSizeSet()) {
            delete next;
            p->m_next = 0;
            break;
        }
    }
}

StyleBackgroundData::StyleBackgroundData()
: m_background(BackgroundFillLayer)
{
}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& o)
    : RefCounted<StyleBackgroundData>()
    , m_background(o.m_background)
    , m_color(o.m_color)
    , m_outline(o.m_outline)
{
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& o) const
{
    return m_background == o.m_background && m_color == o.m_color && m_outline == o.m_outline;
}

StyleMarqueeData::StyleMarqueeData()
    : increment(RenderStyle::initialMarqueeIncrement())
    , speed(RenderStyle::initialMarqueeSpeed())
    , loops(RenderStyle::initialMarqueeLoopCount())
    , behavior(RenderStyle::initialMarqueeBehavior())
    , direction(RenderStyle::initialMarqueeDirection())
{
}

StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData& o)
    : RefCounted<StyleMarqueeData>()
    , increment(o.increment)
    , speed(o.speed)
    , loops(o.loops)
    , behavior(o.behavior)
    , direction(o.direction) 
{
}

bool StyleMarqueeData::operator==(const StyleMarqueeData& o) const
{
    return increment == o.increment && speed == o.speed && direction == o.direction &&
           behavior == o.behavior && loops == o.loops;
}

StyleFlexibleBoxData::StyleFlexibleBoxData()
    : flex(RenderStyle::initialBoxFlex())
    , flex_group(RenderStyle::initialBoxFlexGroup())
    , ordinal_group(RenderStyle::initialBoxOrdinalGroup())
    , align(RenderStyle::initialBoxAlign())
    , pack(RenderStyle::initialBoxPack())
    , orient(RenderStyle::initialBoxOrient())
    , lines(RenderStyle::initialBoxLines())
{
}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
    : RefCounted<StyleFlexibleBoxData>()
    , flex(o.flex)
    , flex_group(o.flex_group)
    , ordinal_group(o.ordinal_group)
    , align(o.align)
    , pack(o.pack)
    , orient(o.orient)
    , lines(o.lines)
{
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return flex == o.flex && flex_group == o.flex_group &&
           ordinal_group == o.ordinal_group && align == o.align &&
           pack == o.pack && orient == o.orient && lines == o.lines;
}

StyleMultiColData::StyleMultiColData()
    : m_width(0)
    , m_count(RenderStyle::initialColumnCount())
    , m_gap(0)
    , m_autoWidth(true)
    , m_autoCount(true)
    , m_normalGap(true)
    , m_breakBefore(RenderStyle::initialPageBreak())
    , m_breakAfter(RenderStyle::initialPageBreak())
    , m_breakInside(RenderStyle::initialPageBreak())
{
}

StyleMultiColData::StyleMultiColData(const StyleMultiColData& o)
    : RefCounted<StyleMultiColData>()
    , m_width(o.m_width)
    , m_count(o.m_count)
    , m_gap(o.m_gap)
    , m_rule(o.m_rule)
    , m_autoWidth(o.m_autoWidth)
    , m_autoCount(o.m_autoCount)
    , m_normalGap(o.m_normalGap)
    , m_breakBefore(o.m_breakBefore)
    , m_breakAfter(o.m_breakAfter)
    , m_breakInside(o.m_breakInside)
{
}

bool StyleMultiColData::operator==(const StyleMultiColData& o) const
{
    return m_width == o.m_width && m_count == o.m_count && m_gap == o.m_gap &&
           m_rule == o.m_rule && m_breakBefore == o.m_breakBefore && 
           m_autoWidth == o.m_autoWidth && m_autoCount == o.m_autoCount && m_normalGap == o.m_normalGap &&
           m_breakAfter == o.m_breakAfter && m_breakInside == o.m_breakInside;
}

StyleTransformData::StyleTransformData()
    : m_operations(RenderStyle::initialTransform())
    , m_x(RenderStyle::initialTransformOriginX())
    , m_y(RenderStyle::initialTransformOriginY())
{
}

StyleTransformData::StyleTransformData(const StyleTransformData& o)
    : RefCounted<StyleTransformData>()
    , m_operations(o.m_operations)
    , m_x(o.m_x)
    , m_y(o.m_y)
{
}

bool StyleTransformData::operator==(const StyleTransformData& o) const
{
    return m_x == o.m_x && m_y == o.m_y && m_operations == o.m_operations;
}

bool TransformOperations::operator==(const TransformOperations& o) const
{
    if (m_operations.size() != o.m_operations.size())
        return false;
        
    unsigned s = m_operations.size();
    for (unsigned i = 0; i < s; i++) {
        if (*m_operations[i] != *o.m_operations[i])
            return false;
    }
    
    return true;
}

PassRefPtr<TransformOperation> ScaleTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isScaleOperation())
        return this;
    
    if (blendToIdentity)
        return ScaleTransformOperation::create(m_x + (1. - m_x) * progress, m_y + (1. - m_y) * progress);
    
    const ScaleTransformOperation* fromOp = static_cast<const ScaleTransformOperation*>(from);
    double fromX = fromOp ? fromOp->m_x : 1.;
    double fromY = fromOp ? fromOp->m_y : 1.;
    return ScaleTransformOperation::create(fromX + (m_x - fromX) * progress, fromY + (m_y - fromY) * progress);
}

PassRefPtr<TransformOperation> RotateTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isRotateOperation())
        return this;
    
    if (blendToIdentity)
        return RotateTransformOperation::create(m_angle - m_angle * progress);
    
    const RotateTransformOperation* fromOp = static_cast<const RotateTransformOperation*>(from);
    double fromAngle = fromOp ? fromOp->m_angle : 0;
    return RotateTransformOperation::create(fromAngle + (m_angle - fromAngle) * progress);
}

PassRefPtr<TransformOperation> SkewTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isSkewOperation())
        return this;
    
    if (blendToIdentity)
        return SkewTransformOperation::create(m_angleX - m_angleX * progress, m_angleY - m_angleY * progress);
    
    const SkewTransformOperation* fromOp = static_cast<const SkewTransformOperation*>(from);
    double fromAngleX = fromOp ? fromOp->m_angleX : 0;
    double fromAngleY = fromOp ? fromOp->m_angleY : 0;
    return SkewTransformOperation::create(fromAngleX + (m_angleX - fromAngleX) * progress, fromAngleY + (m_angleY - fromAngleY) * progress);
}

PassRefPtr<TransformOperation> TranslateTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isTranslateOperation())
        return this;
    
    if (blendToIdentity)
        return TranslateTransformOperation::create(Length(m_x.type()).blend(m_x, progress), Length(m_y.type()).blend(m_y, progress));

    const TranslateTransformOperation* fromOp = static_cast<const TranslateTransformOperation*>(from);
    Length fromX = fromOp ? fromOp->m_x : Length(m_x.type());
    Length fromY = fromOp ? fromOp->m_y : Length(m_y.type());
    return TranslateTransformOperation::create(m_x.blend(fromX, progress), m_y.blend(fromY, progress));
}

PassRefPtr<TransformOperation> MatrixTransformOperation::blend(const TransformOperation* from, double progress, bool blendToIdentity)
{
    if (from && !from->isMatrixOperation())
        return this;
    
    if (blendToIdentity)
        return MatrixTransformOperation::create(m_a * (1. - progress) + progress,
                                            m_b * (1. - progress),
                                            m_c * (1. - progress),
                                            m_d * (1. - progress) + progress,
                                            m_e * (1. - progress),
                                            m_f * (1. - progress));

    const MatrixTransformOperation* fromOp = static_cast<const MatrixTransformOperation*>(from);
    double fromA = fromOp ? fromOp->m_a : 1.;
    double fromB = fromOp ? fromOp->m_b : 0;
    double fromC = fromOp ? fromOp->m_c : 0;
    double fromD = fromOp ? fromOp->m_d : 1.;
    double fromE = fromOp ? fromOp->m_e : 0;
    double fromF = fromOp ? fromOp->m_f : 0;
    
    return MatrixTransformOperation::create(fromA + (m_a - fromA) * progress,
                                        fromB + (m_b - fromB) * progress,
                                        fromC + (m_c - fromC) * progress,
                                        fromD + (m_d - fromD) * progress,
                                        fromE + (m_e - fromE) * progress,
                                        fromF + (m_f - fromF) * progress);
}

bool KeyframeList::operator==(const KeyframeList& o) const
{
    if (m_keyframes.size() != o.m_keyframes.size())
        return false;

    Vector<KeyframeValue>::const_iterator it2 = o.m_keyframes.begin();
    for (Vector<KeyframeValue>::const_iterator it1 = m_keyframes.begin(); it1 != m_keyframes.end(); ++it1) {
        if (it1->key != it2->key)
            return false;
        const RenderStyle& style1 = it1->style;
        const RenderStyle& style2 = it2->style;
        if (!(style1 == style2))
            return false;
        ++it2;
    }

    return true;
}

void
KeyframeList::insert(float inKey, const RenderStyle& inStyle)
{
    if (inKey < 0 || inKey > 1)
        return;

    for (size_t i = 0; i < m_keyframes.size(); ++i) {
        if (m_keyframes[i].key == inKey) {
            m_keyframes[i].style = inStyle;
            return;
        }
        if (m_keyframes[i].key > inKey) {
            // insert before
            m_keyframes.insert(i, KeyframeValue());
            m_keyframes[i].key = inKey;
            m_keyframes[i].style = inStyle;
            return;
        }
    }
    
    // append
    m_keyframes.append(KeyframeValue());
    m_keyframes[m_keyframes.size()-1].key = inKey;
    m_keyframes[m_keyframes.size()-1].style = inStyle;
}

Animation::Animation()
    : m_delay(RenderStyle::initialAnimationDelay())
    , m_direction(RenderStyle::initialAnimationDirection())
    , m_duration(RenderStyle::initialAnimationDuration())
    , m_iterationCount(RenderStyle::initialAnimationIterationCount())
    , m_name(RenderStyle::initialAnimationName())
    , m_property(RenderStyle::initialAnimationProperty())
    , m_timingFunction(RenderStyle::initialAnimationTimingFunction())
    , m_playState(RenderStyle::initialAnimationPlayState())
    , m_delaySet(false)
    , m_directionSet(false)
    , m_durationSet(false)
    , m_iterationCountSet(false)
    , m_nameSet(false)
    , m_playStateSet(false)
    , m_propertySet(false)
    , m_timingFunctionSet(false)
    , m_isNone(false)
{
}

Animation::Animation(const Animation& o)
    : RefCounted<Animation>()
    , m_delay(o.m_delay)
    , m_direction(o.m_direction)
    , m_duration(o.m_duration)
    , m_iterationCount(o.m_iterationCount)
    , m_name(o.m_name)
    , m_property(o.m_property)
    , m_timingFunction(o.m_timingFunction)
    , m_playState(o.m_playState)
    , m_delaySet(o.m_delaySet)
    , m_directionSet(o.m_directionSet)
    , m_durationSet(o.m_durationSet)
    , m_iterationCountSet(o.m_iterationCountSet)
    , m_nameSet(o.m_nameSet)
    , m_playStateSet(o.m_playStateSet)
    , m_propertySet(o.m_propertySet)
    , m_timingFunctionSet(o.m_timingFunctionSet)
    , m_isNone(o.m_isNone)
{
}

Animation& Animation::operator=(const Animation& o)
{
    m_delay = o.m_delay;
    m_direction = o.m_direction;
    m_duration = o.m_duration;
    m_iterationCount = o.m_iterationCount;
    m_name = o.m_name;
    m_playState = o.m_playState;
    m_property = o.m_property;
    m_timingFunction = o.m_timingFunction;

    m_delaySet = o.m_delaySet;
    m_directionSet = o.m_directionSet;
    m_durationSet = o.m_durationSet;
    m_iterationCountSet = o.m_iterationCountSet;
    m_nameSet = o.m_nameSet;
    m_playStateSet = o.m_playStateSet;
    m_propertySet = o.m_propertySet;
    m_timingFunctionSet = o.m_timingFunctionSet;

    m_isNone = o.m_isNone;

    return *this;
}

bool Animation::animationsMatch(const Animation* o, bool matchPlayStates /* = true */) const
{
    if (!o)
        return false;
    
    bool result = m_delay == o->m_delay &&
                  m_direction == o->m_direction &&
                  m_duration == o->m_duration &&
                  m_iterationCount == o->m_iterationCount &&
                  m_name == o->m_name &&
                  m_property == o->m_property && 
                  m_timingFunction == o->m_timingFunction &&
                  m_delaySet == o->m_delaySet &&
                  m_directionSet == o->m_directionSet &&
                  m_durationSet == o->m_durationSet &&
                  m_iterationCountSet == o->m_iterationCountSet &&
                  m_nameSet == o->m_nameSet &&
                  m_propertySet == o->m_propertySet &&
                  m_timingFunctionSet == o->m_timingFunctionSet &&
                  m_isNone == o->m_isNone;

    if (!result)
        return false;

    if (matchPlayStates && (m_playState != o->m_playState || m_playStateSet != o->m_playStateSet))
        return false;

    // now check keyframes
    if ((m_keyframeList.get() && !o->m_keyframeList.get()) || (!m_keyframeList.get() && o->m_keyframeList.get()))
        return false;
    if (!(m_keyframeList.get()))
        return true;
    return *m_keyframeList == *o->m_keyframeList;
}

#define FILL_UNSET_PROPERTY(test, propGet, propSet) \
for (i = 0; i < size() && (*this)[i]->test(); ++i) { } \
if (i < size() && i != 0) { \
    for (size_t j = 0; i < size(); ++i, ++j) \
        (*this)[i]->propSet((*this)[j]->propGet()); \
}

void AnimationList::fillUnsetProperties()
{
    size_t i;
    FILL_UNSET_PROPERTY(isDelaySet, delay, setDelay);
    FILL_UNSET_PROPERTY(isDirectionSet, direction, setDirection);
    FILL_UNSET_PROPERTY(isDurationSet, duration, setDuration);
    FILL_UNSET_PROPERTY(isIterationCountSet, iterationCount, setIterationCount);
    FILL_UNSET_PROPERTY(isPlayStateSet, playState, setPlayState);
    FILL_UNSET_PROPERTY(isNameSet, name, setName);
    FILL_UNSET_PROPERTY(isTimingFunctionSet, timingFunction, setTimingFunction);
    FILL_UNSET_PROPERTY(isPropertySet, property, setProperty);
}

bool AnimationList::operator==(const AnimationList& o) const
{
    if (size() != o.size())
        return false;
    for (size_t i = 0; i < size(); ++i)
        if (*at(i) != *o.at(i))
            return false;
    return true;
}

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : lineClamp(RenderStyle::initialLineClamp())
    , opacity(RenderStyle::initialOpacity())
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginTopCollapse(MCOLLAPSE)
    , marginBottomCollapse(MCOLLAPSE)
    , matchNearestMailBlockquoteColor(RenderStyle::initialMatchNearestMailBlockquoteColor())
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
    , m_boxShadow(0)
    , m_animations(0)
    , m_transitions(0)
    , m_mask(FillLayer(MaskFillLayer))
#if ENABLE(XBL)
    , bindingURI(0)
#endif
{
}

StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : RefCounted<StyleRareNonInheritedData>()
    , lineClamp(o.lineClamp)
    , opacity(o.opacity)
    , flexibleBox(o.flexibleBox)
    , marquee(o.marquee)
    , m_multiCol(o.m_multiCol)
    , m_transform(o.m_transform)
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginTopCollapse(o.marginTopCollapse)
    , marginBottomCollapse(o.marginBottomCollapse)
    , matchNearestMailBlockquoteColor(o.matchNearestMailBlockquoteColor)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
    , m_boxShadow(o.m_boxShadow ? new ShadowData(*o.m_boxShadow) : 0)
    , m_boxReflect(o.m_boxReflect)
    , m_animations(o.m_animations ? new AnimationList(*o.m_animations) : 0)
    , m_transitions(o.m_transitions ? new AnimationList(*o.m_transitions) : 0)
    , m_mask(o.m_mask)
    , m_maskBoxImage(o.m_maskBoxImage)
#if ENABLE(XBL)
    , bindingURI(o.bindingURI ? o.bindingURI->copy() : 0)
#endif
{
}

StyleRareNonInheritedData::~StyleRareNonInheritedData()
{
}

#if ENABLE(XBL)
bool StyleRareNonInheritedData::bindingsEquivalent(const StyleRareNonInheritedData& o) const
{
    if (this == &o) return true;
    if (!bindingURI && o.bindingURI || bindingURI && !o.bindingURI)
        return false;
    if (bindingURI && o.bindingURI && (*bindingURI != *o.bindingURI))
        return false;
    return true;
}
#endif

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return lineClamp == o.lineClamp
#if ENABLE(DASHBOARD_SUPPORT)
        && m_dashboardRegions == o.m_dashboardRegions
#endif
        && opacity == o.opacity
        && flexibleBox == o.flexibleBox
        && marquee == o.marquee
        && m_multiCol == o.m_multiCol
        && m_transform == o.m_transform
        && m_content == o.m_content
        && m_counterDirectives == o.m_counterDirectives
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginTopCollapse == o.marginTopCollapse
        && marginBottomCollapse == o.marginBottomCollapse
        && matchNearestMailBlockquoteColor == o.matchNearestMailBlockquoteColor
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
        && shadowDataEquivalent(o)
        && reflectionDataEquivalent(o)
        && animationDataEquivalent(o)
        && transitionDataEquivalent(o)
        && m_mask == o.m_mask
        && m_maskBoxImage == o.m_maskBoxImage
#if ENABLE(XBL)
        && bindingsEquivalent(o)
#endif
        ;
}

bool StyleRareNonInheritedData::shadowDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_boxShadow && o.m_boxShadow || m_boxShadow && !o.m_boxShadow)
        return false;
    if (m_boxShadow && o.m_boxShadow && (*m_boxShadow != *o.m_boxShadow))
        return false;
    return true;
}

bool StyleRareNonInheritedData::reflectionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (m_boxReflect != o.m_boxReflect) {
        if (!m_boxReflect || !o.m_boxReflect)
            return false;
        return *m_boxReflect == *o.m_boxReflect;
    }
    return true;

}

void StyleRareNonInheritedData::updateKeyframes(const CSSStyleSelector* styleSelector)
{
    if (m_animations) {
        for (size_t i = 0; i < m_animations->size(); ++i) {
            if ((*m_animations)[i]->isValidAnimation()) {
                // When keyframes have been parsed, execute this code.
                // RefPtr<KeyframeList> keyframe = styleSelector->findKeyframeRule((*m_animations)[i]->name());
                // (*m_animations)[i]->setAnimationKeyframe(keyframe);
            }
        }
    }
}

bool StyleRareNonInheritedData::animationDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_animations && o.m_animations || m_animations && !o.m_animations)
        return false;
    if (m_animations && o.m_animations && (*m_animations != *o.m_animations))
        return false;
    return true;
}

bool StyleRareNonInheritedData::transitionDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_transitions && o.m_transitions || m_transitions && !o.m_transitions)
        return false;
    if (m_transitions && o.m_transitions && (*m_transitions != *o.m_transitions))
        return false;
    return true;
}

StyleRareInheritedData::StyleRareInheritedData()
    : textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , textShadow(0)
    , textSecurity(RenderStyle::initialTextSecurity())
    , userModify(READ_ONLY)
    , wordBreak(RenderStyle::initialWordBreak())
    , wordWrap(RenderStyle::initialWordWrap())
    , nbspMode(NBNORMAL)
    , khtmlLineBreak(LBNORMAL)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
    , resize(RenderStyle::initialResize())
    , userSelect(RenderStyle::initialUserSelect())
{
}

StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : RefCounted<StyleRareInheritedData>()
    , textStrokeColor(o.textStrokeColor)
    , textStrokeWidth(o.textStrokeWidth)
    , textFillColor(o.textFillColor)
    , textShadow(o.textShadow ? new ShadowData(*o.textShadow) : 0)
    , highlight(o.highlight)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , wordWrap(o.wordWrap)
    , nbspMode(o.nbspMode)
    , khtmlLineBreak(o.khtmlLineBreak)
    , textSizeAdjust(o.textSizeAdjust)
    , resize(o.resize)
    , userSelect(o.userSelect)
{
}

StyleRareInheritedData::~StyleRareInheritedData()
{
    delete textShadow;
}

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return textStrokeColor == o.textStrokeColor
        && textStrokeWidth == o.textStrokeWidth
        && textFillColor == o.textFillColor
        && shadowDataEquivalent(o)
        && highlight == o.highlight
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && wordWrap == o.wordWrap
        && nbspMode == o.nbspMode
        && khtmlLineBreak == o.khtmlLineBreak
        && textSizeAdjust == o.textSizeAdjust
        && userSelect == o.userSelect;
}

bool StyleRareInheritedData::shadowDataEquivalent(const StyleRareInheritedData& o) const
{
    if (!textShadow && o.textShadow || textShadow && !o.textShadow)
        return false;
    if (textShadow && o.textShadow && (*textShadow != *o.textShadow))
        return false;
    return true;
}

StyleInheritedData::StyleInheritedData()
    : indent(RenderStyle::initialTextIndent())
    , line_height(RenderStyle::initialLineHeight())
    , list_style_image(RenderStyle::initialListStyleImage())
    , color(RenderStyle::initialColor())
    , m_effectiveZoom(RenderStyle::initialZoom())
    , horizontal_border_spacing(RenderStyle::initialHorizontalBorderSpacing())
    , vertical_border_spacing(RenderStyle::initialVerticalBorderSpacing())
    , widows(RenderStyle::initialWidows())
    , orphans(RenderStyle::initialOrphans())
    , page_break_inside(RenderStyle::initialPageBreak())
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : RefCounted<StyleInheritedData>()
    , indent(o.indent)
    , line_height(o.line_height)
    , list_style_image(o.list_style_image)
    , cursorData(o.cursorData)
    , font(o.font)
    , color(o.color)
    , m_effectiveZoom(o.m_effectiveZoom)
    , horizontal_border_spacing(o.horizontal_border_spacing)
    , vertical_border_spacing(o.vertical_border_spacing)
    , widows(o.widows)
    , orphans(o.orphans)
    , page_break_inside(o.page_break_inside)
{
}

static bool cursorDataEquivalent(const CursorList* c1, const CursorList* c2)
{
    if (c1 == c2)
        return true;
    if (!c1 && c2 || c1 && !c2)
        return false;
    return (*c1 == *c2);
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
        indent == o.indent &&
        line_height == o.line_height &&
        imagesEquivalent(list_style_image.get(), o.list_style_image.get()) &&
        cursorDataEquivalent(cursorData.get(), o.cursorData.get()) &&
        font == o.font &&
        color == o.color &&
        m_effectiveZoom == o.m_effectiveZoom &&
        horizontal_border_spacing == o.horizontal_border_spacing &&
        vertical_border_spacing == o.vertical_border_spacing &&
        widows == o.widows &&
        orphans == o.orphans &&
        page_break_inside == o.page_break_inside;
}

static inline bool operator!=(const CounterContent& a, const CounterContent& b)
{
    return a.identifier() != b.identifier()
        || a.listStyle() != b.listStyle()
        || a.separator() != b.separator();
}

// ----------------------------------------------------------

void* RenderStyle::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderStyle::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RenderStyle::arenaDelete(RenderArena *arena)
{
    RenderStyle *ps = pseudoStyle;
    RenderStyle *prev = 0;
    
    while (ps) {
        prev = ps;
        ps = ps->pseudoStyle;
        // to prevent a double deletion.
        // this works only because the styles below aren't really shared
        // Dirk said we need another construct as soon as these are shared
        prev->pseudoStyle = 0;
        prev->deref(arena);
    }
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)this, this);
}

inline RenderStyle *initDefaultStyle()
{
    if (!defaultStyle)
        defaultStyle = ::new RenderStyle(true);
    return defaultStyle;
}

RenderStyle::RenderStyle()
    : box(initDefaultStyle()->box)
    , visual(defaultStyle->visual)
    , background(defaultStyle->background)
    , surround(defaultStyle->surround)
    , rareNonInheritedData(defaultStyle->rareNonInheritedData)
    , rareInheritedData(defaultStyle->rareInheritedData)
    , inherited(defaultStyle->inherited)
    , pseudoStyle(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
    , m_ref(0)
#if ENABLE(SVG)
    , m_svgStyle(defaultStyle->m_svgStyle)
#endif
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
}

RenderStyle::RenderStyle(bool)
    : pseudoStyle(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
    , m_ref(1)
{
    setBitDefaults();

    box.init();
    visual.init();
    background.init();
    surround.init();
    rareNonInheritedData.init();
    rareNonInheritedData.access()->flexibleBox.init();
    rareNonInheritedData.access()->marquee.init();
    rareNonInheritedData.access()->m_multiCol.init();
    rareNonInheritedData.access()->m_transform.init();
    rareInheritedData.init();
    inherited.init();
    
#if ENABLE(SVG)
    m_svgStyle.init();
#endif
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
    , box(o.box)
    , visual(o.visual)
    , background(o.background)
    , surround(o.surround)
    , rareNonInheritedData(o.rareNonInheritedData)
    , rareInheritedData(o.rareInheritedData)
    , inherited(o.inherited)
    , pseudoStyle(0)
    , m_pseudoState(o.m_pseudoState)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
    , m_ref(0)
#if ENABLE(SVG)
    , m_svgStyle(o.m_svgStyle)
#endif
{
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent)
{
    rareInheritedData = inheritParent->rareInheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
#if ENABLE(SVG)
    if (m_svgStyle != inheritParent->m_svgStyle)
        m_svgStyle.access()->inheritFrom(inheritParent->m_svgStyle.get());
#endif
}

RenderStyle::~RenderStyle()
{
}

bool RenderStyle::operator==(const RenderStyle& o) const
{
    // compare everything except the pseudoStyle pointer
    return inherited_flags == o.inherited_flags &&
            noninherited_flags == o.noninherited_flags &&
            box == o.box &&
            visual == o.visual &&
            background == o.background &&
            surround == o.surround &&
            rareNonInheritedData == o.rareNonInheritedData &&
            rareInheritedData == o.rareInheritedData &&
            inherited == o.inherited
#if ENABLE(SVG)
            && m_svgStyle == o.m_svgStyle
#endif
            ;
}

bool RenderStyle::isStyleAvailable() const
{
    return this != CSSStyleSelector::styleNotYetAvailable();
}

static inline int pseudoBit(RenderStyle::PseudoId pseudo)
{
    return 1 << (pseudo - 1);
}

bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    return pseudoBit(pseudo) & noninherited_flags._pseudoBits;
}

void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    noninherited_flags._pseudoBits |= pseudoBit(pseudo);
}

RenderStyle* RenderStyle::getPseudoStyle(PseudoId pid)
{
    if (!pseudoStyle || styleType() != NOPSEUDO)
        return 0;
    RenderStyle* ps = pseudoStyle;
    while (ps && ps->styleType() != pid)
        ps = ps->pseudoStyle;
    return ps;
}

void RenderStyle::addPseudoStyle(RenderStyle* pseudo)
{
    if (!pseudo)
        return;
    pseudo->ref();
    pseudo->pseudoStyle = pseudoStyle;
    pseudoStyle = pseudo;
}

bool RenderStyle::inheritedNotEqual(RenderStyle* other) const
{
    return inherited_flags != other->inherited_flags ||
           inherited != other->inherited ||
#if ENABLE(SVG)
           m_svgStyle->inheritedNotEqual(other->m_svgStyle.get()) ||
#endif
           rareInheritedData != other->rareInheritedData;
}

bool positionedObjectMoved(const LengthBox& a, const LengthBox& b)
{
    // If any unit types are different, then we can't guarantee
    // that this was just a movement.
    if (a.left.type() != b.left.type() ||
        a.right.type() != b.right.type() ||
        a.top.type() != b.top.type() ||
        a.bottom.type() != b.bottom.type())
        return false;
        
    // Only one unit can be non-auto in the horizontal direction and
    // in the vertical direction.  Otherwise the adjustment of values
    // is changing the size of the box.
    if (!a.left.isIntrinsicOrAuto() && !a.right.isIntrinsicOrAuto())
        return false;
    if (!a.top.isIntrinsicOrAuto() && !a.bottom.isIntrinsicOrAuto())
        return false;
        
    // One of the units is fixed or percent in both directions and stayed
    // that way in the new style.  Therefore all we are doing is moving.
    return true;
}

/*
  compares two styles. The result gives an idea of the action that
  needs to be taken when replacing the old style with a new one.

  CbLayout: The containing block of the object needs a relayout.
  Layout: the RenderObject needs a relayout after the style change
  Visible: The change is visible, but no relayout is needed
  NonVisible: The object does need neither repaint nor relayout after
       the change.

  ### TODO:
  A lot can be optimised here based on the display type, lots of
  optimisations are unimplemented, and currently result in the
  worst case result causing a relayout of the containing block.
*/
RenderStyle::Diff RenderStyle::diff(const RenderStyle* other) const
{
#if ENABLE(SVG)
    // This is horribly inefficient.  Eventually we'll have to integrate
    // this more directly by calling: Diff svgDiff = svgStyle->diff(other)
    // and then checking svgDiff and returning from the appropriate places below.
    if (m_svgStyle != other->m_svgStyle)
        return Layout;
#endif

    if (box->width != other->box->width ||
        box->min_width != other->box->min_width ||
        box->max_width != other->box->max_width ||
        box->height != other->box->height ||
        box->min_height != other->box->min_height ||
        box->max_height != other->box->max_height)
        return Layout;
    
    if (box->vertical_align != other->box->vertical_align || noninherited_flags._vertical_align != other->noninherited_flags._vertical_align)
        return Layout;
    
    if (box->boxSizing != other->box->boxSizing)
        return Layout;
    
    if (surround->margin != other->surround->margin)
        return Layout;
        
    if (surround->padding != other->surround->padding)
        return Layout;
    
    if (rareNonInheritedData.get() != other->rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_appearance != other->rareNonInheritedData->m_appearance ||
            rareNonInheritedData->marginTopCollapse != other->rareNonInheritedData->marginTopCollapse ||
            rareNonInheritedData->marginBottomCollapse != other->rareNonInheritedData->marginBottomCollapse ||
            rareNonInheritedData->lineClamp != other->rareNonInheritedData->lineClamp ||
            rareNonInheritedData->textOverflow != other->rareNonInheritedData->textOverflow)
            return Layout;
        
        if (rareNonInheritedData->flexibleBox.get() != other->rareNonInheritedData->flexibleBox.get() &&
            *rareNonInheritedData->flexibleBox.get() != *other->rareNonInheritedData->flexibleBox.get())
            return Layout;
        
        if (!rareNonInheritedData->shadowDataEquivalent(*other->rareNonInheritedData.get()))
            return Layout;

        if (!rareNonInheritedData->reflectionDataEquivalent(*other->rareNonInheritedData.get()))
            return Layout;
    
        if (rareNonInheritedData->m_multiCol.get() != other->rareNonInheritedData->m_multiCol.get() &&
            *rareNonInheritedData->m_multiCol.get() != *other->rareNonInheritedData->m_multiCol.get())
            return Layout;
        
        if (rareNonInheritedData->m_transform.get() != other->rareNonInheritedData->m_transform.get() &&
            *rareNonInheritedData->m_transform.get() != *other->rareNonInheritedData->m_transform.get())
            return Layout;

#if ENABLE(DASHBOARD_SUPPORT)
        // If regions change, trigger a relayout to re-calc regions.
        if (rareNonInheritedData->m_dashboardRegions != other->rareNonInheritedData->m_dashboardRegions)
            return Layout;
#endif
    }

    if (rareInheritedData.get() != other->rareInheritedData.get()) {
        if (rareInheritedData->highlight != other->rareInheritedData->highlight ||
            rareInheritedData->textSizeAdjust != other->rareInheritedData->textSizeAdjust ||
            rareInheritedData->wordBreak != other->rareInheritedData->wordBreak ||
            rareInheritedData->wordWrap != other->rareInheritedData->wordWrap ||
            rareInheritedData->nbspMode != other->rareInheritedData->nbspMode ||
            rareInheritedData->khtmlLineBreak != other->rareInheritedData->khtmlLineBreak ||
            rareInheritedData->textSecurity != other->rareInheritedData->textSecurity)
            return Layout;
        
        if (!rareInheritedData->shadowDataEquivalent(*other->rareInheritedData.get()))
            return Layout;
            
        if (textStrokeWidth() != other->textStrokeWidth())
            return Layout;
    }

    if (inherited->indent != other->inherited->indent ||
        inherited->line_height != other->inherited->line_height ||
        inherited->list_style_image != other->inherited->list_style_image ||
        inherited->font != other->inherited->font ||
        inherited->horizontal_border_spacing != other->inherited->horizontal_border_spacing ||
        inherited->vertical_border_spacing != other->inherited->vertical_border_spacing ||
        inherited_flags._box_direction != other->inherited_flags._box_direction ||
        inherited_flags._visuallyOrdered != other->inherited_flags._visuallyOrdered ||
        inherited_flags._htmlHacks != other->inherited_flags._htmlHacks ||
        noninherited_flags._position != other->noninherited_flags._position ||
        noninherited_flags._floating != other->noninherited_flags._floating ||
        noninherited_flags._originalDisplay != other->noninherited_flags._originalDisplay)
        return Layout;

   
    if (((int)noninherited_flags._effectiveDisplay) >= TABLE) {
        if (inherited_flags._border_collapse != other->inherited_flags._border_collapse ||
            inherited_flags._empty_cells != other->inherited_flags._empty_cells ||
            inherited_flags._caption_side != other->inherited_flags._caption_side ||
            noninherited_flags._table_layout != other->noninherited_flags._table_layout)
            return Layout;
        
        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse &&
            (borderTopStyle() == BHIDDEN && other->borderTopStyle() == BNONE ||
             borderTopStyle() == BNONE && other->borderTopStyle() == BHIDDEN ||
             borderBottomStyle() == BHIDDEN && other->borderBottomStyle() == BNONE ||
             borderBottomStyle() == BNONE && other->borderBottomStyle() == BHIDDEN ||
             borderLeftStyle() == BHIDDEN && other->borderLeftStyle() == BNONE ||
             borderLeftStyle() == BNONE && other->borderLeftStyle() == BHIDDEN ||
             borderRightStyle() == BHIDDEN && other->borderRightStyle() == BNONE ||
             borderRightStyle() == BNONE && other->borderRightStyle() == BHIDDEN))
            return Layout;
    }

    if (noninherited_flags._effectiveDisplay == LIST_ITEM) {
        if (inherited_flags._list_style_type != other->inherited_flags._list_style_type ||
            inherited_flags._list_style_position != other->inherited_flags._list_style_position)
            return Layout;
    }

    if (inherited_flags._text_align != other->inherited_flags._text_align ||
        inherited_flags._text_transform != other->inherited_flags._text_transform ||
        inherited_flags._direction != other->inherited_flags._direction ||
        inherited_flags._white_space != other->inherited_flags._white_space ||
        noninherited_flags._clear != other->noninherited_flags._clear)
        return Layout;

    // Overflow returns a layout hint.
    if (noninherited_flags._overflowX != other->noninherited_flags._overflowX ||
        noninherited_flags._overflowY != other->noninherited_flags._overflowY)
        return Layout;
 
    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other->borderLeftWidth() ||
        borderTopWidth() != other->borderTopWidth() ||
        borderBottomWidth() != other->borderBottomWidth() ||
        borderRightWidth() != other->borderRightWidth())
        return Layout;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    const CounterDirectiveMap* mapA = rareNonInheritedData->m_counterDirectives.get();
    const CounterDirectiveMap* mapB = other->rareNonInheritedData->m_counterDirectives.get();
    if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
        return Layout;
    if (visual->counterIncrement != other->visual->counterIncrement ||
        visual->counterReset != other->visual->counterReset)
        return Layout;

    if (inherited->m_effectiveZoom != other->inherited->m_effectiveZoom)
        return Layout;

    // Make sure these left/top/right/bottom checks stay below all layout checks and above
    // all visible checks.
    if (position() != StaticPosition) {
        if (surround->offset != other->surround->offset) {
             // Optimize for the case where a positioned layer is moving but not changing size.
            if (position() == AbsolutePosition && positionedObjectMoved(surround->offset, other->surround->offset))
                return LayoutPositionedMovementOnly;

            // FIXME: We will need to do a bit of work in RenderObject/Box::setStyle before we
            // can stop doing a layout when relative positioned objects move.  In particular, we'll need
            // to update scrolling positions and figure out how to do a repaint properly of the updated layer.
            //if (other->position() == RelativePosition)
            //    return RepaintLayer;
            //else
                return Layout;
        } else if (box->z_index != other->box->z_index || box->z_auto != other->box->z_auto ||
                 visual->clip != other->visual->clip || visual->hasClip != other->visual->hasClip)
            return RepaintLayer;
    }

    if (rareNonInheritedData->opacity != other->rareNonInheritedData->opacity ||
        rareNonInheritedData->m_mask != other->rareNonInheritedData->m_mask ||
        rareNonInheritedData->m_maskBoxImage != other->rareNonInheritedData->m_maskBoxImage)
        return RepaintLayer;

    if (inherited->color != other->inherited->color ||
        inherited_flags._visibility != other->inherited_flags._visibility ||
        inherited_flags._text_decorations != other->inherited_flags._text_decorations ||
        inherited_flags._force_backgrounds_to_white != other->inherited_flags._force_backgrounds_to_white ||
        surround->border != other->surround->border ||
        *background.get() != *other->background.get() ||
        visual->textDecoration != other->visual->textDecoration ||
        rareInheritedData->userModify != other->rareInheritedData->userModify ||
        rareInheritedData->userSelect != other->rareInheritedData->userSelect ||
        rareNonInheritedData->userDrag != other->rareNonInheritedData->userDrag ||
        rareNonInheritedData->m_borderFit != other->rareNonInheritedData->m_borderFit ||
        rareInheritedData->textFillColor != other->rareInheritedData->textFillColor ||
        rareInheritedData->textStrokeColor != other->rareInheritedData->textStrokeColor)
        return Repaint;

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any repaint or layout.

    // Animations don't need to be checked either.  We always set the new style on the RenderObject, so we will get a chance to fire off
    // the resulting transition properly.
    return Equal;
}

void RenderStyle::setClip( Length top, Length right, Length bottom, Length left )
{
    StyleVisualData *data = visual.access();
    data->clip.top = top;
    data->clip.right = right;
    data->clip.bottom = bottom;
    data->clip.left = left;
}

void RenderStyle::addCursor(CachedImage* image, const IntPoint& hotSpot)
{
    CursorData data;
    data.cursorImage = image;
    data.hotSpot = hotSpot;
    if (!inherited.access()->cursorData)
        inherited.access()->cursorData = CursorList::create();
    inherited.access()->cursorData->append(data);
}

void RenderStyle::setCursorList(PassRefPtr<CursorList> other)
{
    inherited.access()->cursorData = other;
}

void RenderStyle::clearCursorList()
{
    inherited.access()->cursorData = CursorList::create();
}

bool RenderStyle::contentDataEquivalent(const RenderStyle* otherStyle) const
{
    ContentData* c1 = rareNonInheritedData->m_content.get();
    ContentData* c2 = otherStyle->rareNonInheritedData->m_content.get();

    while (c1 && c2) {
        if (c1->m_type != c2->m_type)
            return false;

        switch (c1->m_type) {
            case CONTENT_NONE:
                break;
            case CONTENT_TEXT:
                if (!equal(c1->m_content.m_text, c2->m_content.m_text))
                    return false;
                break;
            case CONTENT_OBJECT:
                if (!imagesEquivalent(c1->m_content.m_image, c2->m_content.m_image))
                    return false;
                break;
            case CONTENT_COUNTER:
                if (*c1->m_content.m_counter != *c2->m_content.m_counter)
                    return false;
                break;
        }

        c1 = c1->m_next;
        c2 = c2->m_next;
    }

    return !c1 && !c2;
}

void RenderStyle::clearContent()
{
    if (rareNonInheritedData->m_content)
        rareNonInheritedData->m_content->clear();
}

void RenderStyle::setContent(StyleImage* image, bool add)
{
    if (!image)
        return; // The object is null. Nothing to do. Just bail.

    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    ContentData* newContentData;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content.set(newContentData);

    newContentData->m_content.m_image = image;
    newContentData->m_type = CONTENT_OBJECT;
    image->ref();
}

void RenderStyle::setContent(StringImpl* s, bool add)
{
    if (!s)
        return; // The string is null. Nothing to do. Just bail.
    
    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    if (add && lastContent) {
        if (lastContent->m_type == CONTENT_TEXT) {
            // We can augment the existing string and share this ContentData node.
            StringImpl* oldStr = lastContent->m_content.m_text;
            String newStr = oldStr;
            newStr.append(s);
            newStr.impl()->ref();
            oldStr->deref();
            lastContent->m_content.m_text = newStr.impl();
            return;
        }
    }

    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;
    
    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content.set(newContentData);
    
    newContentData->m_content.m_text = s;
    newContentData->m_content.m_text->ref();
    newContentData->m_type = CONTENT_TEXT;
}

void RenderStyle::setContent(CounterContent* c, bool add)
{
    if (!c)
        return;

    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content.set(newContentData);

    newContentData->m_content.m_counter = c;
    newContentData->m_type = CONTENT_COUNTER;
}

void ContentData::clear()
{
    switch (m_type) {
        case CONTENT_NONE:
            break;
        case CONTENT_OBJECT:
            m_content.m_image->deref();
            break;
        case CONTENT_TEXT:
            m_content.m_text->deref();
            break;
        case CONTENT_COUNTER:
            delete m_content.m_counter;
            break;
    }

    ContentData* n = m_next;
    m_type = CONTENT_NONE;
    m_next = 0;

    // Reverse the list so we can delete without recursing.
    ContentData* last = 0;
    ContentData* c;
    while ((c = n)) {
        n = c->m_next;
        c->m_next = last;
        last = c;
    }
    for (c = last; c; c = n) {
        n = c->m_next;
        c->m_next = 0;
        delete c;
    }
}

void RenderStyle::applyTransform(AffineTransform& transform, const IntSize& borderBoxSize, bool includeTransformOrigin) const
{
    // transform-origin brackets the transform with translate operations.
    // Optimize for the case where the only transform is a translation, since the transform-origin is irrelevant
    // in that case.
    bool applyTransformOrigin = false;
    unsigned s = rareNonInheritedData->m_transform->m_operations.size();
    unsigned i;
    if (includeTransformOrigin) {
        for (i = 0; i < s; i++) {
            if (!rareNonInheritedData->m_transform->m_operations[i]->isTranslateOperation()) {
                applyTransformOrigin = true;
                break;
            }
        }
    }
    
    if (applyTransformOrigin)
        transform.translate(transformOriginX().calcFloatValue(borderBoxSize.width()), transformOriginY().calcFloatValue(borderBoxSize.height()));
    
    for (i = 0; i < s; i++)
        rareNonInheritedData->m_transform->m_operations[i]->apply(transform, borderBoxSize);
        
    if (applyTransformOrigin)
        transform.translate(-transformOriginX().calcFloatValue(borderBoxSize.width()), -transformOriginY().calcFloatValue(borderBoxSize.height()));
}

#if ENABLE(XBL)
BindingURI::BindingURI(StringImpl* uri) 
    : m_next(0)
{ 
    m_uri = uri;
    if (uri)
        uri->ref();
}

BindingURI::~BindingURI()
{
    if (m_uri)
        m_uri->deref();
    delete m_next;
}

BindingURI* BindingURI::copy()
{
    BindingURI* newBinding = new BindingURI(m_uri);
    if (next()) {
        BindingURI* nextCopy = next()->copy();
        newBinding->setNext(nextCopy);
    }
    
    return newBinding;
}

bool BindingURI::operator==(const BindingURI& o) const
{
    if ((m_next && !o.m_next) || (!m_next && o.m_next) ||
        (m_next && o.m_next && *m_next != *o.m_next))
        return false;
    
    if (m_uri == o.m_uri)
        return true;
    if (!m_uri || !o.m_uri)
        return false;
    
    return String(m_uri) == String(o.m_uri);
}

void RenderStyle::addBindingURI(StringImpl* uri)
{
    BindingURI* binding = new BindingURI(uri);
    if (!bindingURIs())
        SET_VAR(rareNonInheritedData, bindingURI, binding)
    else 
        for (BindingURI* b = bindingURIs(); b; b = b->next()) {
            if (!b->next())
                b->setNext(binding);
        }
}
#endif

void RenderStyle::setTextShadow(ShadowData* val, bool add)
{
    StyleRareInheritedData* rareData = rareInheritedData.access(); 
    if (!add) {
        delete rareData->textShadow;
        rareData->textShadow = val;
        return;
    }

    val->next = rareData->textShadow;
    rareData->textShadow = val;
}

void RenderStyle::setBoxShadow(ShadowData* shadowData, bool add)
{
    StyleRareNonInheritedData* rareData = rareNonInheritedData.access(); 
    if (!add) {
        rareData->m_boxShadow.set(shadowData);
        return;
    }

    shadowData->next = rareData->m_boxShadow.release();
    rareData->m_boxShadow.set(shadowData);
}

ShadowData::ShadowData(const ShadowData& o)
    : x(o.x)
    , y(o.y)
    , blur(o.blur)
    , color(o.color)
{
    next = o.next ? new ShadowData(*o.next) : 0;
}

bool ShadowData::operator==(const ShadowData& o) const
{
    if ((next && !o.next) || (!next && o.next) ||
        (next && o.next && *next != *o.next))
        return false;
    
    return x == o.x && y == o.y && blur == o.blur && color == o.color;
}

bool operator==(const CounterDirectives& a, const CounterDirectives& b)
{
    if (a.m_reset != b.m_reset || a.m_increment != b.m_increment)
        return false;
    if (a.m_reset && a.m_resetValue != b.m_resetValue)
        return false;
    if (a.m_increment && a.m_incrementValue != b.m_incrementValue)
        return false;
    return true;
}

const CounterDirectiveMap* RenderStyle::counterDirectives() const
{
    return rareNonInheritedData->m_counterDirectives.get();
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    OwnPtr<CounterDirectiveMap>& map = rareNonInheritedData.access()->m_counterDirectives;
    if (!map)
        map.set(new CounterDirectiveMap);
    return *map.get();
}

#if ENABLE(DASHBOARD_SUPPORT)
const Vector<StyleDashboardRegion>& RenderStyle::initialDashboardRegions()
{ 
    static Vector<StyleDashboardRegion> emptyList;
    return emptyList;
}

const Vector<StyleDashboardRegion>& RenderStyle::noneDashboardRegions()
{ 
    static Vector<StyleDashboardRegion> noneList;
    static bool noneListInitialized = false;
    
    if (!noneListInitialized) {
        StyleDashboardRegion region;
        region.label = "";
        region.offset.top  = Length();
        region.offset.right = Length();
        region.offset.bottom = Length();
        region.offset.left = Length();
        region.type = StyleDashboardRegion::None;
        noneList.append (region);
        noneListInitialized = true;
    }
    return noneList;
}
#endif

void RenderStyle::adjustAnimations()
{
    AnimationList* animationList = rareNonInheritedData->m_animations.get();
    if (!animationList)
        return;

    // get rid of empty transitions and anything beyond them
    for (size_t i = 0; i < animationList->size(); ++i) {
        if ((*animationList)[i]->isEmpty()) {
            animationList->resize(i);
            break;
        }
    }

    if (animationList->isEmpty()) {
        clearAnimations();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    animationList->fillUnsetProperties();
}

void RenderStyle::adjustTransitions()
{
    AnimationList* transitionList = rareNonInheritedData->m_transitions.get();
    if (!transitionList)
        return;

    // get rid of empty transitions and anything beyond them
    for (size_t i = 0; i < transitionList->size(); ++i) {
        if ((*transitionList)[i]->isEmpty()) {
            transitionList->resize(i);
            break;
        }
    }

    if (transitionList->isEmpty()) {
        clearTransitions();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    transitionList->fillUnsetProperties();
        
    // Make sure there are no duplicate properties. This is an O(n^2) algorithm
    // but the lists tend to be very short, so it is probably ok
    for (size_t i = 0; i < transitionList->size(); ++i) {
        for (size_t j = i+1; j < transitionList->size(); ++j) {
            if ((*transitionList)[i]->property() == (*transitionList)[j]->property()) {
                // toss i
                transitionList->remove(i);
                j = i;
            }
        }
    }
}

AnimationList* RenderStyle::accessAnimations()
{
    if (!rareNonInheritedData.access()->m_animations)
        rareNonInheritedData.access()->m_animations.set(new AnimationList());
    return rareNonInheritedData->m_animations.get();
}

AnimationList* RenderStyle::accessTransitions()
{
    if (!rareNonInheritedData.access()->m_transitions)
        rareNonInheritedData.access()->m_transitions.set(new AnimationList());
    return rareNonInheritedData->m_transitions.get();
}

const Animation* RenderStyle::transitionForProperty(int property)
{
    if (transitions()) {
        for (size_t i = 0; i < transitions()->size(); ++i) {
            const Animation* p = (*transitions())[i].get();
            if (p->property() == cAnimateAll || p->property() == property) {
                return p;
            }
        }
    }
    return 0;
}

}
