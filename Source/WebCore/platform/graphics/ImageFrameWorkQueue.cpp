/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#include "ImageFrameWorkQueue.h"

#include "BitmapImageSource.h"
#include "ImageDecoder.h"
#include "Logging.h"
#include <mutex>
#include <wtf/AutodrainedPool.h>
#include <wtf/Lock.h>
#include <wtf/MonotonicTime.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/NumberOfCores.h>
#include <wtf/RunLoop.h>
#include <wtf/SystemTracing.h>
#include <wtf/Vector.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/TextStream.h>

#if USE(COCOA_EVENT_LOOP)
#include <wtf/darwin/DispatchExtras.h>
#endif

namespace WebCore {

#if USE(COCOA_EVENT_LOOP)

// Serial image queues share one concurrent target, so libdispatch can manage
// execution resources across images without workers waiting for future frames.
static ConcurrentWorkQueue& imageDecodingTargetQueue()
{
    static LazyNeverDestroyed<Ref<ConcurrentWorkQueue>> queue;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        queue.construct(ConcurrentWorkQueue::create("org.webkit.ImageDecoder"_s, WorkQueue::QOS::Default));
    });
    return queue.get().get();
}

class ImageDecodingQueue : public ThreadSafeRefCounted<ImageDecodingQueue> {
public:
    static Ref<ImageDecodingQueue> create() { return adoptRef(*new ImageDecodingQueue); }

    void dispatch(Function<void()>&& request)
    {
        dispatch_async_f(m_dispatchQueue.get(), request.leak(), [](void* context) {
            auto request = WTF::adopt(static_cast<Function<void()>::Impl*>(context));
            request();
        });
    }

private:
    ImageDecodingQueue()
    {
        // Specify the target's QoS on each image queue as well.
        auto targetQueue = imageDecodingTargetQueue().dispatchQueue();
        int relativePriority = 0;
        auto qos = dispatch_queue_get_qos_class(targetQueue, &relativePriority);
        auto attributes = dispatch_queue_attr_make_with_qos_class(serialQueueWithAutoreleasePoolAttrSingleton(), qos, relativePriority);
        // dispatch_queue_create_with_target returns a +1 reference, but its
        // ownership annotation is unavailable when compiling as C++.
        SUPPRESS_RETAINPTR_CTOR_ADOPT m_dispatchQueue = adoptOSObject(dispatch_queue_create_with_target("org.webkit.ImageDecoder.Image", attributes, targetQueue));
    }

    OSObjectPtr<dispatch_queue_t> m_dispatchQueue;
};

#else

// Requests for one image stay serial, including across stop() and restart. Images
// have no permanent worker affinity: a ready image can use any available worker.
class ImageDecodingQueue : public ThreadSafeRefCounted<ImageDecodingQueue> {
public:
    static Ref<ImageDecodingQueue> create() { return adoptRef(*new ImageDecodingQueue); }

    void dispatch(Function<void()>&&);
    bool runOne();

private:
    Lock m_lock;
    Deque<Function<void()>> m_requests WTF_GUARDED_BY_LOCK(m_lock);
    bool m_scheduled WTF_GUARDED_BY_LOCK(m_lock) { false };
};

class ImageDecodingScheduler {
public:
    static ImageDecodingScheduler& singleton()
    {
        static LazyNeverDestroyed<ImageDecodingScheduler> scheduler;
        static std::once_flag onceFlag;
        std::call_once(onceFlag, [] {
            scheduler.construct();
        });
        return scheduler.get();
    }

    void dispatch(Ref<ImageDecodingQueue>&& queue)
    {
        RefPtr<WorkQueue> worker;
        {
            Locker locker { m_lock };
            m_readyQueues.append(WTF::move(queue));
            if (!m_idleWorkers.isEmpty())
                worker = m_idleWorkers.takeLast();
            else if (m_workerCount < m_maximumConcurrency)
                ++m_workerCount;
            else
                return;
            queue = m_readyQueues.takeFirst();
        }

        if (!worker)
            worker = WorkQueue::create("org.webkit.ImageDecoder"_s, WorkQueue::QOS::Default);

        worker->dispatch([this, worker = Ref { *worker }, queue = WTF::move(queue)] () mutable {
            while (true) {
                bool hasMoreRequests = queue->runOne();
                Locker locker { m_lock };
                // Give other ready images a turn before this image's next frame.
                if (hasMoreRequests)
                    m_readyQueues.append(WTF::move(queue));
                if (m_readyQueues.isEmpty()) {
                    m_idleWorkers.append(WTF::move(worker));
                    return;
                }
                queue = m_readyQueues.takeFirst();
            }
        });
    }

private:
    // Queue pressure must not turn into threads waiting for work or admission.
    // Reserve workers before dispatch, and reuse them across images and frames.
    const unsigned m_maximumConcurrency { static_cast<unsigned>(std::max(1, WTF::numberOfProcessorCores())) };
    Lock m_lock;
    Deque<Ref<ImageDecodingQueue>> m_readyQueues WTF_GUARDED_BY_LOCK(m_lock);
    Vector<Ref<WorkQueue>> m_idleWorkers WTF_GUARDED_BY_LOCK(m_lock);
    unsigned m_workerCount WTF_GUARDED_BY_LOCK(m_lock) { 0 };
};

