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

#include "KWQPixmap.h"
#include "xml/dom2_eventsimpl.h"

#ifdef __OBJC__
@class NSImage;
@class NSPasteboard;
#else
class NSImage;
class NSPasteboard;
#endif

typedef unsigned NSDragOperation;

class MacFrame;

class KWQClipboard : public DOM::ClipboardImpl
{
public:
    // security mechanisms
    typedef enum {
        Numb, ImageWritable, Writable, TypesReadable, Readable
    } AccessPolicy;

    KWQClipboard(bool forDragging, NSPasteboard *pasteboard, AccessPolicy policy, MacFrame *frame = 0);
    virtual ~KWQClipboard();

    bool isForDragging() const;
    
    DOM::DOMString dropEffect() const;
    void setDropEffect(const DOM::DOMString &s);
    DOM::DOMString effectAllowed() const;
    void setEffectAllowed(const DOM::DOMString &s);
    
    void clearData(const DOM::DOMString &type);
    void clearAllData();
    DOM::DOMString getData(const DOM::DOMString &type, bool &success) const;
    bool setData(const DOM::DOMString &type, const DOM::DOMString &data);
        
    // extensions beyond IE's API
    virtual QStringList types() const;

    IntPoint dragLocation() const;    // same point as client passed us
    QPixmap dragImage() const;
    void setDragImage(const QPixmap &, const IntPoint &);
    DOM::NodeImpl *dragImageElement();
    void setDragImageElement(DOM::NodeImpl *, const IntPoint &);

    // Methods for getting info in Cocoa's type system
    NSImage *dragNSImage(NSPoint *loc);    // loc converted from dragLoc, based on whole image size
    bool sourceOperation(NSDragOperation *op) const;
    bool destinationOperation(NSDragOperation *op) const;
    void setSourceOperation(NSDragOperation op);
    void setDestinationOperation(NSDragOperation op);

    void setAccessPolicy(AccessPolicy policy);
    AccessPolicy accessPolicy() const;
    void setDragHasStarted() { m_dragStarted = true; }

private:
    void setDragImage(const QPixmap &pm, DOM::NodeImpl *, const IntPoint &loc);

    NSPasteboard *m_pasteboard;
    bool m_forDragging;
    DOM::DOMString m_dropEffect;
    DOM::DOMString m_effectAllowed;
    IntPoint m_dragLoc;
    QPixmap m_dragImage;
    RefPtr<DOM::NodeImpl> m_dragImageElement;
    AccessPolicy m_policy;
    int m_changeCount;
    bool m_dragStarted;
    MacFrame *m_frame;   // used on the source side to generate dragging images
};

#endif
