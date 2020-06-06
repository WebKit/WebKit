/*
 * Copyright (C) 2020 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(DRAG_SUPPORT)

#include <WebCore/DragActions.h>
#include <WebCore/IntPoint.h>
#include <WebCore/SelectionData.h>
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GtkWidget GtkWidget;

#if USE(GTK4)
typedef struct _GdkDrop GdkDrop;
#else
typedef struct _GdkDragContext GdkDragContext;
typedef struct _GtkSelectionData GtkSelectionData;
#endif

namespace WebKit {

class ShareableBitmap;

class DropTarget {
    WTF_MAKE_NONCOPYABLE(DropTarget); WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DropTarget(GtkWidget*);
    ~DropTarget();

    void didPerformAction();

private:
    void accept(unsigned = 0);
    void enter(WebCore::IntPoint&&, unsigned = 0);
    void update(WebCore::IntPoint&&, unsigned = 0);
    void leave();
    void drop(WebCore::IntPoint&&, unsigned = 0);

#if USE(GTK4)
    void loadData(const char* mimeType, CompletionHandler<void(GRefPtr<GBytes>&&)>&&);
    void didLoadData();
#else
    void dataReceived(WebCore::IntPoint&&, GtkSelectionData*, unsigned, unsigned);
    void leaveTimerFired();
#endif

    GtkWidget* m_webView { nullptr };
#if USE(GTK4)
    GRefPtr<GdkDrop> m_drop;
#else
    GRefPtr<GdkDragContext> m_drop;
#endif
    Optional<WebCore::IntPoint> m_position;
    unsigned m_dataRequestCount { 0 };
    Optional<WebCore::SelectionData> m_selectionData;
    Optional<WebCore::DragOperation> m_operation;
#if USE(GTK4)
    GRefPtr<GCancellable> m_cancellable;
#else
    RunLoop::Timer<DropTarget> m_leaveTimer;
#endif
};

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT)
