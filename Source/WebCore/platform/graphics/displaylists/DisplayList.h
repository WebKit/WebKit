/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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

#include "DisplayListItemBuffer.h"
#include "DisplayListItemType.h"
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

enum AsTextFlag {
    None                            = 0,
    IncludesPlatformOperations      = 1 << 0,
};

typedef unsigned AsTextFlags;

class DisplayList {
    WTF_MAKE_NONCOPYABLE(DisplayList); WTF_MAKE_FAST_ALLOCATED;
    friend class Recorder;
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

    String asText(AsTextFlags) const;

    const ImageBufferHashMap& imageBuffers() const { return m_imageBuffers; }
    const NativeImageHashMap& nativeImages() const { return m_nativeImages; }
    const FontRenderingResourceMap& fonts() const { return m_fonts; }

    WEBCORE_EXPORT void setItemBufferReadingClient(ItemBufferReadingClient*);
    WEBCORE_EXPORT void setItemBufferWritingClient(ItemBufferWritingClient*);
    WEBCORE_EXPORT void prepareToAppend(ItemBufferHandle&&);

    void shrinkToFit();

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
    WEBCORE_EXPORT void dump() const;
#endif

    WEBCORE_EXPORT void forEachItemBuffer(Function<void(const ItemBufferHandle&)>&&) const;

    template<typename T, class... Args> void append(Args&&... args);
    void append(ItemHandle);

    bool tracksDrawingItemExtents() const { return m_tracksDrawingItemExtents; }
    WEBCORE_EXPORT void setTracksDrawingItemExtents(bool);

    class Iterator;

    WEBCORE_EXPORT Iterator begin() const;
    WEBCORE_EXPORT Iterator end() const;

private:
    ItemBuffer* itemBufferIfExists() const { return m_items.get(); }
    WEBCORE_EXPORT ItemBuffer& itemBuffer();

    void addDrawingItemExtent(std::optional<FloatRect>&& extent)
    {
        ASSERT(m_tracksDrawingItemExtents);
        m_drawingItemExtents.append(WTFMove(extent));
    }

    void cacheImageBuffer(WebCore::ImageBuffer& imageBuffer)
    {
        m_imageBuffers.ensure(imageBuffer.renderingResourceIdentifier(), [&]() {
            return makeRef(imageBuffer);
        });
    }

    void cacheNativeImage(NativeImage& image)
    {
        m_nativeImages.ensure(image.renderingResourceIdentifier(), [&]() {
            return makeRef(image);
        });
    }

    void cacheFont(Font& font)
    {
        m_fonts.ensure(font.renderingResourceIdentifier(), [&]() {
            return makeRef(font);
        });
    }

    static bool shouldDumpForFlags(AsTextFlags, ItemHandle);

    ImageBufferHashMap m_imageBuffers;
    NativeImageHashMap m_nativeImages;
    FontRenderingResourceMap m_fonts;
    std::unique_ptr<ItemBuffer> m_items;
    Vector<std::optional<FloatRect>> m_drawingItemExtents;
    bool m_tracksDrawingItemExtents { true };
};

template<typename T, class... Args>
void DisplayList::append(Args&&... args)
{
    itemBuffer().append<T>(std::forward<Args>(args)...);
}

WTF::TextStream& operator<<(WTF::TextStream&, const DisplayList&);

} // DisplayList

} // WebCore
