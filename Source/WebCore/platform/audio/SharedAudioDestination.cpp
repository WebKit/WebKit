/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
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
#include <wtf/MediaTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/WTFSemaphore.h>
#include <wtf/WeakPtr.h>
#include <wtf/WorkQueue.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

class SharedAudioDestinationAdapter : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<SharedAudioDestinationAdapter>, public AudioIOCallback {
public:
    using CreationOptions = AudioDestinationCreationOptions;
    using AudioDestinationCreationFunction = SharedAudioDestination::AudioDestinationCreationFunction;
    static Ref<SharedAudioDestinationAdapter> ensureAdapter(const CreationOptions&, AudioDestinationCreationFunction&& ensureFunction);
    ~SharedAudioDestinationAdapter();

    void addRenderer(SharedAudioDestination&, CompletionHandler<void(bool)>&&);
    void removeRenderer(SharedAudioDestination&, CompletionHandler<void(bool)>&&);

    unsigned framesPerBuffer() const
    {
        return m_workBus->length();
    }

    MediaTime outputLatency() const
    {
        return m_destination->outputLatency();
    }

#if PLATFORM(IOS_FAMILY)
    const String& sceneIdentifier() const { return m_sceneIdentifier; }
#endif

    AudioDestinationCreationFunction takeEnsureFunction() { return WTFMove(m_ensureFunction); }

private:
#if PLATFORM(IOS_FAMILY)
    using AdapterKey = std::tuple<unsigned, float, String>;
#else
    using AdapterKey = std::tuple<unsigned, float>;
#endif
    using AdapterMap = HashMap<AdapterKey, ThreadSafeWeakPtr<SharedAudioDestinationAdapter>>;
    static AdapterMap& sharedMap();

    SharedAudioDestinationAdapter(const CreationOptions&, AudioDestinationCreationFunction&&);

    void render(AudioBus& destinationBus, size_t framesToProcess, const AudioIOPosition& outputPosition) final;
    void isPlayingDidChange() final { }

    void configureRenderThread(CompletionHandler<void(bool)>&&);

    unsigned m_numberOfOutputChannels;
    float m_sampleRate;

#if PLATFORM(IOS_FAMILY)
    String m_sceneIdentifier { emptyString() };
#endif

    const Ref<AudioDestination> m_destination;
    const Ref<AudioBus> m_workBus;
    AudioDestinationCreationFunction m_ensureFunction;

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

Ref<SharedAudioDestinationAdapter> SharedAudioDestinationAdapter::ensureAdapter(const CreationOptions& options, AudioDestinationCreationFunction&& ensureFunction)
{
    std::tuple key { options.numberOfOutputChannels, options.sampleRate
#if PLATFORM(IOS_FAMILY)
        , options.sceneIdentifier.isNull() ? emptyString() : options.sceneIdentifier
#endif
    };
    auto results = sharedMap().find(key);
    if (results != sharedMap().end()) {
        if (RefPtr existingAdapter = results->value.get())
            return existingAdapter.releaseNonNull();
    }

    Ref newAdapter = adoptRef(*new SharedAudioDestinationAdapter(options, WTFMove(ensureFunction)));
    auto weakAdapter = ThreadSafeWeakPtr<SharedAudioDestinationAdapter> { newAdapter.get() };
    sharedMap().set(key, WTFMove(weakAdapter));
    return newAdapter;
}

SharedAudioDestinationAdapter::SharedAudioDestinationAdapter(const CreationOptions& options, AudioDestinationCreationFunction&& ensureFunction)
    : m_numberOfOutputChannels { options.numberOfOutputChannels }
    , m_sampleRate { options.sampleRate }
    , m_destination { ensureFunction({ *this, options.inputDeviceId, options.numberOfInputChannels, options.numberOfOutputChannels, options.sampleRate
#if PLATFORM(IOS_FAMILY)
        , options.sceneIdentifier.isNull() ? emptyString() : options.sceneIdentifier
#endif
        }) }
    , m_workBus { AudioBus::create(options.numberOfOutputChannels, AudioUtilities::renderQuantumSize) }
    , m_ensureFunction { WTFMove(ensureFunction) }
{
}

SharedAudioDestinationAdapter::~SharedAudioDestinationAdapter()
{
    auto key = std::make_tuple(m_numberOfOutputChannels, m_sampleRate
#if PLATFORM(IOS_FAMILY)
        , m_sceneIdentifier
#endif
        );
    sharedMap().remove(key);
    m_destination->clearCallback();
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
        m_destination->start(nullptr, WTFMove(completionHandler));
        return;
    }

