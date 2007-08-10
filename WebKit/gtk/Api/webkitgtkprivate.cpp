/*
 * Copyright (C) 2007 Holger Hans Peter Freyther
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
 * 3.  Neither the name of Apple, Inc. ("Apple") nor the names of
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

#include "webkitgtkprivate.h"
#include "NotImplemented.h"
#include "FrameLoader.h"
#include "ChromeClientGtk.h"

using namespace WebCore;

namespace WebKitGtk {
void apply(WebKitGtkSettings*, WebCore::Settings*)
{
    notImplemented();
}

WebKitGtkSettings* create(WebCore::Settings*)
{
    notImplemented();
    return 0;
}

WebKitGtkFrame* getFrameFromPage(WebKitGtkPage* page)
{
    return webkit_gtk_page_get_main_frame(page);
}

WebKitGtkPage* getPageFromFrame(WebKitGtkFrame* frame)
{
    return webkit_gtk_frame_get_page(frame);
}

WebCore::Frame* core(WebKitGtkFrame* frame)
{
    WebKitGtkFramePrivate* frame_data = WEBKIT_GTK_FRAME_GET_PRIVATE(frame);
    return frame_data->frame;
}

WebKitGtkFrame* kit(WebCore::Frame* coreFrame)
{
    FrameLoaderClientGtk* client = static_cast<FrameLoaderClientGtk*>(coreFrame->loader()->client());
    return client->webFrame();
}

WebCore::Page* core(WebKitGtkPage* page)
{
    WebKitGtkPagePrivate* page_data = WEBKIT_GTK_PAGE_GET_PRIVATE(page);
    return page_data->page;
}

WebKitGtkPage* kit(WebCore::Page* page)
{
    ChromeClientGtk* client = static_cast<ChromeClientGtk*>(page->chrome()->client());
    return client->webPage();
}
}
