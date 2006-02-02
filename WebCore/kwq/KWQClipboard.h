/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

// An implementation of the clipboard class from IE that talks to the Cocoa Pasteboard

#ifndef KWQCLIPBOARD_H_
#define KWQCLIPBOARD_H_

#include "Image.h"
#include "IntPoint.h"
#include "dom2_eventsimpl.h"

#ifdef __OBJC__
@class NSImage;
@class NSPasteboard;
#else
class NSImage;
class NSPasteboard;
#endif

typedef unsigned NSDragOperation;

class QStringList;

namespace WebCore {

class MacFrame;

class KWQClipboard : public ClipboardImpl, public CachedObjectClient {
public:
    // security mechanisms
    typedef enum {
        Numb, ImageWritable, Writable, TypesReadable, Readable
    } AccessPolicy;

    KWQClipboard(bool forDragging, NSPasteboard *pasteboard, AccessPolicy policy, MacFrame *frame = 0);
    virtual ~KWQClipboard();

    bool isForDragging() const;
    
    DOMString dropEffect() const;
    void setDropEffect(const DOMString &s);
    DOMString effectAllowed() const;
    void setEffectAllowed(const DOMString &s);
    
    void clearData(const DOMString &type);
    void clearAllData();
    DOMString getData(const DOMString &type, bool &success) const;
    bool setData(const DOMString &type, const DOMString &data);
        
    // extensions beyond IE's API
    virtual QStringList types() const;

    IntPoint dragLocation() const;    // same point as client passed us
    CachedImage* dragImage() const;
    void setDragImage(CachedImage*, const IntPoint &);
    NodeImpl *dragImageElement();
    void setDragImageElement(NodeImpl *, const IntPoint &);

#if __APPLE__
    // Methods for getting info in Cocoa's type system
    NSImage *dragNSImage(NSPoint *loc);    // loc converted from dragLoc, based on whole image size
    bool sourceOperation(NSDragOperation *op) const;
    bool destinationOperation(NSDragOperation *op) const;
    void setSourceOperation(NSDragOperation op);
    void setDestinationOperation(NSDragOperation op);
#endif

    void setAccessPolicy(AccessPolicy policy);
    AccessPolicy accessPolicy() const;
    void setDragHasStarted() { m_dragStarted = true; }
    
private:
    void setDragImage(CachedImage* cachedImage, NodeImpl *, const IntPoint &loc);

    NSPasteboard *m_pasteboard;
    bool m_forDragging;
    DOMString m_dropEffect;
    DOMString m_effectAllowed;
    IntPoint m_dragLoc;
    CachedImage* m_dragImage;
    RefPtr<NodeImpl> m_dragImageElement;
    AccessPolicy m_policy;
    int m_changeCount;
    bool m_dragStarted;
    MacFrame *m_frame;   // used on the source side to generate dragging images
};

}

// FIXME: Remove when everything is in the WebCore namespace.
using WebCore::KWQClipboard;

#endif
