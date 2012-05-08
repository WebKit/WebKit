/*
 * Copyright (C) 2009 Zan Dobersek <zandobersek@gmail.com>
 * Copyright (C) 2010 Igalia S.L.
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "DumpRenderTree.h"
#include "DumpRenderTreeChrome.h"
#include "IntRect.h"
#include "PixelDumpSupportCairo.h"
#include "RefPtrCairo.h"
#include "WebCoreSupport/DumpRenderTreeSupportEfl.h"
#include <Ecore_Evas.h>
#include <Evas.h>

PassRefPtr<BitmapContext> createBitmapContextFromWebView(bool, bool, bool, bool drawSelectionRect)
{
    const Evas_Object* mainView = browser->mainView();
    Evas* evas = evas_object_evas_get(mainView);
    Ecore_Evas* ecoreEvas = ecore_evas_ecore_evas_get(evas);

    int width, height;
    evas_object_geometry_get(mainView, 0, 0, &width, &height);

    const void* pixels = ecore_evas_buffer_pixels_get(ecoreEvas);
    RefPtr<cairo_surface_t> viewSurface = adoptRef(cairo_image_surface_create_for_data(static_cast<unsigned char*>(const_cast<void*>(pixels)), CAIRO_FORMAT_ARGB32, width, height, width * 4));

    RefPtr<cairo_surface_t> imageSurface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    RefPtr<cairo_t> context = adoptRef(cairo_create(imageSurface.get()));

    cairo_set_source_surface(context.get(), viewSurface.get(), 0, 0);
    cairo_paint(context.get());

    if (drawSelectionRect) {
        const Evas_Object* mainFrame = browser->mainFrame();
        const WebCore::IntRect selectionRect = DumpRenderTreeSupportEfl::selectionRectangle(mainFrame);

        if (!selectionRect.isEmpty()) {
            cairo_set_line_width(context.get(), 1.0);
            cairo_rectangle(context.get(), selectionRect.x(), selectionRect.y(), selectionRect.width(), selectionRect.height());
            cairo_set_source_rgba(context.get(), 1.0, 0.0, 0.0, 1.0);
            cairo_stroke(context.get());
        }
    }

    return BitmapContext::createByAdoptingBitmapAndContext(0, context.release().leakRef());
}
