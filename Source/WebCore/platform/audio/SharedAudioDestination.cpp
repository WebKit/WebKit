/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "SharedAudioDestination.h"

#if ENABLE(WEB_AUDIO)

#include "AudioUtilities.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/WTFSemaphore.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

class SharedAudioDestinationAdapter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SharedAudioDestinationAdapter>, public AudioIOCallback {
public:
    using AudioDestinationCreationFunction = SharedAudioDestination::AudioDestinationCreationFunction;
    static Ref<SharedAudioDestinationAdapter> ensureAdapter(unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&& ensureFunction);
    ~SharedAudioDestinationAdapter();

    void addRenderer(SharedAudioDestination&, CompletionHandler<void(bool)>&&);
    void removeRenderer(SharedAudioDestination&, CompletionHandler<void(bool)>&&);

    unsigned framesPerBuffer() const
    {
        return m_workBus->length();
    }

private:
    using AdapterKey = std::tuple<unsigned, float>;
    using AdapterMap = UncheckedKeyHashMap<AdapterKey, ThreadSafeWeakPtr<SharedAudioDestinationAdapter>>;
    static AdapterMap& sharedMap();

    SharedAudioDestinationAdapter(unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&&);

    void render(AudioBus* sourceBus, AudioBus* destinationBus, size_t framesToProcess, const AudioIOPosition& outputPosition) final;
    void isPlayingDidChange() final { }

    void configureRenderThread(CompletionHandler<void(bool)>&&);

    Ref<AudioDestination> protectedDestination() { return m_destination; }
    Ref<AudioBus> protectedWorkBus() { return m_workBus; }

    unsigned m_numberOfOutputChannels;
    float m_sampleRate;

    Ref<AudioDestination> m_destination;
    Ref<AudioBus> m_workBus;

    bool m_started { false };

    Lock m_renderLock;

    using RenderVector = Vector<RefPtr<SharedAudioDestination>>;
    RenderVector m_renderers WTF_GUARDED_BY_CAPABILITY(mainThread);

    bool m_needsConfiguration WTF_GUARDED_BY_LOCK(m_renderLock) { true };
    RenderVector m_newRenderers WTF_GUARDED_BY_LOCK(m_renderLock);

    // Only accessed on the audio thread:
    RenderVector m_configuredRenderers;
};

auto SharedAudioDestinationAdapter::sharedMap() -> AdapterMap&
{
    static MainThreadNeverDestroyed<AdapterMap> map;
    return map;
}

Ref<SharedAudioDestinationAdapter> SharedAudioDestinationAdapter::ensureAdapter(unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&& ensureFunction)
{
    std::tuple key { numberOfOutputChannels, sampleRate };
    auto results = sharedMap().find(key);
    if (results != sharedMap().end()) {
        if (RefPtr existingAdapter = results->value.get())
            return existingAdapter.releaseNonNull();
    }

    Ref newAdapter = adoptRef(*new SharedAudioDestinationAdapter(numberOfOutputChannels, sampleRate, WTFMove(ensureFunction)));
    auto weakAdapter = ThreadSafeWeakPtr<SharedAudioDestinationAdapter> { newAdapter.get() };
    sharedMap().set(key, WTFMove(weakAdapter));
    return newAdapter;
}

SharedAudioDestinationAdapter::SharedAudioDestinationAdapter(unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&& ensureFunction)
    : m_numberOfOutputChannels { numberOfOutputChannels }
    , m_sampleRate { sampleRate }
    , m_destination { ensureFunction(*this) }
    , m_workBus { AudioBus::create(numberOfOutputChannels, AudioUtilities::renderQuantumSize).releaseNonNull() }
{
}

SharedAudioDestinationAdapter::~SharedAudioDestinationAdapter()
{
    auto key = std::make_tuple(m_numberOfOutputChannels, m_sampleRate);
    sharedMap().remove(key);
    protectedDestination()->clearCallback();
}

void SharedAudioDestinationAdapter::addRenderer(SharedAudioDestination& renderer, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsMainThread();
    if (!m_renderers.contains(&renderer))
        m_renderers.append(&renderer);
    configureRenderThread(WTFMove(completionHandler));
}

void SharedAudioDestinationAdapter::removeRenderer(SharedAudioDestination& renderer, CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsMainThread();
    m_renderers.removeFirst(&renderer);
    ASSERT(!m_renderers.contains(&renderer));
    configureRenderThread(WTFMove(completionHandler));
}

void SharedAudioDestinationAdapter::configureRenderThread(CompletionHandler<void(bool)>&& completionHandler)
{
    assertIsMainThread();

    bool shouldStart = !m_started && !m_renderers.isEmpty();
    bool shouldStop = m_started && m_renderers.isEmpty();
    bool shouldSkipRendering = !m_started && m_renderers.isEmpty();
    bool onlyNeedsConfiguration = m_started && !m_renderers.isEmpty();

    {
        Locker locker { m_renderLock };
        m_newRenderers = m_renderers;
        m_needsConfiguration = true;
        if (onlyNeedsConfiguration) {
            // The destination is already running, but needs configuration. Assume
            // the configuration will succeed and call the completionHandler early.
            callOnMainThread([completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(true);
            });
            return;
        }
    }

    if (shouldStart) {
        m_started = true;
        protectedDestination()->start(nullptr, WTFMove(completionHandler));
        return;
    }

    if (shouldStop) {
        m_started = false;
        protectedDestination()->stop(WTFMove(completionHandler));
        return;
    }

    // If the destination has not been started, and the list of
    // renderers is empty, do not wait for the render thread to
    // finish configuration, as it will never run.
    if (shouldSkipRendering) {
        callOnMainThread([completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(true);
        });
        return;
    }
}


