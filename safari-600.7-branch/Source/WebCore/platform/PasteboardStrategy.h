/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PasteboardStrategy_h
#define PasteboardStrategy_h

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class Color;
class SharedBuffer;
class URL;
struct PasteboardImage;
struct PasteboardWebContent;

class PasteboardStrategy {
public:
#if PLATFORM(IOS)
    // FIXME: We should move Mac to this.
    virtual void writeToPasteboard(const PasteboardWebContent&) = 0;
    virtual void writeToPasteboard(const PasteboardImage&) = 0;
    virtual void writeToPasteboard(const String& pasteboardType, const String&) = 0;
    virtual int getPasteboardItemsCount() = 0;
    virtual String readStringFromPasteboard(int index, const String& pasteboardType) = 0;
    virtual PassRefPtr<SharedBuffer> readBufferFromPasteboard(int index, const String& pasteboardType) = 0;
    virtual URL readURLFromPasteboard(int index, const String& pasteboardType) = 0;
    virtual long changeCount() = 0;
#endif // PLATFORM(IOS)
#if PLATFORM(COCOA)
    virtual void getTypes(Vector<String>& types, const String& pasteboardName) = 0;
    virtual PassRefPtr<SharedBuffer> bufferForType(const String& pasteboardType, const String& pasteboardName) = 0;
    virtual void getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType, const String& pasteboardName) = 0;
    virtual String stringForType(const String& pasteboardType, const String& pasteboardName) = 0;
    virtual long changeCount(const String& pasteboardName) = 0;
    virtual String uniqueName() = 0;
    virtual Color color(const String& pasteboardName) = 0;
    virtual URL url(const String& pasteboardName) = 0;
    
    virtual long addTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) = 0;
    virtual long setTypes(const Vector<String>& pasteboardTypes, const String& pasteboardName) = 0;
    virtual long copy(const String& fromPasteboard, const String& toPasteboard) = 0;
    virtual long setBufferForType(PassRefPtr<SharedBuffer>, const String& pasteboardType, const String& pasteboardName) = 0;
    virtual long setPathnamesForType(const Vector<String>&, const String& pasteboardType, const String& pasteboardName) = 0;
    virtual long setStringForType(const String&, const String& pasteboardType, const String& pasteboardName) = 0;
#endif
protected:
    virtual ~PasteboardStrategy()
    {
    }
};

}

#endif // !PasteboardStrategy_h
