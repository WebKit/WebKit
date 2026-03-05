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
#include "WPEScreenSyncObserver.h"

#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/Vector.h>
#include <wtf/glib/WTFGType.h>

struct ObserverCallback {
    WTF_MAKE_STRUCT_TZONE_ALLOCATED(ObserverCallback);

    ObserverCallback(WPEScreenSyncObserverSyncFunc syncFunc, gpointer userData, GDestroyNotify destroyNotify)
        : syncFunc(syncFunc)
        , userData(userData)
        , destroyNotify(destroyNotify)
    {
    }

    ~ObserverCallback()
    {
        if (destroyNotify)
            destroyNotify(userData);
    }

    WPEScreenSyncObserverSyncFunc syncFunc;
    gpointer userData;
    GDestroyNotify destroyNotify;
};

WTF_MAKE_STRUCT_TZONE_ALLOCATED_IMPL(ObserverCallback);

/**
 * WPEScreenSyncObserver:
 *
 * A screen sync observer.
 */
struct _WPEScreenSyncObserverPrivate {
    Lock lock;
    HashMap<unsigned, std::unique_ptr<ObserverCallback>> callbacks WTF_GUARDED_BY_LOCK(lock);
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEScreenSyncObserver, wpe_screen_sync_observer, G_TYPE_OBJECT)

static void wpeScreenSyncObserverSync(WPEScreenSyncObserver* observer)
{
    auto* priv = observer->priv;
    Vector<std::pair<WPEScreenSyncObserverSyncFunc, gpointer>> callbacks;
    {
        Locker locker { priv->lock };
        callbacks.reserveInitialCapacity(priv->callbacks.size());
        for (const auto& callback : priv->callbacks.values())
            callbacks.append({ callback->syncFunc, callback->userData });
    }

    for (auto [syncFunc, userData] : callbacks)
        syncFunc(observer, userData);
}

static void wpe_screen_sync_observer_class_init(WPEScreenSyncObserverClass* screenSyncObserverClass)
{
    screenSyncObserverClass->sync = wpeScreenSyncObserverSync;
}

/**
 * wpe_screen_sync_observer_add_callback:
 * @observer: a #WPEScreenSyncObserver
 * @sync_func: (scope notified): a #WPEScreenSyncObserverSyncFunc
 * @user_data: data to pass to @sync_func
 * @destroy_notify: (nullable): function for freeing @user_data or %NULL.
 *
 * Add a @sync_func to be called from a secondary thread when the screen sync is triggered.
 *
 * To remove this callback pass the identifier that is returned by this function to
 * wpe_screen_sync_observer_remove_callback().
 *
 * Returns: an identifier for this callback
 */
guint wpe_screen_sync_observer_add_callback(WPEScreenSyncObserver* observer, WPEScreenSyncObserverSyncFunc syncFunc, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_val_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer), 0);
    g_return_val_if_fail(syncFunc, 0);

    auto* priv = observer->priv;
    bool shouldStart = false;
    unsigned nextCallbackID = 0;
    {
        Locker locker { priv->lock };
        static unsigned callbackID = 0;
        nextCallbackID = ++callbackID;
        shouldStart = priv->callbacks.isEmpty();
        priv->callbacks.add(nextCallbackID, makeUnique<ObserverCallback>(syncFunc, userData, destroyNotify));
    }

    if (shouldStart) {
        auto* screenSyncObserverClass = WPE_SCREEN_SYNC_OBSERVER_GET_CLASS(observer);
        screenSyncObserverClass->start(observer);
    }

    return nextCallbackID;
}

/**
 * wpe_screen_sync_observer_remove_callback:
 * @observer: a #WPEScreenSyncObserver
 * @id: an identifier returned by wpe_screen_sync_observer_add_callback()
 *
 * Remove a callback previously added with wpe_screen_sync_observer_add_callback().
 */
void wpe_screen_sync_observer_remove_callback(WPEScreenSyncObserver* observer, guint id)
{
    g_return_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer));
    g_return_if_fail(id > 0);

    auto* priv = observer->priv;
    bool shouldStop = false;
    {
        Locker locker { priv->lock };
        priv->callbacks.remove(id);
        shouldStop = priv->callbacks.isEmpty();
    }

    if (shouldStop) {
        auto* screenSyncObserverClass = WPE_SCREEN_SYNC_OBSERVER_GET_CLASS(observer);
        screenSyncObserverClass->stop(observer);
    }
}
