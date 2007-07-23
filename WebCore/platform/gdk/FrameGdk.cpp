/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Michael Emmel mike.emmel@gmail.com 
 * Copyright (C) 2007 Holger Hans Peter Freyther
 * All rights reserved.
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

#include "config.h"
#include "FrameGdk.h"

#include "CString.h"
#include "Document.h"
#include "DocumentLoader.h"
#include "EventHandler.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGdk.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "KeyboardCodes.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformKeyboardEvent.h"
#include "PlatformMouseEvent.h"
#include "PlatformString.h"
#include "PlatformWheelEvent.h"
#include "RenderObject.h"
#include "RenderTreeAsText.h"
#include "ResourceHandle.h"
#include "ResourceResponse.h"
#include "SelectionController.h"
#include "Settings.h"
#include "TypingCommand.h"
#include "webkitgtkframe.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

// This function loads resources from WebKit
// This does not belong here and I'm not sure where
// it should go
// I don't know what the plans or design is
// for none code resources
Vector<char> loadResourceIntoArray(const char* resourceName)
{
    Vector<char> resource;
    //if (strcmp(resourceName,"missingImage") == 0) {
    //}
    return resource;
}

struct FrameGdkExposeData {
    GtkContainer *container;
    GdkEventExpose *expose;
};

static void frame_gdk_expose_child(GtkWidget *widget, gpointer _data)
{
    FrameGdkExposeData* data = (FrameGdkExposeData*)_data;
    int width, height;
    gtk_container_propagate_expose(data->container, widget, data->expose);
} 

namespace WebCore {

FrameGdk::FrameGdk(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClientGdk* frameLoader)
    : Frame(page, ownerElement, frameLoader)
{
    Settings* settings = page->settings();
    settings->setLoadsImagesAutomatically(true);
    settings->setMinimumFontSize(5);
    settings->setMinimumLogicalFontSize(5);
    settings->setShouldPrintBackgrounds(true);
    settings->setJavaScriptEnabled(true);

    settings->setDefaultFixedFontSize(14);
    settings->setDefaultFontSize(14);
    settings->setSerifFontFamily("Times New Roman");
    settings->setSansSerifFontFamily("Arial");
    settings->setFixedFontFamily("Courier");
    settings->setStandardFontFamily("Arial");
}

FrameGdk::~FrameGdk()
{
    loader()->cancelAndClear();
}

void FrameGdk::dumpRenderTree() const
{
    if (view()->needsLayout())
        view()->layout();
    
    String txt = externalRepresentation(renderer());
    CString utf8Str = txt.utf8();
    const char *utf8 = utf8Str.data();
    if (utf8)
        printf("%s\n", utf8);
    else
        printf("FrameGdk::dumpRenderTree() no data\n");
}

bool FrameGdk::keyPress(const PlatformKeyboardEvent& keyEvent)
{
    if (!eventHandler())
        return false;

    return eventHandler()->keyEvent(keyEvent);
}

void FrameGdk::handleGdkEvent(GdkEvent* event)
{
    switch (event->type) {

        case GDK_EXPOSE: {
            GdkRectangle clip;
            gdk_region_get_clipbox(event->expose.region, &clip);
            gdk_window_begin_paint_region(event->any.window, event->expose.region);
            cairo_t* cr = gdk_cairo_create(event->any.window);
            GraphicsContext ctx(cr);
            ctx.setGdkDrawable(event->any.window);
            if (renderer()) {
                if (view()->needsLayout())
                    view()->layout();
                IntRect rect(clip.x, clip.y, clip.width, clip.height);
                paint(&ctx, rect);
            }
            cairo_destroy(cr);

            /*
             * Make sure children of the view get redrawn
             */
            FrameGdkExposeData data = { GTK_CONTAINER(view()->gtkWidget()), &event->expose };
            gtk_container_forall(GTK_CONTAINER(view()->gtkWidget()), frame_gdk_expose_child, &data);
            gdk_window_end_paint(event->any.window);
            
            break;
        }

        case GDK_CONFIGURE: {
            view()->updateGeometry(event->configure.width, event->configure.height);
            forceLayout();
            break;
        }

        case GDK_SCROLL: {
            PlatformWheelEvent wheelEvent(event);
            view()->wheelEvent(wheelEvent);
            if (wheelEvent.isAccepted())
                return;

            HitTestRequest hitTestRequest(true, true);
            HitTestResult hitTestResult(wheelEvent.pos());
            renderer()->layer()->hitTest(hitTestRequest, hitTestResult);
            Node* node = hitTestResult.innerNode();
            if (!node)
                return;
            /*
             * FIXME: Does this belong here?
             * Default to scrolling the page
             * not sure why its null
             * broke anyway when its not null
             * doScroll(renderer(), wheelEvent.deltaX(), wheelEvent.deltaY());
             */
            break;
        }
        case GDK_DRAG_ENTER:
        case GDK_DRAG_LEAVE:
        case GDK_DRAG_MOTION:
        case GDK_DRAG_STATUS:
        case GDK_DROP_START:
        case GDK_DROP_FINISHED: {
            //bool updateDragAndDrop(const PlatformMouseEvent&, Clipboard*);
            //void cancelDragAndDrop(const PlatformMouseEvent&, Clipboard*);
            //bool performDragAndDrop(const PlatformMouseEvent&, Clipboard*);
            break;
        }
        case GDK_MOTION_NOTIFY:
            eventHandler()->mouseMoved(PlatformMouseEvent(event));
            break;
        case GDK_BUTTON_PRESS:
        case GDK_2BUTTON_PRESS:
        case GDK_3BUTTON_PRESS:
            eventHandler()->handleMousePressEvent(PlatformMouseEvent(event));
            break;
        case GDK_BUTTON_RELEASE:
            eventHandler()->handleMouseReleaseEvent(PlatformMouseEvent(event));
            break;
        case GDK_KEY_PRESS:
        case GDK_KEY_RELEASE: {
            PlatformKeyboardEvent keyEvent(event);
            keyPress(keyEvent);
        }
        default:
            break;
    }
}

void Frame::issueTransposeCommand()
{
    notImplemented();
}

void Frame::cleanupPlatformScriptObjects()
{
    notImplemented();
}

DragImageRef Frame::dragImageForSelection() 
{
    notImplemented();
    return 0;
}

void Frame::dashboardRegionsChanged()
{
    notImplemented();
}

}
