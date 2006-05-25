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
#include "Document.h"
#include "markup.h"
#include "RenderTreeAsText.h"

#include <io.h>
#include <fcntl.h>
#include <direct.h>

using namespace WebCore;

static void localFileTest(FrameWin* frame, char* path)
{
    frame->begin();
    FILE* file = fopen(path, "rb");
    if (!file) {
        fprintf(stderr, "Failed to open file: %s\n", path);
        return;
    }

    char buffer[4000];
    int newBytes = 0;
    while ((newBytes = fread(buffer, 1, 4000, file)) > 0) {
        frame->write(buffer, newBytes);
    }
    fclose(file);
    frame->end();
}

static void dumpRenderTreeMain(FrameWin* frame)
{
    char filenameBuffer[2048];
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char *newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';
        
        if (!*filenameBuffer)
            continue;
            
        localFileTest(frame, filenameBuffer);
        DeprecatedString renderDump = externalRepresentation(frame->renderer());
        puts(renderDump.ascii());
        puts("#EOF\n");

        fflush(stdout);
    }
}

static void dumpRenderTreeToStdOut(FrameWin* frame)
{
    DeprecatedString renderDump = externalRepresentation(frame->renderer());
    printf("\n\nRenderTree:\n\n%s", renderDump.ascii());
}

static void serializeToStdOut(FrameWin* frame)
{
    DeprecatedString markup = createMarkup(frame->document());
    printf("Source:\n\n%s", markup.ascii());
}

int main(int argc, char* argv[])
{
    Page* page = new Page();
    FrameWin* frame = new FrameWin(page, 0, 0);
    FrameView* frameView = new FrameView(frame);
    frame->setView(frameView);
    
    if (argc == 2 && strcmp(argv[1], "-") == 0)
        dumpRenderTreeMain(frame);
    else {
        // Default file for debugging in visual studio
        char *fileToTest = "c:\\cygwin\\tmp\\test.html";
        if (argc == 2)
            fileToTest = argv[1];
        localFileTest(frame, fileToTest);
        serializeToStdOut(frame);
        dumpRenderTreeToStdOut(frame);
        if (argc != 2) {
            // Keep window open for visual studio debugging.
            fflush(stdout);
            while(1);
        }
    }

    return 0;
}
