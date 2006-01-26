/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "KWQPageState.h"

#import <JavaScriptCore/interpreter.h>
#import <JavaScriptCore/property_map.h>

#import "DocumentImpl.h"
#import "FrameView.h"
#import "kjs_window.h"

#import <kxmlcore/Assertions.h>
#import "KWQFoundationExtras.h"
#import "MacFrame.h"

using namespace WebCore;
using namespace KJS;

@implementation KWQPageState

- (id)initWithDocument:(DocumentImpl *)doc URL:(const KURL &)u windowProperties:(SavedProperties *)wp locationProperties:(SavedProperties *)lp interpreterBuiltins:(SavedBuiltins *)ib pausedTimeouts:(PausedTimeouts *)pt
{
    [super init];

    doc->ref();
    document = doc;
    doc->setInPageCache(YES);
    mousePressNode = static_cast<MacFrame *>(doc->frame())->mousePressNode();
    if (mousePressNode)
        mousePressNode->ref();
    URL = new KURL(u);
    windowProperties = wp;
    locationProperties = lp;
    interpreterBuiltins = ib;
    pausedTimeouts = pt;

    doc->view()->ref();

    return self;
}

- (PausedTimeouts *)pausedTimeouts
{
    return pausedTimeouts;
}

- (void)clear
{
    if (mousePressNode)
        mousePressNode->deref();        
    mousePressNode = 0;

    delete URL;
    URL = 0;

    JSLock lock;

    delete windowProperties;
    windowProperties = 0;
    delete locationProperties;
    locationProperties = 0;
    delete interpreterBuiltins;
    interpreterBuiltins = 0;

    delete pausedTimeouts;
    pausedTimeouts = 0;

    Collector::collect();
}

- (void)invalidate
{
    // Should only ever invalidate once.
    ASSERT(document);
    ASSERT(document->view());
    ASSERT(!document->inPageCache());

    if (document) {
        FrameView *view = document->view();
        if (view)
            view->deref();
        document->deref();
        document = 0;
    }

    [self clear];
}

- (void)dealloc
{
    if (document) {
        ASSERT(document->view());
        ASSERT(document->inPageCache());

        FrameView *view = document->view();

        MacFrame::clearTimers(view);

        bool detached = document->renderer() == 0;
        document->setInPageCache(NO);
        if (detached) {
            document->detach();
            document->removeAllEventListenersFromAllNodes();
        }
        document->deref();
        document = 0;
        
        if (view) {
            view->clearPart();
            view->deref();
        }
    }

    [self clear];

    [super dealloc];
}

- (void)finalize
{
    // FIXME: This work really should not be done at deallocation time.
    // We need to do it at some well-defined time instead.

    if (document) {
        ASSERT(document->inPageCache());
        ASSERT(document->view());

        FrameView *view = document->view();

        MacFrame::clearTimers(view);

        bool detached = document->renderer() == 0;
        document->setInPageCache(NO);
        if (detached) {
            document->detach();
            document->removeAllEventListenersFromAllNodes();
        }
        document->deref();
        document = 0;
        
        if (view) {
            view->clearPart();
            view->deref();
        }
    }

    [self clear];

    [super finalize];
}

- (DocumentImpl *)document
{
    return document;
}

- (NodeImpl *)mousePressNode
{
    return mousePressNode;
}

- (KURL *)URL
{
    return URL;
}

- (SavedProperties *)windowProperties
{
    return windowProperties;
}

- (SavedProperties *)locationProperties
{
    return locationProperties;
}

- (SavedBuiltins *)interpreterBuiltins
{
    return interpreterBuiltins;
}

@end
