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

#include "config.h"
#import "KWQClipboard.h"
#import <kxmlcore/Assertions.h>
#import "KWQFoundationExtras.h"
#import "MacFrame.h"
#import "KWQStringList.h"
#import "WebCoreGraphicsBridge.h"
#import "WebCoreImageRenderer.h"

#import <AppKit/AppKit.h>

using DOM::DOMString;
using DOM::NodeImpl;

namespace WebCore {

KWQClipboard::KWQClipboard(bool forDragging, NSPasteboard *pasteboard, AccessPolicy policy, MacFrame *frame)
  : m_pasteboard(KWQRetain(pasteboard)), m_forDragging(forDragging),
    m_policy(policy), m_dragStarted(false), m_frame(frame)
{
    m_changeCount = [m_pasteboard changeCount];
}

KWQClipboard::~KWQClipboard()
{
    KWQRelease(m_pasteboard);
}

bool KWQClipboard::isForDragging() const
{
    return m_forDragging;
}

void KWQClipboard::setAccessPolicy(AccessPolicy policy)
{
    // once you go numb, can never go back
    ASSERT(m_policy != Numb || policy == Numb);
    m_policy = policy;
}

KWQClipboard::AccessPolicy KWQClipboard::accessPolicy() const
{
    return m_policy;
}

static NSString *cocoaTypeFromMIMEType(const DOMString &type)
{
    QString qType = type.qstring().stripWhiteSpace();

    // two special cases for IE compatibility
    if (qType == "Text") {
        return NSStringPboardType;
    }
    if (qType == "URL") {
        return NSURLPboardType;
    } 

    // Ignore any trailing charset - JS strings are Unicode, which encapsulates the charset issue
    if (qType == "text/plain" || qType.startsWith("text/plain;")) {
        return NSStringPboardType;
    }
    if (qType == "text/uri-list") {
        // special case because UTI doesn't work with Cocoa's URL type
        return NSURLPboardType; // note special case in getData to read NSFilenamesType
    }
    
    // Try UTI now
    NSString *mimeType = qType.getNSString();
    CFStringRef UTIType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, (CFStringRef)mimeType, NULL);
    if (UTIType) {
        CFStringRef pbType = UTTypeCopyPreferredTagWithClass(UTIType, kUTTagClassNSPboardType);
        CFRelease(UTIType);
        if (pbType) {
            return KWQCFAutorelease(pbType);
        }
    }

   // No mapping, just pass the whole string though
    return qType.getNSString();
}

static QString MIMETypeFromCocoaType(NSString *type)
{
    // UTI may not do these right, so make sure we get the right, predictable result
    if ([type isEqualToString:NSStringPboardType]) {
        return QString("text/plain");
    }
    if ([type isEqualToString:NSURLPboardType] || [type isEqualToString:NSFilenamesPboardType]) {
        return QString("text/uri-list");
    }
    
    // Now try the general UTI mechanism
    CFStringRef UTIType = UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, (CFStringRef)type, NULL);
    if (UTIType) {
        CFStringRef mimeType = UTTypeCopyPreferredTagWithClass(UTIType, kUTTagClassMIMEType);
        CFRelease(UTIType);
        if (mimeType) {
            QString result = QString::fromCFString(mimeType);
            CFRelease(mimeType);
            return result;
        }
    }

    // No mapping, just pass the whole string though
    return QString::fromNSString(type);
}

void KWQClipboard::clearData(const DOMString &type)
{
    if (m_policy != Writable) {
        return;
    }
    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    if (cocoaType) {
        [m_pasteboard setString:@"" forType:cocoaType];
    }
}

void KWQClipboard::clearAllData()
{
    if (m_policy != Writable) {
        return;
    }
    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    [m_pasteboard declareTypes:[NSArray array] owner:nil];
}

