/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#include "stdafx.h"
#include "config.h"

#include "WebFrame.h"
#include "WebView.h"

#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "render_frames.h"

#include <io.h>
#include <fcntl.h>
#include <direct.h>

using namespace WebCore;

namespace WebKit {

class WebFrame::WebFramePrivate {
public:
    WebFramePrivate() { }
    ~WebFramePrivate() { }

    Frame* frame;
    FrameView* frameView;
    WebView* webView;
};

WebFrame::WebFrame(char* name, WebView* view)
: d(new WebFrame::WebFramePrivate)
{
    d->webView = view;
    Page *page = new Page();
    d->frame = new FrameWin(page, 0);
    d->frameView = new FrameView(d->frame);
    d->frame->setView(d->frameView);
    d->frameView->setWindowHandle(view->windowHandle());
}

void WebFrame::loadFilePath(char* path)
{
    char URL[2048];
    strcpy(URL, "file://localhost/");
    strcat(URL, path);

    d->frame->didOpenURL(URL);
    d->frame->begin(URL);
    HANDLE fileHandle = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
     
    bool result = false;
    DWORD bytesRead = 0;
    
    do {
      const int bufferSize = 8193;
      char buffer[bufferSize];
      result = ReadFile(fileHandle, &buffer, bufferSize - 1, &bytesRead, NULL); 
      buffer[bytesRead] = '\0';
      d->frame->write(buffer);
      // Check for end of file. 
    } while (result && bytesRead);
    
    d->frame->end();
}

void WebFrame::loadHTMLString(char *html, char *baseURL)
{
    d->frame->begin();
    d->frame->write(html);
    d->frame->end();
}

void WebFrame::paint()
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->webView->windowHandle(), &ps);
    GraphicsContext context(ps.hdc);
    d->frame->paint(&context, ps.rcPaint);
    EndPaint(d->webView->windowHandle(), &ps);
}

WebCore::Frame* WebFrame::impl()
{
    return d->frame;
}

WebCore::FrameView* WebFrame::viewImpl()
{
    return d->frameView;
}

}