void ImageDecodingQueue::dispatch(Function<void()>&& request)
{
    {
        Locker locker { m_lock };
        m_requests.append(WTF::move(request));
        if (m_scheduled)
            return;
        m_scheduled = true;
    }
    ImageDecodingScheduler::singleton().dispatch(Ref { *this });
}

bool ImageDecodingQueue::runOne()
{
    AutodrainedPool pool;
    Function<void()> request;
    {
        Locker locker { m_lock };
        ASSERT(m_scheduled);
        request = m_requests.takeFirst();
    }
    request();

    Locker locker { m_lock };
    m_scheduled = !m_requests.isEmpty();
    return m_scheduled;
}

#endif

Ref<ImageFrameWorkQueue> ImageFrameWorkQueue::create(BitmapImageSource& source)
{
    return adoptRef(*new ImageFrameWorkQueue(source));
}

ImageFrameWorkQueue::ImageFrameWorkQueue(BitmapImageSource& source)
    : m_source(source)
    , m_workQueue(ImageDecodingQueue::create())
{
}

ImageFrameWorkQueue::~ImageFrameWorkQueue() = default;

void ImageFrameWorkQueue::dispatch(const Request& request)
{
    // Reuse the decoder until stop(), as the original request loop did. Finishing
    // an idle work item must not discard the decoder's state between frames.
    if (!m_decoder)
        m_decoder = m_source.get()->decoder();
    if (!m_decoder)
        return;

    decodeQueue().append(request);

    // Scheduling the next request does not wait for the creation thread to
    // handle completion, and an empty queue never keeps a worker waiting.
    m_workQueue->dispatch([protectedThis = Ref { *this }, weakRunLoop = ThreadSafeWeakPtr { RunLoop::currentSingleton() }, protectedSource = m_source.get(), protectedDecoder = Ref { *m_decoder }, generation = m_generation.load(std::memory_order_relaxed), minimumDecodingDuration = m_minimumDecodingDurationForTesting, request = request] () mutable {
        RefPtr<NativeImage> nativeImage;
        if (generation == protectedThis->m_generation.load(std::memory_order_relaxed)) {
            TraceScope tracingScope(AsyncImageDecodeStart, AsyncImageDecodeEnd);

            MonotonicTime startingTime;
            if (minimumDecodingDuration > 0_s)
                startingTime = MonotonicTime::now();

            DecodingDestination decodingDestination = request.options.decodingDestination();
            if (auto result = protectedDecoder->createNativeImageAtIndex(request.index, request.subsamplingLevel, request.options)) {
                nativeImage = WTF::move(std::get<Ref<NativeImage>>(*result));
                decodingDestination = std::get<DecodingDestination>(*result);
            }

            request.options = { request.options.decodingMode(), decodingDestination, request.options.sizeForDrawing() };

            // Pretend as if decoding the frame took minimumDecodingDuration.
            if (minimumDecodingDuration > 0_s) {
                auto actualDecodingDuration = MonotonicTime::now() - startingTime;
                if (minimumDecodingDuration > actualDecodingDuration)
                    sleep(minimumDecodingDuration - actualDecodingDuration);
            }
        }

        if (RefPtr protectedRunLoop = weakRunLoop.get()) {
            // Post completion for cancelled requests and decoding failures too, so
            // the creation thread can release the source and work queue.
            callOnRunLoop(*protectedRunLoop, [protectedThis = WTF::move(protectedThis), protectedSource = WTF::move(protectedSource), generation, request, nativeImage = WTF::move(nativeImage)] () mutable {
                if (generation != protectedThis->m_generation.load(std::memory_order_relaxed))
                    return;

                // The DecodeQueue may have been cleared before the frame was decoded.
                if (protectedThis->decodeQueue().isEmpty() || !request.isCompatibleWith(protectedThis->decodeQueue().first())) {
                    LOG(Images, "AsyncImageDecoder::%s - %p. DecodeQueue was cleared at index = %d.", __FUNCTION__, protectedThis.ptr(), request.index);
                    return;
                }

                protectedThis->decodeQueue().removeFirst();
                protectedSource->imageFrameDecodeAtIndexHasFinished(request.index, request.subsamplingLevel, request.animatingState, request.options, WTF::move(nativeImage));
            });
        }
    });
}

void ImageFrameWorkQueue::stop()
{
    m_generation.fetch_add(1, std::memory_order_relaxed);
    Ref source = m_source.get();

    for (auto& request : m_decodeQueue) {
        LOG(Images, "AsyncImageDecoder::%s - %p. Decoding was cancelled for frame at index = %d.", __FUNCTION__, this, request.index);
        source->destroyNativeImageAtIndex(request.index);
    }

    m_decodeQueue.clear();
    m_decoder = nullptr;
}

bool ImageFrameWorkQueue::isPendingDecodingAtIndex(unsigned index, SubsamplingLevel subsamplingLevel, const DecodingOptions& options) const
{
    auto it = std::find_if(m_decodeQueue.begin(), m_decodeQueue.end(), [index, subsamplingLevel, &options](const Request& request) {
        return request.index == index && subsamplingLevel >= request.subsamplingLevel && request.options.isCompatibleWith(options);
    });
    return it != m_decodeQueue.end();
}

void ImageFrameWorkQueue::dump(TextStream& ts) const
{
    if (isIdle())
        return;

    ts.dumpProperty("pending-for-decoding"_s, m_decodeQueue.size());
}

} // namespace WebCore
