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

#import <Foundation/Foundation.h>

#include <qmap.h>

#include "kjs_window.h"
#include "dom_docimpl.h"

class KURL;

namespace DOM {
    class DocumentImpl;
}

namespace KJS {
    class SavedProperties;
}

@interface KWQPageState : NSObject
{
    DOM::DocumentImpl *document;
    DOM::NodeImpl *mousePressNode;
    KURL *URL;
    KJS::SavedProperties *windowProperties;
    KJS::SavedProperties *locationProperties;
    KJS::SavedBuiltins *interpreterBuiltins;
    QMap<int, KJS::ScheduledAction*> *pausedActions;
    DOM::DocumentImpl::ParseMode parseMode;
}

- initWithDocument:(DOM::DocumentImpl *)doc URL:(const KURL &)u windowProperties:(KJS::SavedProperties *)wp locationProperties:(KJS::SavedProperties *)lp interpreterBuiltins:(KJS::SavedBuiltins *)ib;

- (DOM::DocumentImpl *)document;
- (DOM::DocumentImpl::ParseMode)parseMode;
- (DOM::NodeImpl *)mousePressNode;
- (KURL *)URL;
- (KJS::SavedProperties *)windowProperties;
- (KJS::SavedProperties *)locationProperties;
- (KJS::SavedBuiltins *)interpreterBuiltins;
- (void)setPausedActions: (QMap<int, KJS::ScheduledAction*> *)pa;
- (QMap<int, KJS::ScheduledAction*> *)pausedActions;
- (void)invalidate;

@end
