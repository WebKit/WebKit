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

#include <wtf/glib/WTFGType.h>

/**
 * WPEScreenSyncObserver:
 *
 * A screen sync observer.
 */
struct _WPEScreenSyncObserverPrivate {
    gboolean isActive;
    WPEScreenSyncObserverSyncFunc syncFunc;
    gpointer userData;
    GDestroyNotify destroyNotify;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEScreenSyncObserver, wpe_screen_sync_observer, G_TYPE_OBJECT)

static void wpeScreenSyncObserverDispose(GObject* object)
{
    auto* priv = WPE_SCREEN_SYNC_OBSERVER(object)->priv;
    if (priv->syncFunc) {
        if (priv->destroyNotify) {
            priv->destroyNotify(priv->userData);
            priv->destroyNotify = nullptr;
        }
        priv->syncFunc = nullptr;
        priv->userData = nullptr;
    }

    G_OBJECT_CLASS(wpe_screen_sync_observer_parent_class)->dispose(object);
}

static void wpeScreenSyncObserverSync(WPEScreenSyncObserver* observer)
{
    auto* priv = observer->priv;
    priv->syncFunc(observer, priv->userData);
}

static void wpe_screen_sync_observer_class_init(WPEScreenSyncObserverClass* screenSyncObserverClass)
{
    auto* objectClass = G_OBJECT_CLASS(screenSyncObserverClass);
    objectClass->dispose = wpeScreenSyncObserverDispose;

    screenSyncObserverClass->sync = wpeScreenSyncObserverSync;
}

/**
 * wpe_screen_sync_observer_set_callback:
 * @observer: a #WPEScreenSyncObserver
 * @sync_func: (scope notified): a #WPEScreenSyncObserverSyncFunc
 * @user_data: data to pass to @sync_func
 * @destroy_notify: (nullable): function for freeing @user_data or %NULL.
 *
 * Add a @sync_func to be called from a secondary thread when the screen sync is triggered.
 * The callback must be set only once and before calling wpe_screen_sync_start().
 */
void wpe_screen_sync_observer_set_callback(WPEScreenSyncObserver* observer, WPEScreenSyncObserverSyncFunc syncFunc, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer));
    g_return_if_fail(syncFunc);

    auto* priv = observer->priv;
    g_return_if_fail(!priv->isActive);
    g_return_if_fail(!priv->syncFunc);

    priv->syncFunc = syncFunc;
    priv->userData = userData;
    priv->destroyNotify = destroyNotify;
}

/**
 * wpe_screen_sync_observer_start:
 * @observer: a #WPEScreenSyncObserver
 *
 * Start the @observer.
 */
void wpe_screen_sync_observer_start(WPEScreenSyncObserver* observer)
{
    g_return_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer));

    auto* priv = observer->priv;
    g_return_if_fail(priv->syncFunc);

    if (priv->isActive)
        return;

    auto* screenSyncObserverClass = WPE_SCREEN_SYNC_OBSERVER_GET_CLASS(observer);
    screenSyncObserverClass->start(observer);
    priv->isActive = TRUE;
}

/**
 * wpe_screen_sync_observer_stop:
 * @observer: a #WPEScreenSyncObserver
 *
 * Stop the @observer.
 */
void wpe_screen_sync_observer_stop(WPEScreenSyncObserver* observer)
{
    g_return_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer));

    auto* priv = observer->priv;
    if (!priv->isActive)
        return;

    auto* screenSyncObserverClass = WPE_SCREEN_SYNC_OBSERVER_GET_CLASS(observer);
    screenSyncObserverClass->stop(observer);
    priv->isActive = FALSE;
}

/**
 * wpe_screen_sync_observer_is_active:
 * @observer: a #WPEScreenSyncObserver
 *
 * Return whether @observer is active.
 *
 * Returns: %TRUE if @observer is active, or %FALSE otherwise
 */
gboolean wpe_screen_sync_observer_is_active(WPEScreenSyncObserver* observer)
{
    g_return_val_if_fail(WPE_IS_SCREEN_SYNC_OBSERVER(observer), FALSE);

    return observer->priv->isActive;
}
