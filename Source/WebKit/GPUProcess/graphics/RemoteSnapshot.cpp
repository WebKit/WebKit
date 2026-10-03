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
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSnapshot);

Ref<RemoteSnapshot> RemoteSnapshot::create(std::optional<FrameIdentifier> rootFrameIdentifier)
{
    return adoptRef(*new RemoteSnapshot(rootFrameIdentifier));
}

RemoteSnapshot::RemoteSnapshot(std::optional<FrameIdentifier> rootFrameIdentifier)
    : m_rootFrameIdentifier(rootFrameIdentifier)
{
    if (!rootFrameIdentifier)
        return;
    Locker locker(m_lock);
    m_frames.add(*rootFrameIdentifier, Frame { });
    m_unresolvedFrames = 1;
}

RemoteSnapshot::~RemoteSnapshot() = default;

bool RemoteSnapshot::addFrameReference(FrameIdentifier frameIdentifier)
{
    Locker locker(m_lock);
    auto result = m_frames.add(frameIdentifier, Frame { });
    if (result.isNewEntry) {
        m_unresolvedFrames++;
        return true;
    }
    // It is ok to setFrame or abandonFrame to win the race. It is not ok to have two addFrameReferences.
    return result.iterator->value.isResolved();
}

void RemoteSnapshot::resolveFrameWithLockHeld(Frame& frame)
{
    ASSERT(frame.isResolved());
    ASSERT(m_unresolvedFrames);
    m_unresolvedFrames--;
    dispatchCompletionHandlersIfComplete();
}

bool RemoteSnapshot::setFrame(FrameIdentifier frameIdentifier, Ref<const DisplayList::DisplayList>&& displayList, SerialFunctionDispatcher& releaseDispatcher)
{
    Locker locker(m_lock);
    auto result = m_frames.add(frameIdentifier, Frame { });
    auto& frame = result.iterator->value;
    if (result.isNewEntry) {
        // Came in before it was referenced, so it was never counted as unresolved.
        frame.displayList = DisplayListAndReleaseDispatcher { WTF::move(displayList), releaseDispatcher };
        return true;
    }
    // Abandoned because the process recording it went away after recording it. Either outcome is fine.
    if (frame.isAbandoned)
        return true;
    // It is ok to addFrameReference to win the race. It's not ok to have two setFrames.
    if (frame.displayList)
        return false;
    frame.displayList = DisplayListAndReleaseDispatcher { WTF::move(displayList), releaseDispatcher };
    resolveFrameWithLockHeld(frame);
    return true;
}

void RemoteSnapshot::abandonFrame(FrameIdentifier frameIdentifier)
{
    Locker locker(m_lock);
    auto result = m_frames.add(frameIdentifier, Frame { .displayList = std::nullopt, .isAbandoned = true });
    if (result.isNewEntry)
        return;
    auto& frame = result.iterator->value;
    if (frame.isResolved())
        return;
    frame.isAbandoned = true;
    resolveFrameWithLockHeld(frame);
}

void RemoteSnapshot::setFrameOwner(FrameIdentifier frameIdentifier, ProcessIdentifier processIdentifier)
{
    Locker locker(m_lock);
    m_frameOwners.set(frameIdentifier, processIdentifier);
}

void RemoteSnapshot::abandonFramesOwnedBy(ProcessIdentifier processIdentifier)
{
    Vector<FrameIdentifier> frames;
    {
        Locker locker(m_lock);
        m_frameOwners.removeIf([&](auto& entry) {
            if (entry.value != processIdentifier)
                return false;
            frames.append(entry.key);
            return true;
        });
    }
    for (auto frameIdentifier : frames)
        abandonFrame(frameIdentifier);
}

void RemoteSnapshot::abandonUnresolvedFrames()
{
    Vector<FrameIdentifier> frames;
    {
        Locker locker(m_lock);
        for (auto& [frameIdentifier, frame] : m_frames) {
            if (!frame.isResolved())
                frames.append(frameIdentifier);
        }
    }
    for (auto frameIdentifier : frames)
        abandonFrame(frameIdentifier);
}

FloatSize RemoteSnapshot::size() const
{
    Locker locker(m_lock);
    return m_size;
}

void RemoteSnapshot::setSize(const FloatSize& size)
{
    Locker locker(m_lock);
    m_size = size;
}

bool RemoteSnapshot::hasFailed() const
{
    Locker locker(m_lock);
    return hasFailedWithLockHeld();
}

bool RemoteSnapshot::hasFailedWithLockHeld() const
{
    if (m_hasFailed)
        return true;
    if (!m_rootFrameIdentifier)
        return false;
    auto iterator = m_frames.find(*m_rootFrameIdentifier);
    return iterator == m_frames.end() || iterator->value.isAbandoned;
}

void RemoteSnapshot::fail()
{
    Locker locker(m_lock);
    m_hasFailed = true;
    dispatchCompletionHandlersIfComplete();
}

bool RemoteSnapshot::isComplete() const
{
    Locker locker(m_lock);
    return isCompleteWithLockHeld();
}

void RemoteSnapshot::whenComplete(CompletionHandler<void(bool)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());
    Locker locker(m_lock);
    m_completionHandlers.append(WTF::move(completionHandler));
    dispatchCompletionHandlersIfComplete();
}

bool RemoteSnapshot::isAwaited() const
{
    Locker locker(m_lock);
    return !m_completionHandlers.isEmpty();
}

void RemoteSnapshot::dispatchCompletionHandlersIfComplete()
{
    if (!isCompleteWithLockHeld() || m_completionHandlers.isEmpty())
        return;
    // Frames are resolved on the rendering backends' work queues. Dispatching even from the main thread keeps
    // the handlers in the order they were added.
    RunLoop::mainSingleton().dispatch([completionHandlers = std::exchange(m_completionHandlers, { }), success = !hasFailedWithLockHeld()] mutable {
        for (auto& completionHandler : completionHandlers)
            completionHandler(success);
    });
}

bool RemoteSnapshot::applyFrame(FrameIdentifier frameIdentifier, GraphicsContext& context) const
{
    RefPtr<const DisplayList::DisplayList> displayList;
    {
        Locker locker(m_lock);
        auto iterator = m_frames.find(frameIdentifier);
        if (iterator == m_frames.end())
            return false;
        if (iterator->value.isAbandoned)
            return true;
        if (iterator->value.displayList)
            displayList = iterator->value.displayList->displayList();
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
    ASSERT(isComplete());
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
    ASSERT(isComplete());
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
