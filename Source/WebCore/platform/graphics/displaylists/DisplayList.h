/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "DecomposedGlyphs.h"
#include "DisplayListItemBuffer.h"
#include "DisplayListItemType.h"
#include "DisplayListResourceHeap.h"
#include "Filter.h"
#include "FloatRect.h"
#include "Font.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include <wtf/FastMalloc.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

namespace DisplayList {

class DisplayList {
    WTF_MAKE_NONCOPYABLE(DisplayList); WTF_MAKE_FAST_ALLOCATED;
    friend class RecorderImpl;
    friend class Replayer;
public:
    WEBCORE_EXPORT DisplayList();
    WEBCORE_EXPORT DisplayList(DisplayList&&);
    WEBCORE_EXPORT DisplayList(ItemBufferHandles&&);

    WEBCORE_EXPORT ~DisplayList();

    WEBCORE_EXPORT DisplayList& operator=(DisplayList&&);

    void dump(WTF::TextStream&) const;

    WEBCORE_EXPORT void clear();
    WEBCORE_EXPORT bool isEmpty() const;
    WEBCORE_EXPORT size_t sizeInBytes() const;

    WEBCORE_EXPORT String asText(OptionSet<AsTextFlag>) const;

    const ResourceHeap& resourceHeap() const { return m_resourceHeap; }

    WEBCORE_EXPORT void setItemBufferReadingClient(ItemBufferReadingClient*);
    WEBCORE_EXPORT void setItemBufferWritingClient(ItemBufferWritingClient*);
    WEBCORE_EXPORT void prepareToAppend(ItemBufferHandle&&);

    void shrinkToFit();

#if !defined(NDEBUG) || !LOG_DISABLED
    CString description() const;
    WEBCORE_EXPORT void dump() const;
#endif

    WEBCORE_EXPORT void forEachItemBuffer(Function<void(const ItemBufferHandle&)>&&) const;

    template<typename T, class... Args> void append(Args&&... args);
    void append(ItemHandle);

    class Iterator;

    WEBCORE_EXPORT Iterator begin() const;
    WEBCORE_EXPORT Iterator end() const;

private:
    ItemBuffer* itemBufferIfExists() const { return m_items.get(); }
    WEBCORE_EXPORT ItemBuffer& itemBuffer();

    void cacheImageBuffer(WebCore::ImageBuffer& imageBuffer)
    {
        m_resourceHeap.add(imageBuffer.renderingResourceIdentifier(), Ref { imageBuffer });
    }

    void cacheNativeImage(NativeImage& image)
    {
        m_resourceHeap.add(image.renderingResourceIdentifier(), Ref { image });
    }

    void cacheFont(Font& font)
    {
        m_resourceHeap.add(font.renderingResourceIdentifier(), Ref { font });
    }

    void cacheDecomposedGlyphs(DecomposedGlyphs& decomposedGlyphs)
    {
        m_resourceHeap.add(decomposedGlyphs.renderingResourceIdentifier(), Ref { decomposedGlyphs });
    }

    void cacheGradient(Gradient& gradient)
    {
        m_resourceHeap.add(gradient.renderingResourceIdentifier(), Ref { gradient });
    }

    void cacheFilter(Filter& filter)
    {
        m_resourceHeap.add(filter.renderingResourceIdentifier(), Ref { filter });
    }

    static bool shouldDumpForFlags(OptionSet<AsTextFlag>, ItemHandle);

    LocalResourceHeap m_resourceHeap;
    std::unique_ptr<ItemBuffer> m_items;
};

template<typename T, class... Args>
void DisplayList::append(Args&&... args)
{
    itemBuffer().append<T>(std::forward<Args>(args)...);
}

WEBCORE_EXPORT WTF::TextStream& operator<<(WTF::TextStream&, const DisplayList&);

} // DisplayList

} // WebCore
