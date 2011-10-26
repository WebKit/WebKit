/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef ImageBySizeCache_h
#define ImageBySizeCache_h

#include "IntSizeHash.h"
#include <wtf/HashCountedSet.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Image;
class RenderObject;

typedef pair<IntSize, int> SizeCountPair;
typedef HashMap<const RenderObject*, SizeCountPair> RenderObjectSizeCountMap;

class ImageBySizeCache {
public:
    ImageBySizeCache();

    void addClient(const RenderObject*, const IntSize&);
    void removeClient(const RenderObject*);
    void setClient(const RenderObject*, const IntSize&);

    Image* getImage(const RenderObject*, const IntSize&);
    void putImage(const IntSize&, PassRefPtr<Image>);

    void clear();

    Image* imageForSize(const IntSize&) const;
    Image* imageForRenderer(const RenderObject*, IntSize* lookedUpSize = 0) const;
    const RenderObjectSizeCountMap& clients() const { return m_clients; }

private:
    HashCountedSet<IntSize> m_sizes; // A count of how many times a given image size is in use.
    RenderObjectSizeCountMap m_clients; // A map from RenderObjects (with entry count) to image sizes.
    HashMap<IntSize, RefPtr<Image> > m_images; // A cache of Image objects by image size.
};

} // namespace WebCore

#endif // ImageBySizeCache_h
