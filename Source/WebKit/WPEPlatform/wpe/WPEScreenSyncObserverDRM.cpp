/*
 * Copyright (C) 2025 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEScreenSyncObserverDRM.h"

#include <chrono>
#include <errno.h>
#include <fcntl.h>
#include <thread>
#include <wtf/Condition.h>
#include <wtf/Function.h>
#include <wtf/Lock.h>
#include <wtf/RunLoop.h>
#include <wtf/Threading.h>
#include <wtf/Vector.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>
#include <xf86drm.h>

struct _WPEScreenSyncObserverDRMPrivate;

namespace WTF {
template<typename T> struct IsDeprecatedTimerSmartPointerException;
template<> struct IsDeprecatedTimerSmartPointerException<_WPEScreenSyncObserverDRMPrivate> : std::true_type { };
}

enum class State {
    Stop,
    Active,
    Failed,
    Invalid
};

struct _WPEScreenSyncObserverDRMPrivate {
    _WPEScreenSyncObserverDRMPrivate()
        : destroyThreadTimer(RunLoop::currentSingleton(), "_WPEScreenSyncObserverDRMPrivate::DestroyThreadTimer"_s, this, &_WPEScreenSyncObserverDRMPrivate::invalidate)
    {
        destroyThreadTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
    }

    void invalidate()
    {
        if (!thread) {
            state = State::Invalid;
            return;
        }

        destroyThreadTimer.stop();
        {
            Locker locker { lock };
            state = State::Invalid;
            condition.notifyAll();
        }
        thread->waitForCompletion();
        thread = nullptr;
    }

    UnixFileDescriptor fd;
    int crtcBitmask;

    RefPtr<Thread> thread;
    Lock lock;
    Condition condition;
    State state;
    RunLoop::Timer destroyThreadTimer;
};

WEBKIT_DEFINE_FINAL_TYPE(WPEScreenSyncObserverDRM, wpe_screen_sync_observer_drm, WPE_TYPE_SCREEN_SYNC_OBSERVER, WPEScreenSyncObserver)

static void wpeScreenSyncObserverDRMDispose(GObject* object)
{
    auto* priv = WPE_SCREEN_SYNC_OBSERVER_DRM(object)->priv;
    priv->invalidate();

    G_OBJECT_CLASS(wpe_screen_sync_observer_drm_parent_class)->dispose(object);
}

static void wpeScreenSyncObserverDRMStart(WPEScreenSyncObserver* observer)
{
    auto* priv = WPE_SCREEN_SYNC_OBSERVER_DRM(observer)->priv;
    Locker locker { priv->lock };
    if (priv->state != State::Active)
        priv->state = State::Active;

    priv->destroyThreadTimer.stop();
    if (!priv->thread) {
        priv->thread = Thread::create("WPEScreenSyncObserverDRM"_s, [observer] {
            auto* priv = WPE_SCREEN_SYNC_OBSERVER_DRM(observer)->priv;
            while (true) {
                {
                    Locker locker { priv->lock };
                    priv->condition.wait(priv->lock, [priv]() -> bool {
                        return priv->state != State::Stop;
                    });
                    if (priv->state == State::Invalid || priv->state == State::Failed)
                        return;
                }

                drmVBlank vblank;
                vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | priv->crtcBitmask);
                vblank.request.sequence = 1;
                vblank.request.signal = 0;
                auto ret = drmWaitVBlank(priv->fd.value(), &vblank);
                if (!ret || ret == -EPERM) {
                    if (ret == -EPERM) {
                        // This can happen when the screen is suspended and the web view hasn't noticed it.
                        // The display link should be stopped in those cases, but since it isn't, we can at
                        // least sleep for a while pretending the screen is on.
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }

                    bool isActive;
                    {
                        Locker locker { priv->lock };
                        isActive = priv->state == State::Active;
                    }
                    if (isActive)
                        WPE_SCREEN_SYNC_OBSERVER_CLASS(wpe_screen_sync_observer_drm_parent_class)->sync(observer);
                } else if (ret) {
                    drmError(ret, "WPEScreenSyncObserverDRM");
                    Locker locker { priv->lock };
                    priv->state = State::Failed;
                }
            }
        }, ThreadType::Graphics, Thread::QOS::Default);
    } else
        priv->condition.notifyAll();
}

static void wpeScreenSyncObserverDRMStop(WPEScreenSyncObserver* observer)
{
    auto* priv = WPE_SCREEN_SYNC_OBSERVER_DRM(observer)->priv;
    Locker locker { priv->lock };
    priv->state = State::Stop;
    if (priv->thread)
        priv->destroyThreadTimer.startOneShot(30_s);
}

static void wpe_screen_sync_observer_drm_class_init(WPEScreenSyncObserverDRMClass* screenSyncObserverDRMClass)
{
    auto* objectClass = G_OBJECT_CLASS(screenSyncObserverDRMClass);
    objectClass->dispose = wpeScreenSyncObserverDRMDispose;

    auto* screenSyncObserverClass = WPE_SCREEN_SYNC_OBSERVER_CLASS(screenSyncObserverDRMClass);
    screenSyncObserverClass->start = wpeScreenSyncObserverDRMStart;
    screenSyncObserverClass->stop = wpeScreenSyncObserverDRMStop;
}

static int crtcBitmaskForIndex(uint32_t crtcIndex)
{
    if (crtcIndex > 1)
        return ((crtcIndex << DRM_VBLANK_HIGH_CRTC_SHIFT) & DRM_VBLANK_HIGH_CRTC_MASK);
    if (crtcIndex > 0)
        return DRM_VBLANK_SECONDARY;
    return 0;
}

WPEScreenSyncObserver* wpeScreenSyncObserverDRMCreate(UnixFileDescriptor&& fd, int crtcIndex)
{
    auto crtcBitmask = crtcBitmaskForIndex(crtcIndex);
    drmVBlank vblank;
    vblank.request.type = static_cast<drmVBlankSeqType>(DRM_VBLANK_RELATIVE | crtcBitmask);
    vblank.request.sequence = 0;
    vblank.request.signal = 0;
    if (drmWaitVBlank(fd.value(), &vblank))
        return nullptr;

    auto* observer = WPE_SCREEN_SYNC_OBSERVER_DRM(g_object_new(WPE_TYPE_SCREEN_SYNC_OBSERVER_DRM, nullptr));
    observer->priv->fd = WTFMove(fd);
    observer->priv->crtcBitmask = crtcBitmask;
    return WPE_SCREEN_SYNC_OBSERVER(observer);
}
