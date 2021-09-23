/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DrawingAreaProxyCoordinatedGraphics.h"
#include "WebPageInspectorEmulationAgent.h"
#include "WebPageProxy.h"
#include <WebCore/IntSize.h>
#include <gtk/gtk.h>

namespace WebKit {
void WebPageInspectorEmulationAgent::platformSetSize(int width, int height, Function<void (const String& error)>&& callback)
{
    GtkWidget* viewWidget = m_page.viewWidget();
    GtkWidget* window = gtk_widget_get_toplevel(viewWidget);
    if (!window) {
        callback("Cannot find parent window"_s);
        return;
    }
    if (!GTK_IS_WINDOW(window)) {
        callback("Toplevel is not a window"_s);
        return;
    }
    GtkAllocation viewAllocation;
    gtk_widget_get_allocation(viewWidget, &viewAllocation);
    if (viewAllocation.width == width && viewAllocation.height == height) {
        callback(String());
        return;
    }

    GtkAllocation windowAllocation;
    gtk_widget_get_allocation(window, &windowAllocation);

    width += windowAllocation.width - viewAllocation.width;
    height += windowAllocation.height - viewAllocation.height;

    if (auto* drawingArea = static_cast<DrawingAreaProxyCoordinatedGraphics*>(m_page.drawingArea())) {
        drawingArea->waitForSizeUpdate([callback = WTFMove(callback)]() {
            callback(String());
        });
    } else {
        callback("No backing store for window"_s);
    }
    gtk_window_resize(GTK_WINDOW(window), width, height);
}

} // namespace WebKit
