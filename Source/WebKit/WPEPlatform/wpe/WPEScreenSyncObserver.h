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

#ifndef WPEScreenSyncObserver_h
#define WPEScreenSyncObserver_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_SCREEN_SYNC_OBSERVER (wpe_screen_sync_observer_get_type())
WPE_API WPE_DECLARE_DERIVABLE_TYPE (WPEScreenSyncObserver, wpe_screen_sync_observer, WPE, SCREEN_SYNC_OBSERVER, GObject)

/**
 * WPEScreenSyncObserverSyncFunc:
 * @observer: a #WPEScreenSyncObserver
 * @user_data: user data
 *
 * Function passed to wpe_screen_sync_observer_add() to be called on
 * every #WPEScreen sync.
 */
typedef void (* WPEScreenSyncObserverSyncFunc) (WPEScreenSyncObserver *observer,
                                                gpointer               user_data);

struct _WPEScreenSyncObserverClass
{
    GObjectClass parent_class;

    void (* start) (WPEScreenSyncObserver *observer);
    void (* stop)  (WPEScreenSyncObserver *observer);
    void (* sync)  (WPEScreenSyncObserver *observer);

    gpointer padding[32];
};

WPE_API void     wpe_screen_sync_observer_set_callback (WPEScreenSyncObserver        *observer,
                                                        WPEScreenSyncObserverSyncFunc sync_func,
                                                        gpointer                      user_data,
                                                        GDestroyNotify                destroy_notify);
WPE_API void     wpe_screen_sync_observer_start        (WPEScreenSyncObserver        *observer);
WPE_API void     wpe_screen_sync_observer_stop         (WPEScreenSyncObserver        *observer);
WPE_API gboolean wpe_screen_sync_observer_is_active    (WPEScreenSyncObserver        *observer);

G_END_DECLS

#endif /* WPEScreenSyncObserver_h */
