/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "FloatRect.h"
#include "GraphicsContext.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

namespace DisplayList {

enum class ItemType : uint8_t {
    Save,
    Restore,
    Translate,
    Rotate,
    Scale,
    ConcatenateCTM,
    SetCTM,
    SetState,
    SetLineCap,
    SetLineDash,
    SetLineJoin,
    SetMiterLimit,
    ClearShadow,
    Clip,
    ClipOut,
    ClipOutToPath,
    ClipPath,
    ClipToDrawingCommands,
    DrawGlyphs,
    DrawImage,
    DrawTiledImage,
    DrawTiledScaledImage,
#if USE(CG) || USE(CAIRO) || USE(DIRECT2D)
    DrawNativeImage,
#endif
    DrawPattern,
    DrawRect,
    DrawLine,
    DrawLinesForText,
    DrawDotsForDocumentMarker,
    DrawEllipse,
    DrawPath,
    DrawFocusRingPath,
    DrawFocusRingRects,
    FillRect,
    FillRectWithColor,
    FillRectWithGradient,
    FillCompositedRect,
    FillRoundedRect,
    FillRectWithRoundedHole,
    FillPath,
    FillEllipse,
    PutImageData,
    StrokeRect,
    StrokePath,
    StrokeEllipse,
    ClearRect,
    BeginTransparencyLayer,
    EndTransparencyLayer,
#if USE(CG)
    ApplyStrokePattern, // FIXME: should not be a recorded item.
    ApplyFillPattern, // FIXME: should not be a recorded item.
#endif
    ApplyDeviceScaleFactor,
};

class Item : public RefCounted<Item> {
public:
    Item() = delete;

    WEBCORE_EXPORT Item(ItemType);
    WEBCORE_EXPORT virtual ~Item();

    ItemType type() const
    {
        return m_type;
    }

    virtual void apply(GraphicsContext&) const = 0;

    static constexpr bool isDisplayListItem = true;

    virtual bool isDrawingItem() const { return false; }

    // A state item is one preserved by Save/Restore.
    bool isStateItem() const
    {
        return isStateItemType(m_type);
    }

    static bool isStateItemType(ItemType itemType)
    {
        switch (itemType) {
        case ItemType::Translate:
        case ItemType::Rotate:
        case ItemType::Scale:
        case ItemType::ConcatenateCTM:
        case ItemType::SetCTM:
        case ItemType::SetState:
        case ItemType::SetLineCap:
        case ItemType::SetLineDash:
        case ItemType::SetLineJoin:
        case ItemType::SetMiterLimit:
        case ItemType::ClearShadow:
            return true;
        default:
            return false;
        }
        return false;
    }

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
#endif
    static size_t sizeInBytes(const Item&);

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Item>> decode(Decoder&);

private:
    ItemType m_type;
};

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
    DisplayList() = default;
    DisplayList(DisplayList&&) = default;

    DisplayList& operator=(DisplayList&&) = default;

    void dump(WTF::TextStream&) const;

    const Vector<Ref<Item>>& list() const { return m_list; }
    Item& itemAt(size_t index)
    {
        ASSERT(index < m_list.size());
        return m_list[index].get();
    }

    WEBCORE_EXPORT void clear();

    size_t itemCount() const { return m_list.size(); }
    size_t sizeInBytes() const;
    
    String asText(AsTextFlags) const;

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
    WEBCORE_EXPORT void dump() const;
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<DisplayList> decode(Decoder&);


private:
    Item& append(Ref<Item>&& item)
    {
        m_list.append(WTFMove(item));
        return m_list.last().get();
    }

    // Less efficient append, only used for tracking replay.
    void appendItem(Item& item)
    {
        m_list.append(item);
    }

    static bool shouldDumpForFlags(AsTextFlags, const Item&);

    Vector<Ref<Item>>& list() { return m_list; }

    Vector<Ref<Item>> m_list;
};


template<class Encoder>
void DisplayList::encode(Encoder& encoder) const
{
    encoder << static_cast<uint64_t>(m_list.size());

    for (auto& item : m_list)
        encoder << item.get();
}

template<class Decoder>
Optional<DisplayList> DisplayList::decode(Decoder& decoder)
{
    Optional<uint64_t> itemCount;
    decoder >> itemCount;
    if (!itemCount)
        return WTF::nullopt;

    DisplayList displayList;

    for (uint64_t i = 0; i < *itemCount; i++) {
        auto item = Item::decode(decoder);
        // FIXME: Once we can decode all types, failing to decode an item should turn into a decode failure.
        // For now, we just have to ignore it.
        if (!item)
            continue;
        displayList.append(WTFMove(*item));
    }

    return displayList;
}

} // DisplayList

WTF::TextStream& operator<<(WTF::TextStream&, const DisplayList::DisplayList&);

} // WebCore
