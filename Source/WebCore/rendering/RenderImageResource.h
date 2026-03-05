/*
 * Copyright (C) 1999 Lars Knoll <knoll@kde.org>
 * Copyright (C) 1999 Antti Koivisto <koivisto@kde.org>
 * Copyright (C) 2006 Allan Sandfeld Jensen <kde@carewolf.com>
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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

#include <WebCore/CachedImage.h>
#include <WebCore/CachedResourceHandle.h>
#include <WebCore/StyleImage.h>
#include <wtf/CheckedPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CachedImage;
class RenderElement;

class RenderImageResource final : public CanMakeCheckedPtr<RenderImageResource> {
    WTF_MAKE_NONCOPYABLE(RenderImageResource);
    WTF_MAKE_TZONE_ALLOCATED(RenderImageResource);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(RenderImageResource);
public:
    explicit RenderImageResource();
    explicit RenderImageResource(Style::Image*);
    ~RenderImageResource();

    void initialize(RenderElement&);
    void willBeDestroyed();

    void clearCachedImage();
    void setCachedImage(CachedResourceHandle<CachedImage>&&);
    CachedImage* cachedImage() const { return m_styleImage ? m_styleImage->cachedImage() : nullptr; }

    void resetAnimation();

    Ref<Image> image(const IntSize& size = { }) const;
    bool currentFrameIsComplete() const;
    bool errorOccurred() const { return m_styleImage ? m_styleImage->errorOccurred() : false; }

    void setContainerContext(const IntSize&, const URL&);

    bool imageHasRelativeWidth() const { return m_styleImage ? m_styleImage->imageHasRelativeWidth() : false; }
    bool imageHasRelativeHeight() const { return m_styleImage ? m_styleImage->imageHasRelativeHeight() : false; }

    inline LayoutSize imageSize(float multiplier) const { return imageSize(multiplier, CachedImage::UsedSize); }
    inline LayoutSize intrinsicSize(float multiplier) const { return imageSize(multiplier, CachedImage::IntrinsicSize); }

    WrappedImagePtr imagePtr() const { return m_styleImage ? m_styleImage->data() : nullptr; }

private:
    LayoutSize imageSize(float multiplier, CachedImage::SizeType) const;

    SingleThreadWeakPtr<RenderElement> m_renderer;
    RefPtr<Style::Image> m_styleImage;
};

} // namespace WebCore
