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

#import "KWQPageState.h"

#import <JavaScriptCore/property_map.h>

#import "dom_docimpl.h"
#import "khtmlview.h"
#import "kjs_window.h"

#import "KWQAssertions.h"
#import "KWQKHTMLPart.h"

using DOM::DocumentImpl;

using khtml::RenderObject;

using KJS::SavedProperties;
using KJS::SavedBuiltins;

@implementation KWQPageState

- initWithDocument:(DocumentImpl *)doc URL:(const KURL &)u windowProperties:(SavedProperties *)wp locationProperties:(SavedProperties *)lp interpreterBuiltins:(SavedBuiltins *)ib
{
    [super init];
    doc->ref();
    document = doc;
    document->setInPageCache(YES);
    document->view()->ref();
    URL = new KURL(u);
    windowProperties = wp;
    locationProperties = lp;
    interpreterBuiltins = ib;
    return self;
}

- (void)setPausedActions: (QMap<int, KJS::ScheduledAction*> *)pa
{
    pausedActions = pa;
}

- (QMap<int, KJS::ScheduledAction*> *)pausedActions
{
    return pausedActions;
}

- (void)_cleanupPausedActions
{
    if (pausedActions){
        QMapIterator<int,KJS::ScheduledAction*> it;
        for (it = pausedActions->begin(); it != pausedActions->end(); ++it) {
            KJS::ScheduledAction *action = *it;
            delete action;
        }
        delete pausedActions;
        pausedActions = 0;
    }
    QObject::clearPausedTimers(self);
}

- (void)clear
{
    document = 0;

    delete URL;
    URL = 0;
    delete windowProperties;
    windowProperties = 0;
    delete locationProperties;
    locationProperties = 0;
    delete interpreterBuiltins;
    interpreterBuiltins = 0;
    [self _cleanupPausedActions];
}

- (void)invalidate
{
    // Should only ever invalidate once.
    ASSERT(document);
    ASSERT(!document->inPageCache());

    document->view()->deref();
    document->deref();

    [self clear];
}

- (void)dealloc
{
    if (document) {
        ASSERT(document->inPageCache());
        ASSERT(document->view());

        KHTMLView *view = document->view();

        KWQKHTMLPart::clearTimers(view);

        bool detached = document->renderer() == 0;
        document->setInPageCache(NO);
        if (detached) {
            document->detach();
        }
        document->deref();
        
        if (view) {
            view->clearPart();
            view->deref();
        }
    }

    [self clear];

    [super dealloc];
}

- (DocumentImpl *)document
{
    return document;
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
