/*
 * Copyright (C) 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "FrameWin.h"
#include "FrameView.h"
#include "Page.h"
#include "render_frames.h"
#include "DocumentImpl.h"
#include "markup.h"
#include "KWQRenderTreeDebug.h"

#include <io.h>
#include <fcntl.h>
#include <direct.h>

using namespace WebCore;

#define LOCAL_FILE_TEST 0

int main(int argc, char* argv[])
{
    Page* page = new Page();
    FrameWin* frame = new FrameWin(page, 0);
    FrameView* frameView = new FrameView(frame);
    frame->setView(frameView);
    
    frame->begin();

#if LOCAL_FILE_TEST
    char* path = "c:\\cygwin\\tmp\\test.html";
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("Failed to open file: %s\n", path);
        printf("Current path: %s\n", _getcwd(0,0));
        while(1);
        exit(1);
    }

    char buffer[4000];
    int newBytes = 0;
    while ((newBytes = fread(buffer, 1, 4000, file)) > 0) {
        frame->write(buffer, newBytes);
    }
    fclose(file);
#else
    frame->write("<html><body><p>hello world</p></body></html>");
#endif
    frame->end();

    QString markup = createMarkup(frame->document());
    printf("Source:\n\n%s", markup.ascii());
    
    QString renderDump = externalRepresentation(frame->renderer());
    printf("\n\nRenderTree:\n\n%s", renderDump.ascii());
    fflush(stdout);
    while(1);
    return 0;
}
