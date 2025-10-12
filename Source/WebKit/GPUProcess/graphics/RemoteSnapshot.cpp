/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)
#include "RemoteSnapshot.h"

#include "WebImage.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSnapshot);

Ref<RemoteSnapshot> RemoteSnapshot::create()
{
    return adoptRef(*new RemoteSnapshot);
}

RemoteSnapshot::RemoteSnapshot() = default;

RemoteSnapshot::~RemoteSnapshot() = default;

bool RemoteSnapshot::addFrameReference(FrameIdentifier frameIdentifier)
{
    Locker locker(m_lock);
    m_referencedFrames++;
    auto result = m_frameDisplayLists.add(frameIdentifier, std::nullopt);
    if (result.isNewEntry)
        return true;
    // It is ok to setFrame win the race. It is not ok to have two addFrameReferences.
    return !result.iterator->value;
}

bool RemoteSnapshot::setFrame(FrameIdentifier frameIdentifier, Ref<const DisplayList::DisplayList>&& displayList, SerialFunctionDispatcher& releaseDispatcher)
{
    Locker locker(m_lock);
    m_completedFrames++;
    auto iterator = m_frameDisplayLists.find(frameIdentifier);
    if (iterator == m_frameDisplayLists.end()) {
        m_frameDisplayLists.add(frameIdentifier, DisplayListAndReleaseDispatcher { WTFMove(displayList), releaseDispatcher });
        return true;
    }
    // It is ok to addFrameReference to win the race. It's not ok to have two setFrames.
    if (iterator->value)
        return false;
    iterator->value = DisplayListAndReleaseDispatcher { WTFMove(displayList), releaseDispatcher };
    return true;
}

bool RemoteSnapshot::applyFrame(FrameIdentifier frameIdentifier, GraphicsContext& context) const
{
    RefPtr<const DisplayList::DisplayList> displayList;
    {
        Locker locker(m_lock);
        auto iterator = m_frameDisplayLists.find(frameIdentifier);
        if (iterator != m_frameDisplayLists.end())
            displayList = iterator->value->displayList();
    }
    if (!displayList)
        return false;
    context.drawDisplayList(*displayList);
    return true;
}

bool RemoteSnapshot::isComplete() const
{
    Locker locker(m_lock);
    return m_completedFrames == m_referencedFrames; // Duplicates are handled when the values are updated.
}

RemoteSnapshot::DisplayListAndReleaseDispatcher::DisplayListAndReleaseDispatcher(Ref<const WebCore::DisplayList::DisplayList>&& displayList, SerialFunctionDispatcher& dispatcher)
    : m_displayList(WTFMove(displayList))
    , m_dispatcher(dispatcher)
{
}

RemoteSnapshot::DisplayListAndReleaseDispatcher::~DisplayListAndReleaseDispatcher()
{
    if (m_displayList)
        m_dispatcher->dispatch([displayList = WTFMove(m_displayList)]() mutable { });
}

#if PLATFORM(COCOA)

std::optional<RefPtr<SharedBuffer>> RemoteSnapshot::drawToPDF(const FloatSize& size, FrameIdentifier rootIdentifier)
{
    ASSERT(isComplete());
    RefPtr buffer = ImageBuffer::create(size, RenderingMode::PDFDocument, RenderingPurpose::Snapshot, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();

    if (!applyFrame(rootIdentifier, context))
        return std::nullopt;

    return ImageBuffer::sinkIntoPDFDocument(WTFMove(buffer));
}

#endif

std::optional<ShareableBitmap::Handle> RemoteSnapshot::drawToBitmap(const FloatSize& size, FrameIdentifier rootFrameIdentifier)
{
    ASSERT(isComplete());
    RefPtr image = WebImage::create(size, ImageOption::Shareable, DestinationColorSpace::SRGB());
    if (!image)
        return std::nullopt;

    auto& context = *image->context();

    if (!applyFrame(rootFrameIdentifier, context))
        return std::nullopt;

    return image->createHandle(SharedMemory::Protection::ReadOnly);
}

}

#endif
