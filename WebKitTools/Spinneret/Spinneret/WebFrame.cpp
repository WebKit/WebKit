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
#include "Spinneret.h"

#include "Document.h"
#include "FrameView.h"
#include "FrameWin.h"
#include "GraphicsContext.h"
#include "Page.h"
#include "render_frames.h"
#include "cairo.h"
#include "cairo-win32.h"
#include "TransferJob.h"

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
    d->frame = new FrameWin(page, 0, this);
    page->setMainFrame(d->frame);
    d->frameView = new FrameView(d->frame);
    d->frame->setView(d->frameView);
    d->frameView->setWindowHandle(view->windowHandle());
}

void WebFrame::loadFilePath(char* path)
{
    char URL[2048];
    strcpy(URL, "file://localhost/");
    strncat(URL, path, 2020);

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

    CloseHandle(fileHandle);
    
    d->frame->end();
}

void WebFrame::loadHTMLString(char *html, char *baseURL)
{
    d->frame->begin();
    d->frame->write(html);
    d->frame->end();
}

void WebFrame::openURL(const DeprecatedString& str)
{
   updateLocationBar(str.ascii());
   loadURL(str.ascii());
}

void WebFrame::submitForm(const String& method, const KURL& url, const FormData* submitFormData)
{
    // FIXME: This is a dumb implementation, doesn't handle subframes, etc.
    d->frame->didOpenURL(url);
    d->frame->begin(url);
    TransferJob* job;
    if (method == "GET" || !submitFormData)
        job = new TransferJob(this, method, url);
    else
        job = new TransferJob(this, method, url, *submitFormData);
    job->start(d->frame->document()->docLoader());
}

void WebFrame::loadURL(const char* URL)
{
    d->frame->didOpenURL(URL);
    d->frame->begin(URL);
    WebCore::TransferJob* job = new TransferJob(this, "GET", URL);
    job->start(d->frame->document()->docLoader());
}
    
void WebFrame::receivedData(WebCore::TransferJob*, const char* data, int length)
{
    d->frame->write(data, length);
}

void WebFrame::receivedAllData(WebCore::TransferJob* job, WebCore::PlatformData)
{
    d->frame->end();
}

void WebFrame::paint()
{
    d->frameView->layout();

    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(d->webView->windowHandle(), &ps);
    cairo_surface_t* finalSurface = cairo_win32_surface_create(hdc);
    cairo_surface_t* surface = cairo_surface_create_similar(finalSurface,
                                                            CAIRO_CONTENT_COLOR_ALPHA,
                                                            ps.rcPaint.right, ps.rcPaint.bottom);

    cairo_t* context = cairo_create(surface);
    GraphicsContext gc(context);
    
    IntRect documentDirtyRect = ps.rcPaint;
    documentDirtyRect.move(d->frameView->contentsX(), d->frameView->contentsY());

    // FIXME: We have to set the transform using both cairo and GDI until we use cairo for text.
    HDC surfaceDC = cairo_win32_surface_get_dc(surface);
    SaveDC(surfaceDC);
    OffsetViewportOrgEx(surfaceDC, -d->frameView->contentsX(), -d->frameView->contentsY(), 0);
    cairo_translate(context, -d->frameView->contentsX(), -d->frameView->contentsY());
    d->frame->paint(&gc, documentDirtyRect);
    RestoreDC(surfaceDC, -1);

    cairo_destroy(context);
    context = cairo_create(finalSurface);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, surface, 0, 0);
    cairo_rectangle(context, 0, 0, ps.rcPaint.right, ps.rcPaint.bottom);
    cairo_fill(context);
    cairo_destroy(context);

    cairo_surface_destroy(surface);
    cairo_surface_destroy(finalSurface);

    EndPaint(d->webView->windowHandle(), &ps);
}

WebCore::Frame* WebFrame::impl()
{
    return d->frame;
}

}