DOMString KWQClipboard::getData(const DOMString &type, bool &success) const
{
    success = false;
    if (m_policy != Readable) {
        return DOMString();
    }
    
    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    NSString *cocoaValue = nil;
    NSArray *availableTypes = [m_pasteboard types];

    // Fetch the data in different ways for the different Cocoa types

    if ([cocoaType isEqualToString:NSURLPboardType]) {
        // When both URL and filenames are present, filenames is superior since it can contain a list.
        // must check this or we get a printf from CF when there's no data of this type
        if ([availableTypes containsObject:NSFilenamesPboardType]) {
            NSArray *fileList = [m_pasteboard propertyListForType:NSFilenamesPboardType];
            if (fileList && [fileList isKindOfClass:[NSArray class]]) {
                unsigned count = [fileList count];
                if (count > 0) {
                    if (type != "text/uri-list")
                        count = 1;
                    NSMutableString *URLs = [NSMutableString string];
                    unsigned i;
                    for (i = 0; i < count; i++) {
                        if (i > 0) {
                            [URLs appendString:@"\n"];
                        }
                        NSString *string = [fileList objectAtIndex:i];
                        if (![string isKindOfClass:[NSString class]])
                            break;
                        NSURL *URL = [NSURL fileURLWithPath:string];
                        [URLs appendString:[URL absoluteString]];
                    }
                    if (i == count)
                        cocoaValue = URLs;
                }
            }
        }
        if (!cocoaValue) {
            // must check this or we get a printf from CF when there's no data of this type
            if ([availableTypes containsObject:NSURLPboardType]) {
                NSURL *url = [NSURL URLFromPasteboard:m_pasteboard];
                if (url) {
                    cocoaValue = [url absoluteString];
                }
            }
        }
    } else if (cocoaType) {        
        cocoaValue = [m_pasteboard stringForType:cocoaType];
    }

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (cocoaValue && m_changeCount == [m_pasteboard changeCount]) {
        success = true;
        return DOMString(QString::fromNSString(cocoaValue));
    }

    return DOMString();
}

bool KWQClipboard::setData(const DOMString &type, const DOMString &data)
{
    if (m_policy != Writable) {
        return false;
    }
    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    NSString *cocoaData = data.qstring().getNSString();

    if ([cocoaType isEqualToString:NSURLPboardType]) {
        [m_pasteboard addTypes:[NSArray arrayWithObject:NSURLPboardType] owner:nil];
        NSURL *url = [[NSURL alloc] initWithString:cocoaData];
        [url writeToPasteboard:m_pasteboard];

        if ([url isFileURL]) {
            [m_pasteboard addTypes:[NSArray arrayWithObject:NSFilenamesPboardType] owner:nil];
            NSArray *fileList = [NSArray arrayWithObject:[url path]];
            [m_pasteboard setPropertyList:fileList forType:NSFilenamesPboardType];
        }

        [url release];
        return true;
    }

    if (cocoaType) {
        // everything else we know of goes on the pboard as a string
        [m_pasteboard addTypes:[NSArray arrayWithObject:cocoaType] owner:nil];
        return [m_pasteboard setString:cocoaData forType:cocoaType];
    }

    return false;
}

QStringList KWQClipboard::types() const
{
    if (m_policy != Readable && m_policy != TypesReadable) {
        return QStringList();
    }

    NSArray *types = [m_pasteboard types];

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != [m_pasteboard changeCount]) {
        return QStringList();
    }

    QStringList result;
    if (types) {
        unsigned count = [types count];
        unsigned i;
        for (i = 0; i < count; i++) {
            NSString *pbType = [types objectAtIndex:i];
            if ([pbType isEqualToString:@"NeXT plain ascii pasteboard type"]) {
                continue;   // skip this ancient type that gets auto-supplied by some system conversion
            }

            QString qstr = MIMETypeFromCocoaType(pbType);
            if (!result.contains(qstr)) {
                result.append(qstr);
            }
        }
    }
    return result;
}

// The rest of these getters don't really have any impact on security, so for now make no checks

IntPoint KWQClipboard::dragLocation() const
{
    return m_dragLoc;
}

Image KWQClipboard::dragImage() const
{
    return m_dragImage;
}

void KWQClipboard::setDragImage(const Image &pm, const IntPoint &loc)
{
    setDragImage(pm, 0, loc);
}

DOM::NodeImpl *KWQClipboard::dragImageElement()
{
    return m_dragImageElement.get();
}

void KWQClipboard::setDragImageElement(NodeImpl *node, const IntPoint &loc)
{
    setDragImage(Image(), node, loc);
}

void KWQClipboard::setDragImage(const Image &pm, NodeImpl *node, const IntPoint &loc)
{
    if (m_policy == ImageWritable || m_policy == Writable) {
        m_dragImage = pm;
        m_dragLoc = loc;
        m_dragImageElement = node;
        
        if (m_dragStarted && m_changeCount == [m_pasteboard changeCount]) {
            NSPoint cocoaLoc;
            NSImage *cocoaImage = dragNSImage(&cocoaLoc);
            if (cocoaImage) {
                [[WebCoreGraphicsBridge sharedBridge] setDraggingImage:cocoaImage at:cocoaLoc];
            }
        }
        // Else either 1) we haven't started dragging yet, so we rely on the part to install this drag image
        // as part of getting the drag kicked off, or 2) Someone kept a ref to the clipboard and is trying to
        // set the image way too late.
    }
}