    if (shouldStop) {
        m_started = false;
        m_destination->stop(WTFMove(completionHandler));
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


void SharedAudioDestinationAdapter::render(AudioBus& destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition)
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
            renderer->sharedRender(destinationBus, numberOfFrames, outputPosition);
            isFirstRenderer = false;
            continue;
        }
        // Subsequent renderers should render to the m_workBus, which will
        // then be summed to the destinationBus.
        renderer->sharedRender(m_workBus, numberOfFrames, outputPosition);
        destinationBus.sumFrom(m_workBus);
    }
}
Ref<SharedAudioDestination> SharedAudioDestination::create(const CreationOptions& options, AudioDestinationCreationFunction&& ensureFunction)
{
    return adoptRef(*new SharedAudioDestination(options, WTFMove(ensureFunction)));
}

SharedAudioDestination::SharedAudioDestination(const CreationOptions& options, AudioDestinationCreationFunction&& ensureFunction)
    : AudioDestination(options)
    , m_outputAdapter(SharedAudioDestinationAdapter::ensureAdapter(options, WTFMove(ensureFunction)))
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

MediaTime SharedAudioDestination::outputLatency() const
{
    return protectedOutputAdapter()->outputLatency();
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

void SharedAudioDestination::sharedRender(AudioBus& destinationBus, size_t numberOfFrames, const AudioIOPosition& outputPosition)
{
    if (!m_dispatchToRenderThreadLock.tryLock()) {
        destinationBus.zero();
        return;
    }

    Locker locker { AdoptLock, m_dispatchToRenderThreadLock };
    if (!m_dispatchToRenderThread)
        callRenderCallback(destinationBus, numberOfFrames, outputPosition);
    else {
        BinarySemaphore semaphore;
        m_dispatchToRenderThread([protectedThis = Ref { *this }, destinationBus = Ref { destinationBus }, numberOfFrames, outputPosition, &semaphore]() mutable {
            protectedThis->callRenderCallback(destinationBus, numberOfFrames, outputPosition);
            semaphore.signal();
        });
        semaphore.wait();
    }
}

#if PLATFORM(IOS_FAMILY)
class NullAudioIOCallback final : public AudioIOCallback {
public:
    static NullAudioIOCallback& singleton()
    {
        static NeverDestroyed<NullAudioIOCallback> callback;
        return callback.get();
    }
private:
    void render(AudioBus&, size_t, const AudioIOPosition&) final { }
    void isPlayingDidChange() final { }
};

void SharedAudioDestination::setSceneIdentifier(const String& identifier)
{
    if (protectedOutputAdapter()->sceneIdentifier() == identifier)
        return;

    // We need to re-create the outputAdapter when the sceneIdentifier
    // changes, as the adapter may be shared with other destinations
    // whose sceneIdentifier is _not_ changing.
    auto ensureFunction = protectedOutputAdapter()->takeEnsureFunction();
    ASSERT(ensureFunction);
    if (!ensureFunction)
        return;

    bool wasPlaying = isPlaying();

    if (wasPlaying)
        protectedOutputAdapter()->removeRenderer(*this, [] (bool) { });

    m_outputAdapter = SharedAudioDestinationAdapter::ensureAdapter({
        NullAudioIOCallback::singleton(),
        inputDeviceId(),
        numberOfInputChannels(),
        numberOfOutputChannels(),
        sampleRate(),
#if PLATFORM(IOS_FAMILY)
        identifier,
#endif
    }, WTFMove(ensureFunction));

    if (wasPlaying)
        protectedOutputAdapter()->addRenderer(*this, [] (bool) { });
}
#endif

Ref<SharedAudioDestinationAdapter> SharedAudioDestination::protectedOutputAdapter() const
{
    return m_outputAdapter;
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
