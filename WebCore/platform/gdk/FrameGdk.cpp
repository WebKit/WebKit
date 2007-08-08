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