NSImage *KWQClipboard::dragNSImage(NSPoint *loc)
{
    NSImage *result = nil;
    if (m_dragImageElement) {
        if (m_frame) {
            NSRect imageRect;
            NSRect elementRect;
            result = m_frame->snapshotDragImage(m_dragImageElement.get(), &imageRect, &elementRect);
            if (loc) {
                // Client specifies point relative to element, not the whole image, which may include child
                // layers spread out all over the place.
                loc->x = elementRect.origin.x - imageRect.origin.x + m_dragLoc.x();
                loc->y = elementRect.origin.y - imageRect.origin.y + m_dragLoc.y();
                loc->y = imageRect.size.height - loc->y;
            }
        }
    } else {
        result = [m_dragImage.imageRenderer() image];
        if (loc) {
            *loc = m_dragLoc;
            loc->y = [result size].height - loc->y;
        }
    }
    return result;
}

DOM::DOMString KWQClipboard::dropEffect() const
{
    return m_dropEffect;
}

void KWQClipboard::setDropEffect(const DOM::DOMString &s)
{
    if (m_policy == Readable || m_policy == TypesReadable) {
        m_dropEffect = s;
    }
}

DOM::DOMString KWQClipboard::effectAllowed() const
{
    return m_effectAllowed;
}

void KWQClipboard::setEffectAllowed(const DOM::DOMString &s)
{
    if (m_policy == Writable) {
        m_effectAllowed = s;
    }
}

// These "conversion" methods are called by the bridge and part, and never make sense to JS, so we don't
// worry about security for these.  The don't allow access to the pasteboard anyway.

static NSDragOperation cocoaOpFromIEOp(const DOMString &op) {
    // yep, it's really just this fixed set
    if (op == "none") {
        return NSDragOperationNone;
    } else if (op == "copy") {
        return NSDragOperationCopy;
    } else if (op == "link") {
        return NSDragOperationLink;
    } else if (op == "move") {
        return NSDragOperationGeneric;
    } else if (op == "copyLink") {
        return NSDragOperationCopy | NSDragOperationLink;
    } else if (op == "copyMove") {
        return NSDragOperationCopy | NSDragOperationGeneric | NSDragOperationMove;
    } else if (op == "linkMove") {
        return NSDragOperationLink | NSDragOperationGeneric | NSDragOperationMove;
    } else if (op == "all") {
        return NSDragOperationEvery;
    } else {
        return NSDragOperationPrivate;  // really a marker for "no conversion"
    }
}

static const DOMString IEOpFromCocoaOp(NSDragOperation op) {
    bool moveSet = ((NSDragOperationGeneric | NSDragOperationMove) & op) != 0;
    
    if ((moveSet && (op & NSDragOperationCopy) && (op & NSDragOperationLink))
        || (op == NSDragOperationEvery)) {
        return "all";
    } else if (moveSet && (op & NSDragOperationCopy)) {
        return "copyMove";
    } else if (moveSet && (op & NSDragOperationLink)) {
        return "linkMove";
    } else if ((op & NSDragOperationCopy) && (op & NSDragOperationLink)) {
        return "copyLink";
    } else if (moveSet) {
        return "move";
    } else if (op & NSDragOperationCopy) {
        return "copy";
    } else if (op & NSDragOperationLink) {
        return "link";
    } else {
        return "none";
    }
}

bool KWQClipboard::sourceOperation(NSDragOperation *op) const
{
    if (m_effectAllowed.isNull()) {
        return false;
    } else {
        *op = cocoaOpFromIEOp(m_effectAllowed);
        return true;
    }
}

bool KWQClipboard::destinationOperation(NSDragOperation *op) const
{
    if (m_dropEffect.isNull()) {
        return false;
    } else {
        *op = cocoaOpFromIEOp(m_dropEffect);
        return true;
    }
}

void KWQClipboard::setSourceOperation(NSDragOperation op)
{
    m_effectAllowed = IEOpFromCocoaOp(op);
}

void KWQClipboard::setDestinationOperation(NSDragOperation op)
{
    m_dropEffect = IEOpFromCocoaOp(op);
}

}
