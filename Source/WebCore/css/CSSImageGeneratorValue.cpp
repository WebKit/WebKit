/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSImageGeneratorValue.h"

#include "CSSCanvasValue.h"
#include "CSSCrossfadeValue.h"
#include "CSSGradientValue.h"
#include "Image.h"
#include "MemoryInstrumentation.h"
#include "RenderObject.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

CSSImageGeneratorValue::CSSImageGeneratorValue(ClassType classType)
    : CSSValue(classType)
{
}

CSSImageGeneratorValue::~CSSImageGeneratorValue()
{
}

void CSSImageGeneratorValue::addClient(RenderObject* renderer, const IntSize& size)
{
    ref();

    ASSERT(renderer);
    if (!size.isEmpty())
        m_sizes.add(size);

    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    if (it == m_clients.end())
        m_clients.add(renderer, SizeAndCount(size, 1));
    else {
        SizeAndCount& sizeCount = it->second;
        ++sizeCount.count;
    }
}

void CSSImageGeneratorValue::removeClient(RenderObject* renderer)
{
    ASSERT(renderer);
    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    ASSERT(it != m_clients.end());

    IntSize removedImageSize;
    SizeAndCount& sizeCount = it->second;
    IntSize size = sizeCount.size;
    if (!size.isEmpty()) {
        m_sizes.remove(size);
        if (!m_sizes.contains(size))
            m_images.remove(size);
    }

    if (!--sizeCount.count)
        m_clients.remove(renderer);

    deref();
}

Image* CSSImageGeneratorValue::getImage(RenderObject* renderer, const IntSize& size)
{
    RenderObjectSizeCountMap::iterator it = m_clients.find(renderer);
    if (it != m_clients.end()) {
        SizeAndCount& sizeCount = it->second;
        IntSize oldSize = sizeCount.size;
        if (oldSize != size) {
            RefPtr<CSSImageGeneratorValue> protect(this);
            removeClient(renderer);
            addClient(renderer, size);
        }
    }

    // Don't generate an image for empty sizes.
    if (size.isEmpty())
        return 0;

    // Look up the image in our cache.
    return m_images.get(size).get();
}

void CSSImageGeneratorValue::putImage(const IntSize& size, PassRefPtr<Image> image)
{
    m_images.add(size, image);
}

void CSSImageGeneratorValue::reportBaseClassMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, MemoryInstrumentation::CSS);
    info.addHashCountedSet(m_sizes);
    info.addHashMap(m_clients);
    info.addHashMap(m_images); // FIXME: instrument Image
}

PassRefPtr<Image> CSSImageGeneratorValue::image(RenderObject* renderer, const IntSize& size)
{
    switch (classType()) {
    case CanvasClass:
        return static_cast<CSSCanvasValue*>(this)->image(renderer, size);
    case CrossfadeClass:
        return static_cast<CSSCrossfadeValue*>(this)->image(renderer, size);
    case LinearGradientClass:
        return static_cast<CSSLinearGradientValue*>(this)->image(renderer, size);
    case RadialGradientClass:
        return static_cast<CSSRadialGradientValue*>(this)->image(renderer, size);
    default:
        ASSERT_NOT_REACHED();
    }
    return 0;
}

bool CSSImageGeneratorValue::isFixedSize() const
{
    switch (classType()) {
    case CanvasClass:
        return static_cast<const CSSCanvasValue*>(this)->isFixedSize();
    case CrossfadeClass:
        return static_cast<const CSSCrossfadeValue*>(this)->isFixedSize();
    case LinearGradientClass:
        return static_cast<const CSSLinearGradientValue*>(this)->isFixedSize();
    case RadialGradientClass:
        return static_cast<const CSSRadialGradientValue*>(this)->isFixedSize();
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

IntSize CSSImageGeneratorValue::fixedSize(const RenderObject* renderer)
{
    switch (classType()) {
    case CanvasClass:
        return static_cast<CSSCanvasValue*>(this)->fixedSize(renderer);
    case CrossfadeClass:
        return static_cast<CSSCrossfadeValue*>(this)->fixedSize(renderer);
    case LinearGradientClass:
        return static_cast<CSSLinearGradientValue*>(this)->fixedSize(renderer);
    case RadialGradientClass:
        return static_cast<CSSRadialGradientValue*>(this)->fixedSize(renderer);
    default:
        ASSERT_NOT_REACHED();
    }
    return IntSize();
}

bool CSSImageGeneratorValue::isPending() const
{
    switch (classType()) {
    case CrossfadeClass:
        return static_cast<const CSSCrossfadeValue*>(this)->isPending();
    case CanvasClass:
        return static_cast<const CSSCanvasValue*>(this)->isPending();
    case LinearGradientClass:
        return static_cast<const CSSLinearGradientValue*>(this)->isPending();
    case RadialGradientClass:
        return static_cast<const CSSRadialGradientValue*>(this)->isPending();
    default:
        ASSERT_NOT_REACHED();
    }
    return false;
}

void CSSImageGeneratorValue::loadSubimages(CachedResourceLoader* cachedResourceLoader)
{
    switch (classType()) {
    case CrossfadeClass:
        static_cast<CSSCrossfadeValue*>(this)->loadSubimages(cachedResourceLoader);
        break;
    case CanvasClass:
        static_cast<CSSCanvasValue*>(this)->loadSubimages(cachedResourceLoader);
        break;
    case LinearGradientClass:
        static_cast<CSSLinearGradientValue*>(this)->loadSubimages(cachedResourceLoader);
        break;
    case RadialGradientClass:
        static_cast<CSSRadialGradientValue*>(this)->loadSubimages(cachedResourceLoader);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebCore
