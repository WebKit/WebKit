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
#include "ChromeClientGtk.h"
#include "FrameLoader.h"
#include "FrameLoaderClientGtk.h"
#include "NotImplemented.h"

using namespace WebCore;

namespace WebKit {
void apply(WebKitSettings*, WebCore::Settings*)
{
    notImplemented();
}

WebKitSettings* create(WebCore::Settings*)
{
    notImplemented();
    return 0;
}

WebKitFrame* getFrameFromPage(WebKitPage* page)
{
    return webkit_page_get_main_frame(page);
}

WebKitPage* getPageFromFrame(WebKitFrame* frame)
{
    return webkit_frame_get_page(frame);
}

WebCore::Frame* core(WebKitFrame* frame)
{
    WebKitFramePrivate* frame_data = WEBKIT_FRAME_GET_PRIVATE(frame);
    return frame_data->frame;
}

WebKitFrame* kit(WebCore::Frame* coreFrame)
{
    WebKit::FrameLoaderClient* client = static_cast<WebKit::FrameLoaderClient*>(coreFrame->loader()->client());
    return client->webFrame();
}

WebCore::Page* core(WebKitPage* page)
{
    WebKitPagePrivate* page_data = WEBKIT_PAGE_GET_PRIVATE(page);
    return page_data->page;
}

WebKitPage* kit(WebCore::Page* page)
{
    WebKit::ChromeClient* client = static_cast<WebKit::ChromeClient*>(page->chrome()->client());
    return client->webPage();
}
}
