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

#import "KWQClipboard.h"

using DOM::DOMString;

KWQClipboard::KWQClipboard(bool forDragging, NSPasteboard *pasteboard)
  : m_pasteboard([pasteboard retain]), m_forDragging(forDragging)
{
}

KWQClipboard::~KWQClipboard()
{
    [m_pasteboard release];
}

bool KWQClipboard::isForDragging() const
{
    return m_forDragging;
}

DOM::DOMString KWQClipboard::dropEffect() const
{
    return m_dropEffect;
}

void KWQClipboard::setDropEffect(const DOM::DOMString &s)
{
    m_dropEffect = s;
}

DOM::DOMString KWQClipboard::dropAllowed() const
{
    return m_dropAllowed;
}

void KWQClipboard::setDropAllowed(const DOM::DOMString &s)
{
    m_dropAllowed = s;
}

// FIXME hardwired for now, will use UTI
static NSString *cocoaTypeFromMIMEType(const DOMString &type) {
    QString qType = type.string();

    // two special cases for IE compatibility
    if (qType == "Text") {
        return NSStringPboardType;
    } else if (qType == "URL") {
        return NSURLPboardType;
    } 
    
    // Cut off any trailing charset - JS String are Unicode, which encapsulates this issue
    int semicolon = qType.find(';');
    if (semicolon >= 0) {
        qType = qType.left(semicolon+1);
    }
    qType = qType.stripWhiteSpace();

    if (!qType.compare("text/plain")) {
        return NSStringPboardType;
    } else if (!qType.compare("text/html")) {
        return NSHTMLPboardType;
    } else if (!qType.compare("text/rtf")) {
        return NSRTFPboardType;
    } else if (!qType.compare("text/uri-list")) {
        return NSURLPboardType;     // note fallback to NSFilenamesPboardType in caller
    } else {
        // FIXME - Better fallback for application/Foo might be to take Foo
        return qType.getNSString();
    }
}

void KWQClipboard::clearData(const DOMString &type)
{    
}

void KWQClipboard::clearAllData()
{
}

DOMString KWQClipboard::getData(const DOMString &type, bool &success) const
{
    success = false;
    NSString *cocoaType = cocoaTypeFromMIMEType(type);
    NSString *cocoaValue = nil;
    NSArray *availableTypes = [m_pasteboard types];

    // Fetch the data in different ways for the different Cocoa types
    if (cocoaType == NSURLPboardType) {
        NSURL *url = nil;
        // must check this or we get a printf from CF when there's no data of this type
        if ([availableTypes containsObject:NSURLPboardType]) {
            url = [NSURL URLFromPasteboard:m_pasteboard];
        }
        if (url) {
            cocoaValue = [url absoluteString];
        } else {
            // Try Filenames type (in addition to URL type) when client requests text/uri-list
            cocoaType = NSFilenamesPboardType;
            cocoaValue = nil;
        }
    }

    if (cocoaType == NSFilenamesPboardType) {
        NSArray *fileList = nil;
        // must check this or we get a printf from CF when there's no data of this type
        if ([availableTypes containsObject:NSFilenamesPboardType]) {
            fileList = [m_pasteboard propertyListForType:cocoaType];
        }
        if (fileList && [fileList count] > 0) {
            NSMutableString *urls = [NSMutableString string];
            unsigned i;
            for (i = 0; i < [fileList count]; i++) {
                if (i > 0) {
                    [urls appendString:@"\n"];
                }
                NSURL *url = [NSURL fileURLWithPath:[fileList objectAtIndex:i]];
                [urls appendString:[url absoluteString]];
            }
            cocoaValue = urls;
        } else {
            cocoaValue = nil;
        }
    } else if (!cocoaValue) {        
        cocoaValue = [m_pasteboard stringForType:cocoaType];
    }

    if (cocoaValue) {
        success = true;
        return DOMString(QString::fromNSString(cocoaValue));
    } else {
        return DOMString();
    }
}

bool KWQClipboard::setData(const DOMString &type, const DOMString &data)
{
    return true;
}
