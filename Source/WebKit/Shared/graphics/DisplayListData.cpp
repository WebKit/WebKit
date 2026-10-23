/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DisplayListData.h"

#if ENABLE(GPU_PROCESS)

#include <WebCore/DisplayListRecorderImpl.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebKit {
using namespace WebCore;

namespace {

class DisplayListDataEncoder {
public:
    std::optional<DisplayListData> encode(const DisplayList::DisplayList& displayList)
    {
        if (!encodeDisplayList(displayList))
            return std::nullopt;
        return DisplayListData { WTF::move(m_displayLists) };
    }

private:
    std::optional<uint64_t> encodeDisplayList(const DisplayList::DisplayList& displayList)
    {
        if (auto iterator = m_indices.find(&displayList); iterator != m_indices.end())
            return iterator->value;

        // A placeholder can draw anything, including a display list that draws it.
        if (!m_displayListsBeingEncoded.add(&displayList).isNewEntry)
            return std::nullopt;

        Vector<DisplayListDataItem> items;
        bool success = appendItems(displayList.items(), items);
        m_displayListsBeingEncoded.remove(&displayList);
        if (!success)
            return std::nullopt;

        m_displayLists.append(WTF::move(items));
        uint64_t index = m_displayLists.size() - 1;
        m_indices.add(&displayList, index);
        return index;
    }

    bool appendItems(std::span<const DisplayList::Item> items, Vector<DisplayListDataItem>& result)
    {
        for (auto& item : items) {
            if (!appendItem(item, result))
                return false;
        }
        return true;
    }

    bool appendItem(const DisplayList::Item& item, Vector<DisplayListDataItem>& result)
    {
        return WTF::switchOn(item,
            [&](const DisplayList::DrawDisplayList& item) {
                auto index = encodeDisplayList(item.displayList());
                if (!index)
                    return false;
                result.constructAndAppend(WTF::InPlaceType<DrawDisplayListAtIndex>, *index);
                return true;
            },
            [&](const DisplayList::DrawPlaceholder& item) {
                Ref recorder = DisplayList::RecorderImpl::create(FloatRect { });
                item.apply(recorder.get());
                Ref contents = recorder->takeDisplayList();
                return appendItems(contents->items(), result);
            },
            [](const DisplayList::ClipToImageBuffer&) { return false; },
            [](const DisplayList::DrawControlPart&) { return false; },
            [](const DisplayList::DrawFilteredImageBuffer&) { return false; },
            [](const DisplayList::DrawGlyphs&) { return false; },
            [](const DisplayList::DrawImageBuffer&) { return false; },
            [](const DisplayList::DrawNativeImage&) { return false; },
            [](const DisplayList::DrawPatternNativeImage&) { return false; },
            [](const DisplayList::DrawPatternImageBuffer&) { return false; },
            [](const DisplayList::DrawSystemImage&) { return false; },
            [](const DisplayList::FillRectWithGradient&) { return false; },
            [](const DisplayList::FillRectWithGradientAndSpaceTransform&) { return false; },
            [](const DisplayList::SetState&) { return false; },
#if USE(SKIA)
            [](const DisplayList::DrawTextBlob&) { return false; },
#endif
            [&](const auto& item) {
                result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                return true;
            });
    }

    Vector<Vector<DisplayListDataItem>> m_displayLists;
    HashMap<RefPtr<const DisplayList::DisplayList>, uint64_t> m_indices;
    HashSet<RefPtr<const DisplayList::DisplayList>> m_displayListsBeingEncoded;
};

} // namespace

std::optional<DisplayListData> DisplayListData::create(const DisplayList::DisplayList& displayList)
{
    return DisplayListDataEncoder { }.encode(displayList);
}

DisplayListData::DisplayListData(Vector<Vector<DisplayListDataItem>>&& displayLists)
    : m_displayLists(WTF::move(displayLists))
{
}

RefPtr<const DisplayList::DisplayList> DisplayListData::createDisplayList() const
{
    Vector<Ref<const DisplayList::DisplayList>> displayLists;
    displayLists.reserveInitialCapacity(m_displayLists.size());
    for (auto& items : m_displayLists) {
        Vector<DisplayList::Item> result;
        result.reserveInitialCapacity(items.size());
        for (auto& item : items) {
            bool isValid = WTF::switchOn(item,
                [&](const DrawDisplayListAtIndex& item) {
                    if (item.index >= displayLists.size())
                        return false;
                    result.constructAndAppend(WTF::InPlaceType<DisplayList::DrawDisplayList>, displayLists[item.index].copyRef());
                    return true;
                },
                [&](const auto& item) {
                    result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                    return true;
                });
            if (!isValid)
                return nullptr;
        }
        displayLists.append(DisplayList::DisplayList::create(WTF::move(result)));
    }
    if (displayLists.isEmpty())
        return nullptr;
    return displayLists.takeLast();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
