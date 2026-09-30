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

RemoteSnapshot::~RemoteSnapshot()
{
    // Make sure whoever is waiting for this snapshot hears back.
    abandon();
}

bool RemoteSnapshot::setFrame(FrameIdentifier frameIdentifier, Ref<const DisplayList::DisplayList>&& displayList, SerialFunctionDispatcher& releaseDispatcher)
{
    Function<void()> completionHandler;
    {
        Locker locker(m_lock);
        if (!m_frameDisplayLists.add(frameIdentifier, std::optional { DisplayListAndReleaseDispatcher { WTF::move(displayList), releaseDispatcher } }).isNewEntry)
            return false;
        m_pendingFrames.remove(frameIdentifier);
        completionHandler = takeCompletionHandlerIfComplete();
    }
    if (completionHandler)
        completionHandler();
    return true;
}

void RemoteSnapshot::whenComplete(FrameIdentifier rootFrameIdentifier, HashMap<FrameIdentifier, WebCore::ProcessIdentifier>&& subframeProcesses, Function<void()>&& completionHandler)
{
    {
        Locker locker(m_lock);
        ASSERT(!m_completionHandler);
        m_completionHandler = WTF::move(completionHandler);
        if (!m_frameDisplayLists.contains(rootFrameIdentifier))
            m_pendingFrames.add(rootFrameIdentifier, std::nullopt);
        for (auto& [frameIdentifier, processIdentifier] : subframeProcesses) {
            if (!m_frameDisplayLists.contains(frameIdentifier))
                m_pendingFrames.add(frameIdentifier, processIdentifier);
        }
        completionHandler = takeCompletionHandlerIfComplete();
    }
    if (completionHandler)
        completionHandler();
}

void RemoteSnapshot::webProcessDidClose(WebCore::ProcessIdentifier processIdentifier)
{
    Function<void()> completionHandler;
    {
        Locker locker(m_lock);
        // These frames will never be drawn, so leave them blank.
        m_pendingFrames.removeIf([&](auto& entry) {
            return entry.value == processIdentifier;
        });
        completionHandler = takeCompletionHandlerIfComplete();
    }
    if (completionHandler)
        completionHandler();
}

void RemoteSnapshot::abandon()
{
    Function<void()> completionHandler;
    {
        Locker locker(m_lock);
        m_pendingFrames.clear();
        completionHandler = std::exchange(m_completionHandler, nullptr);
    }
    if (completionHandler)
        completionHandler();
}

Function<void()> RemoteSnapshot::takeCompletionHandlerIfComplete()
{
    if (!m_pendingFrames.isEmpty())
        return nullptr;
    return std::exchange(m_completionHandler, nullptr);
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

RemoteSnapshot::DisplayListAndReleaseDispatcher::DisplayListAndReleaseDispatcher(Ref<const WebCore::DisplayList::DisplayList>&& displayList, SerialFunctionDispatcher& dispatcher)
    : m_displayList(WTF::move(displayList))
    , m_dispatcher(dispatcher)
{
}

RemoteSnapshot::DisplayListAndReleaseDispatcher::~DisplayListAndReleaseDispatcher()
{
    if (m_displayList)
        m_dispatcher->dispatch([displayList = WTF::move(m_displayList)]() mutable { });
}

#if PLATFORM(COCOA)

std::optional<RefPtr<SharedBuffer>> RemoteSnapshot::drawToPDF(const FloatSize& size, FrameIdentifier rootIdentifier)
{
    RefPtr buffer = ImageBuffer::create(size, RenderingMode::PDFDocument, RenderingPurpose::Snapshot, 1, ColorSpace::SRGB(), PixelFormat::BGRA8);
    if (!buffer)
        return nullptr;

    auto& context = buffer->context();

    if (!applyFrame(rootIdentifier, context))
        return std::nullopt;

    return ImageBuffer::sinkIntoPDFDocument(WTF::move(buffer));
}

#endif

std::optional<ShareableBitmap::Handle> RemoteSnapshot::drawToBitmap(const FloatSize& size, FrameIdentifier rootFrameIdentifier)
{
    Ref image = WebImage::create(size, ImageOption::Shareable, ColorSpace::SRGB());
    auto* context = image->context();
    if (!context)
        return std::nullopt;

    if (!applyFrame(rootFrameIdentifier, *context))
        return std::nullopt;

    return image->createHandle(SharedMemory::Protection::ReadOnly);
}

}

#endif
