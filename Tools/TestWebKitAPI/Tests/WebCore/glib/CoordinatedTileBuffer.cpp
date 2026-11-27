/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include "config.h"

#include "Helpers/Test.h"
#include <WebCore/CoordinatedTileBuffer.h>
#include <wtf/MainThread.h>
#include <wtf/WorkQueue.h>

namespace TestWebKitAPI {

using namespace WTF;
using namespace WebCore;

TEST(CoordinatedTileBuffer, Create)
{
    // Opaque buffer.
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 25, 50 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->size().width(), 25);
    EXPECT_EQ(buffer->size().height(), 50);
    EXPECT_FALSE(buffer->supportsAlpha());
    EXPECT_EQ(buffer->pixelFormat(), PixelFormat::BGRA8);
    EXPECT_FALSE(buffer->isBackedByOpenGL());
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Pending);

    // Transparent buffer.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 50, 25 }, CoordinatedTileBuffer::SupportsAlpha, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->size().width(), 50);
    EXPECT_EQ(buffer->size().height(), 25);
    EXPECT_TRUE(buffer->supportsAlpha());
    EXPECT_EQ(buffer->pixelFormat(), PixelFormat::BGRA8);
    EXPECT_FALSE(buffer->isBackedByOpenGL());
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Pending);
}

TEST(CoordinatedTileBuffer, Paint)
{
    // Visible tile.
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Pre-rendered tile.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 1, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 1u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
}

TEST(CoordinatedTileBuffer, Cancel)
{
    // Cancel visible tile on Pending state.
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->cancelPainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Cancelled);

    // Cancel pre-rendered tile on Pending state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->cancelPainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Cancelled);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Cancelled);

    // Cancel visible tile on Painting state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->cancelPainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Cancel pre-rendered tile on Painting state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->cancelPainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Cancel visible tile on Complete state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
    EXPECT_EQ(buffer->cancelPainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Cancel pre-rendered tile on Complete state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 1, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
    EXPECT_EQ(buffer->cancelPainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
}

TEST(CoordinatedTileBuffer, Failed)
{
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ std::numeric_limits<int>::max(), std::numeric_limits<int>::max() }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Failed);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    auto data = static_cast<CoordinatedUnacceleratedTileBuffer*>(buffer.ptr())->takeData();
    EXPECT_TRUE(data.isEmpty());
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    buffer = CoordinatedUnacceleratedTileBuffer::create({ 0, 0 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Failed);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    data = static_cast<CoordinatedUnacceleratedTileBuffer*>(buffer.ptr())->takeData();
    EXPECT_TRUE(data.isEmpty());
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
}

TEST(CoordinatedTileBuffer, UpdateDistance)
{
    // Demote: Visible -> PreRendered.
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->updatePriority(2, CoordinatedTileBuffer::IsInScrollDirection::No, 1), 1u);
    EXPECT_EQ(buffer->distance(), 2u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Promote: PreRendered -> Visible.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 2u);
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::No, 1), 1u);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // No change: Visible -> Visible.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // No change: PreRendered -> PreRendered.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 2u);
    EXPECT_EQ(buffer->updatePriority(3, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    EXPECT_EQ(buffer->distance(), 3u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Promote on Painting state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 2u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::No, 1), 1u);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // Demote on Painting state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Painting);
    EXPECT_EQ(buffer->updatePriority(2, CoordinatedTileBuffer::IsInScrollDirection::No, 1), 1u);
    EXPECT_EQ(buffer->distance(), 2u);
    EXPECT_EQ(buffer->completePainting(), std::nullopt);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);

    // UpdateDistance on Complete state.
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    EXPECT_EQ(buffer->distance(), 0u);

    // UpdateDistance on Cancel state.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    EXPECT_EQ(buffer->distance(), 0u);
    EXPECT_EQ(buffer->cancelPainting(), 1u);
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Cancelled);
    EXPECT_EQ(buffer->updatePriority(2, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    EXPECT_EQ(buffer->distance(), 2u);
}

TEST(CoordinatedTileBuffer, Priority)
{
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 2, CoordinatedTileBuffer::IsInScrollDirection::Yes, 1);
    auto priority = buffer->priority();
    EXPECT_EQ(priority.distance, 2u);
    EXPECT_EQ(priority.isInScrollDirection, CoordinatedTileBuffer::IsInScrollDirection::Yes);

    // Changing only the scroll direction of a hidden tile doesn't change the rendering update ID.
    EXPECT_EQ(buffer->updatePriority(2, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    priority = buffer->priority();
    EXPECT_EQ(priority.distance, 2u);
    EXPECT_EQ(priority.isInScrollDirection, CoordinatedTileBuffer::IsInScrollDirection::No);

    // Promote to visible while in scroll direction: visibility transition still reported.
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::Yes, 1), 1u);
    priority = buffer->priority();
    EXPECT_EQ(priority.distance, 0u);
    EXPECT_EQ(priority.isInScrollDirection, CoordinatedTileBuffer::IsInScrollDirection::Yes);

    // Visible -> visible with a different scroll direction: no change reported.
    EXPECT_EQ(buffer->updatePriority(0, CoordinatedTileBuffer::IsInScrollDirection::No, 1), std::nullopt);
    EXPECT_EQ(buffer->priority().isInScrollDirection, CoordinatedTileBuffer::IsInScrollDirection::No);

    // Priority is still updated on Complete state.
    EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
    EXPECT_EQ(buffer->completePainting(), 1u);
    EXPECT_EQ(buffer->updatePriority(3, CoordinatedTileBuffer::IsInScrollDirection::Yes, 1), std::nullopt);
    priority = buffer->priority();
    EXPECT_EQ(priority.distance, 3u);
    EXPECT_EQ(priority.isInScrollDirection, CoordinatedTileBuffer::IsInScrollDirection::Yes);
}

