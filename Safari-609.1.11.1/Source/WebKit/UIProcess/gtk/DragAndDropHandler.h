/*
 * Copyright (C) 2014 Igalia S.L.
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

#pragma once

#if ENABLE(DRAG_SUPPORT)

#include <WebCore/DragActions.h>
#include <WebCore/IntPoint.h>
#include <WebCore/SelectionData.h>
#include <gtk/gtk.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _GdkDragContext GdkDragContext;
typedef struct _GtkSelectionData GtkSelectionData;

namespace WebCore {
class DragData;
class SelectionData;
}

namespace WebKit {

class ShareableBitmap;
class WebPageProxy;

class DragAndDropHandler {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(DragAndDropHandler);
public:
    DragAndDropHandler(WebPageProxy&);

    void startDrag(Ref<WebCore::SelectionData>&&, WebCore::DragOperation, RefPtr<ShareableBitmap>&& dragImage);
    void fillDragData(GdkDragContext*, GtkSelectionData*, unsigned info);
    void finishDrag(GdkDragContext*);

    void dragEntered(GdkDragContext*, GtkSelectionData*, unsigned info, unsigned time);
    void dragMotion(GdkDragContext*, const WebCore::IntPoint& position, unsigned time);
    void dragLeave(GdkDragContext*);
    bool drop(GdkDragContext*, const WebCore::IntPoint& position, unsigned time);

private:
    struct DroppingContext {
        WTF_MAKE_STRUCT_FAST_ALLOCATED;
        DroppingContext(GdkDragContext*, const WebCore::IntPoint& position);

        GdkDragContext* gdkContext { nullptr };
        WebCore::IntPoint lastMotionPosition;
        Ref<WebCore::SelectionData> selectionData;
        unsigned pendingDataRequests { 0 };
        bool dropHappened { false };
    };

    WebCore::SelectionData* dropDataSelection(GdkDragContext*, GtkSelectionData*, unsigned info, WebCore::IntPoint& position);
    WebCore::SelectionData* dragDataSelection(GdkDragContext*, const WebCore::IntPoint& position, unsigned time);

    WebPageProxy& m_page;
    HashMap<GdkDragContext*, std::unique_ptr<DroppingContext>> m_droppingContexts;
    GRefPtr<GdkDragContext> m_dragContext;
    RefPtr<WebCore::SelectionData> m_draggingSelectionData;
};

} // namespace WebKit

#endif // ENABLE(DRAG_SUPPORT)