void SharedAudioDestinationAdapter::render(AudioBus* sourceBus, AudioBus* destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition)
{
    if (m_renderLock.tryLock()) {
        Locker locker { AdoptLock, m_renderLock };
        if (m_needsConfiguration) {
            // The SharedAudioDestinationAdapter avoids allocing or deallocing on the
            // high priority audio thread by merely swapping the contents of the renderer
            // configuration vectors. After the swap, the previous contents of m_configuredRenderers
            // will be destroyed on the main thread.
            RenderVector oldRenderers = std::exchange(m_configuredRenderers, WTFMove(m_newRenderers));
            m_needsConfiguration = false;
            callOnMainThread([oldRenderers = WTFMove(oldRenderers)] () { });
        }
    }

    bool isFirstRenderer = true;
    for (RefPtr renderer : m_configuredRenderers) {
        if (isFirstRenderer) {
            // The first renderer should render directly to destinationBus.
            renderer->sharedRender(sourceBus, destinationBus, numberOfFrames, outputPosition);
            isFirstRenderer = false;
            continue;
        }
        // Subsequent renderers should render to the m_workBus, which will
        // then be summed to the destinationBus.
        Ref protectedWorkBus = this->protectedWorkBus();
        renderer->sharedRender(sourceBus, protectedWorkBus.ptr(), numberOfFrames, outputPosition);
        destinationBus->sumFrom(protectedWorkBus);
    }
}
Ref<SharedAudioDestination> SharedAudioDestination::create(AudioIOCallback& callback, unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&& ensureFunction)
{
    return adoptRef(*new SharedAudioDestination(callback, numberOfOutputChannels, sampleRate, WTFMove(ensureFunction)));
}

SharedAudioDestination::SharedAudioDestination(AudioIOCallback& callback, unsigned numberOfOutputChannels, float sampleRate, AudioDestinationCreationFunction&& ensureFunction)
    : AudioDestination(callback, sampleRate)
    , m_outputAdapter(SharedAudioDestinationAdapter::ensureAdapter(numberOfOutputChannels, sampleRate, WTFMove(ensureFunction)))
{
}

SharedAudioDestination::~SharedAudioDestination()
{
    if (isPlaying())
        stop([] (bool) { });
}

void SharedAudioDestination::start(Function<void(Function<void()>&&)>&& dispatchToRenderThread, CompletionHandler<void(bool)>&& completionHandler)
{
    {
        Locker locker { m_dispatchToRenderThreadLock };
        m_dispatchToRenderThread = WTFMove(dispatchToRenderThread);
    }

    setIsPlaying(true);
    protectedOutputAdapter()->addRenderer(*this, WTFMove(completionHandler));
}

void SharedAudioDestination::stop(CompletionHandler<void(bool)>&& completionHandler)
{
    setIsPlaying(false);
    protectedOutputAdapter()->removeRenderer(*this, WTFMove(completionHandler));

    {
        Locker locker { m_dispatchToRenderThreadLock };
        m_dispatchToRenderThread = nullptr;
    }
}

unsigned SharedAudioDestination::framesPerBuffer() const
{
    return m_outputAdapter->framesPerBuffer();
}

void SharedAudioDestination::setIsPlaying(bool isPlaying)
{
    ASSERT(isMainThread());

    if (m_isPlaying == isPlaying)
        return;

    m_isPlaying = isPlaying;

    {
        Locker locker { m_callbackLock };
        if (m_callback)
            m_callback->isPlayingDidChange();
    }
}

void SharedAudioDestination::sharedRender(AudioBus* sourceBus, AudioBus* destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition)
{
    if (!m_dispatchToRenderThreadLock.tryLock()) {
        destinationBus->zero();
        return;
    }

    Locker locker { AdoptLock, m_dispatchToRenderThreadLock };
    if (!m_dispatchToRenderThread)
        callRenderCallback(sourceBus, destinationBus, numberOfFrames, outputPosition);
    else {
        BinarySemaphore semaphore;
        m_dispatchToRenderThread([protectedThis = Ref { *this }, sourceBus = RefPtr { sourceBus }, destinationBus = RefPtr { destinationBus }, numberOfFrames, outputPosition, &semaphore]() mutable {
            protectedThis->callRenderCallback(sourceBus.get(), destinationBus.get(), numberOfFrames, outputPosition);
            semaphore.signal();
        });
        semaphore.wait();
    }
}

Ref<SharedAudioDestinationAdapter> SharedAudioDestination::protectedOutputAdapter()
{
    return m_outputAdapter;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
