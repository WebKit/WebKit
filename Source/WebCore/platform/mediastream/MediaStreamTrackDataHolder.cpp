/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamTrackDataHolder.h"
#include <wtf/CheckedRef.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

class PreventSourceFromEndingObserverWrapper : public ThreadSafeRefCounted<PreventSourceFromEndingObserverWrapper, WTF::DestructionThread::Main> {
public:
    static Ref<PreventSourceFromEndingObserverWrapper> create(Ref<RealtimeMediaSource>&& source)
    {
        auto wrapper = adoptRef(*new PreventSourceFromEndingObserverWrapper);
        wrapper->initialize(WTF::move(source));
        return wrapper;
    }

private:
    PreventSourceFromEndingObserverWrapper() = default;

    void initialize(Ref<RealtimeMediaSource>&& source)
    {
        ensureOnMainThread([protectedThis = Ref { *this }, source = WTF::move(source)] () mutable {
            protectedThis->m_observer = makeUnique<PreventSourceFromEndingObserver>(WTF::move(source));
        });
    }

    class PreventSourceFromEndingObserver final : public RealtimeMediaSourceObserver, public CanMakeCheckedPtr<PreventSourceFromEndingObserver> {
        WTF_MAKE_TZONE_ALLOCATED_INLINE(PreventSourceFromEndingObserver);
        WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PreventSourceFromEndingObserver);
    public:
        explicit PreventSourceFromEndingObserver(Ref<RealtimeMediaSource>&& source)
            : m_source(WTF::move(source))
        {
            m_source->addObserver(*this);
        }

        ~PreventSourceFromEndingObserver()
        {
            m_source->removeObserver(*this);
        }

        // RealtimeMediaSourceObserver.
        uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
        uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
        void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
        void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
        void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

    private:
        bool preventSourceFromEnding() final { return true; }

        const Ref<RealtimeMediaSource> m_source;
    };

    std::unique_ptr<PreventSourceFromEndingObserver> m_observer;
};

MediaStreamTrackDataHolder::MediaStreamTrackDataHolder(String&& trackId, String&& label, RealtimeMediaSource::Type type, CaptureDevice::DeviceType deviceType, bool isEnabled, bool isEnded, MediaStreamTrackHintValue contentHint, bool isProducingData, bool isMuted, bool isInterrupted, RealtimeMediaSourceSettings settings, RealtimeMediaSourceCapabilities capabilities, Ref<RealtimeMediaSource>&& source)
    : trackId(WTF::move(trackId))
    , label(WTF::move(label))
    , type(type)
    , deviceType(deviceType)
    , isEnabled(isEnabled)
    , isEnded(isEnded)
    , contentHint(contentHint)
    , isProducingData(isProducingData)
    , isMuted(isMuted)
    , isInterrupted(isInterrupted)
    , settings(WTF::move(settings))
    , capabilities(WTF::move(capabilities))
    , source(source.get())
    , preventSourceFromEndingObserverWrapper(PreventSourceFromEndingObserverWrapper::create(WTF::move(source)))
{
}

MediaStreamTrackDataHolder::~MediaStreamTrackDataHolder()
{
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