TEST(CoordinatedTileBuffer, WaitPaintingFinished)
{
    WTF::initializeMainThread();

    Ref workQueue = WorkQueue::create("CoordinatedTileBuffer Test Work Queue"_s);

    // Wait until complete.
    Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    workQueue->dispatch([buffer = buffer.copyRef()] mutable {
        EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
        EXPECT_EQ(buffer->completePainting(), 1u);
        EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
    });
    buffer->waitUntilPaintingFinished();
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
    workQueue->dispatchSync([] { });

    // Wait until cancel.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    workQueue->dispatch([buffer = buffer.copyRef()] mutable {
        EXPECT_EQ(buffer->cancelPainting(), 1u);
        EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Cancelled);
    });
    buffer->waitUntilPaintingFinished();
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Cancelled);
    workQueue->dispatchSync([] { });

    // Don't wait for pre-rendered tiles.
    buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 1, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
    buffer->waitUntilPaintingFinished();
    EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Pending);
}

TEST(CoordinatedTileBuffer, MemoryUsage)
{
    CoordinatedTileBuffer::resetMemoryUsage();
    EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);

    auto bufferSizeInBytes = [](const Ref<CoordinatedTileBuffer>& buffer) -> size_t {
        return static_cast<size_t>(buffer->size().width() * buffer->size().height() * 4);
    };

    {
        Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);
        EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), bufferSizeInBytes(buffer));
        auto data = static_cast<CoordinatedUnacceleratedTileBuffer*>(buffer.ptr())->takeData();
        EXPECT_EQ(data.size(), bufferSizeInBytes(buffer));
        // First call return the maximum size, second the current one.
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), bufferSizeInBytes(buffer));
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);
        EXPECT_EQ(buffer->completePainting(), 1u);
        EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
    }
    EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);

    {
        Ref buffer = CoordinatedUnacceleratedTileBuffer::create({ 16, 16 }, CoordinatedTileBuffer::NoFlags, 0, CoordinatedTileBuffer::IsInScrollDirection::No, 1);
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);
        EXPECT_EQ(buffer->beginPainting(), CoordinatedTileBuffer::BeginPaintingResult::Started);
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), bufferSizeInBytes(buffer));
        EXPECT_EQ(buffer->completePainting(), 1u);
        EXPECT_EQ(buffer->paintingState(), CoordinatedTileBuffer::PaintingState::Complete);
        EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), bufferSizeInBytes(buffer));
    }
    // First call return the maximum size, second the current one.
    EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 1024);
    EXPECT_EQ(CoordinatedTileBuffer::getMemoryUsage(), 0);
}

} // namespace TestWebKitAPI
