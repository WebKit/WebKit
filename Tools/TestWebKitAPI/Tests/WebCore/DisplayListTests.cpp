/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <WebCore/DisplayList.h>
#include <WebCore/DisplayListItems.h>
#include <WebCore/Gradient.h>

namespace TestWebKitAPI {
using namespace WebCore;
using DisplayList::DisplayList;
using namespace DisplayList;

constexpr size_t globalItemBufferCapacity = 1 << 12;
static uint8_t globalItemBuffer[globalItemBufferCapacity];

static Ref<Gradient> createGradient()
{
    auto gradient = Gradient::create(Gradient::ConicData { { 0., 0. }, 1.25 });
    gradient->addColorStop({ 0.1, Color::red });
    gradient->addColorStop({ 0.5, Color::green });
    gradient->addColorStop({ 0.9, Color::blue });
    return gradient;
}

static Path createComplexPath()
{
    Path path;
    path.moveTo({ 10., 10. });
    path.addLineTo({ 50., 50. });
    path.addQuadCurveTo({ 100., 100. }, { 0., 200. });
    path.addLineTo({ 10., 10. });
    return path;
}

TEST(DisplayListTests, AppendItems)
{
    DisplayList list;

    EXPECT_TRUE(list.isEmpty());

    auto gradient = createGradient();
    auto path = createComplexPath();

    for (int i = 0; i < 50; ++i) {
        list.append<SetStrokeThickness>(1.5);
        list.append<FillPath>(path);
        list.append<FillRectWithGradient>(FloatRect { 1., 1., 10., 10. }, gradient);
        list.append<SetInlineFillColor>(Color::red);
#if ENABLE(INLINE_PATH_DATA)
        list.append<StrokeInlinePath>(InlinePathData { LineData {{ 0., 0. }, { 10., 15. }}});
#endif
    }

    EXPECT_FALSE(list.isEmpty());

    bool observedUnexpectedItem = false;
    for (auto [handle, extent, size] : list) {
        switch (handle->type()) {
        case ItemType::SetStrokeThickness: {
            EXPECT_FALSE(handle->isDrawingItem());
            EXPECT_TRUE(handle->is<SetStrokeThickness>());
            auto& item = handle->get<SetStrokeThickness>();
            EXPECT_EQ(item.thickness(), 1.5);
            break;
        }
        case ItemType::FillPath: {
            EXPECT_TRUE(handle->isDrawingItem());
            EXPECT_TRUE(handle->is<FillPath>());
            auto& item = handle->get<FillPath>();
            EXPECT_EQ(item.path().platformPath(), path.platformPath());
            break;
        }
        case ItemType::FillRectWithGradient: {
            EXPECT_TRUE(handle->isDrawingItem());
            EXPECT_TRUE(handle->is<FillRectWithGradient>());
            auto& item = handle->get<FillRectWithGradient>();
            EXPECT_EQ(item.rect(), FloatRect(1., 1., 10., 10.));
            EXPECT_EQ(&item.gradient(), gradient.ptr());
            break;
        }
        case ItemType::SetInlineFillColor: {
            EXPECT_FALSE(handle->isDrawingItem());
            EXPECT_TRUE(handle->is<SetInlineFillColor>());
            auto& item = handle->get<SetInlineFillColor>();
            EXPECT_EQ(item.color(), Color::red);
            break;
        }
#if ENABLE(INLINE_PATH_DATA)
        case ItemType::StrokeInlinePath: {
            EXPECT_TRUE(handle->isDrawingItem());
            EXPECT_TRUE(handle->is<StrokeInlinePath>());
            auto& item = handle->get<StrokeInlinePath>();
            const auto path = item.path();
            auto& line = path.inlineData<LineData>();
            EXPECT_EQ(line.start, FloatPoint(0, 0));
            EXPECT_EQ(line.end, FloatPoint(10., 15.));
            break;
        }
#endif
        case ItemType::MetaCommandChangeItemBuffer:
            break;
        default: {
            observedUnexpectedItem = true;
            break;
        }
        }
    }

    EXPECT_FALSE(observedUnexpectedItem);
    EXPECT_GT(list.sizeInBytes(), 0U);

    list.clear();

    observedUnexpectedItem = false;
    for (auto itemAndExtent : list) {
        UNUSED_PARAM(itemAndExtent);
        observedUnexpectedItem = true;
    }

    EXPECT_TRUE(list.isEmpty());
    EXPECT_EQ(list.sizeInBytes(), 0U);
    EXPECT_FALSE(observedUnexpectedItem);

    list.append<FillRectWithColor>(FloatRect { 0, 0, 100, 100 }, Color::black);

    for (auto [handle, extent, size] : list) {
        EXPECT_EQ(handle->type(), ItemType::FillRectWithColor);
        EXPECT_TRUE(handle->is<FillRectWithColor>());

        auto& item = handle->get<FillRectWithColor>();
        EXPECT_EQ(item.color().asInline(), Color::black);
        EXPECT_EQ(item.rect(), FloatRect(0, 0, 100, 100));
    }

    EXPECT_FALSE(list.isEmpty());
    EXPECT_GT(list.sizeInBytes(), 0U);
}

TEST(DisplayListTests, ItemBufferClient)
{
    Vector<StrokePath> strokePathItems;
    static ItemBufferIdentifier globalBufferIdentifier = ItemBufferIdentifier::generate();

    class StrokePathReader : public ItemBufferReadingClient {
    public:
        StrokePathReader(const Vector<StrokePath>& items)
            : m_items(items)
        {
        }

    private:
        Optional<ItemHandle> WARN_UNUSED_RETURN decodeItem(const uint8_t* data, size_t dataLength, ItemType type, uint8_t* handleLocation) final
        {
            EXPECT_EQ(type, ItemType::StrokePath);
            EXPECT_EQ(dataLength, sizeof(size_t));
            new (handleLocation + sizeof(uint64_t)) StrokePath(m_items[*reinterpret_cast<const size_t*>(data)]);
            return {{ handleLocation }};
        }

        const Vector<StrokePath>& m_items;
    };

    class StrokePathWriter : public ItemBufferWritingClient {
    public:
        StrokePathWriter(Vector<StrokePath>& items)
            : m_items(items)
        {
        }

    private:
        ItemBufferHandle createItemBuffer(size_t capacity) final
        {
            EXPECT_LT(capacity, globalItemBufferCapacity);
            return { globalBufferIdentifier, globalItemBuffer, globalItemBufferCapacity };
        }

        RefPtr<SharedBuffer> encodeItem(ItemHandle handle) const final
        {
            auto index = m_items.size();
            m_items.append(handle.get<StrokePath>());
            return SharedBuffer::create(reinterpret_cast<uint8_t*>(&index), sizeof(size_t));
        }

        void didAppendData(const ItemBufferHandle&, size_t, DidChangeItemBuffer) final { }

        Vector<StrokePath>& m_items;
    };

    DisplayList list;
    StrokePathWriter writer { strokePathItems };
    list.setItemBufferClient(&writer);

    auto path = createComplexPath();
    list.append<SetInlineStrokeColor>(Color::blue);
    list.append<StrokePath>(path);
    list.append<SetInlineStrokeColor>(Color::red);
    list.append<StrokePath>(path);

    DisplayList shallowCopy {{ ItemBufferHandle { globalBufferIdentifier, globalItemBuffer, list.sizeInBytes() } }};
    StrokePathReader reader { strokePathItems };
    shallowCopy.setItemBufferClient(&reader);

    Vector<ItemType> itemTypes;
    for (auto [handle, extent, size] : shallowCopy)
        itemTypes.append(handle->type());

    EXPECT_FALSE(shallowCopy.isEmpty());
    EXPECT_EQ(itemTypes.size(), 4U);
    EXPECT_EQ(itemTypes[0], ItemType::SetInlineStrokeColor);
    EXPECT_EQ(itemTypes[1], ItemType::StrokePath);
    EXPECT_EQ(itemTypes[2], ItemType::SetInlineStrokeColor);
    EXPECT_EQ(itemTypes[3], ItemType::StrokePath);
}

} // namespace TestWebKitAPI
