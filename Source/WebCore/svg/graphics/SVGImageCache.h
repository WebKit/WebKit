/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
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
 */

#pragma once

#include <WebCore/Image.h>
#include <WebCore/StyleImageContainerContext.h>
#include <WebCore/StyleImageContainerContextKey.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ImageBuffer;
class SVGImage;
class SVGImageForContainer;

class SVGImageCache {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(SVGImageCache, WEBCORE_EXPORT);
public:
    explicit SVGImageCache(SVGImage*);
    ~SVGImageCache();

    void registerContainerContext(const ImageContainerContextKey&, ImageContainerContext&&);
    void unregisterContainerContext(const ImageContainerContextKey&);

    FloatSize imageSizeForKey(const ImageContainerContextKey*) const;
    Image* imageForKey(const ImageContainerContextKey*) const;

private:
    Image* findImageForKey(const ImageContainerContextKey*) const;

    using ImageForContainerMap = HashMap<SingleThreadWeakPtr<const ImageContainerContextKey>, Ref<SVGImageForContainer>>;

    WeakPtr<SVGImage> m_svgImage;
    ImageForContainerMap m_imageForContainerMap;
};

} // namespace WebCore
