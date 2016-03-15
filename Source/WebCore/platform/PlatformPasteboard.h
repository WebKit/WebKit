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

#ifndef PlatformPasteboard_h
#define PlatformPasteboard_h

#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
OBJC_CLASS NSPasteboard;
#endif

#if PLATFORM(IOS)
OBJC_CLASS UIPasteboard;
#endif

namespace WebCore {

class Color;
class SharedBuffer;
class URL;
struct PasteboardImage;
struct PasteboardWebContent;

class PlatformPasteboard {
public:
    // FIXME: probably we don't need a constructor that takes a pasteboard name for iOS.
    WEBCORE_EXPORT explicit PlatformPasteboard(const String& pasteboardName);
#if PLATFORM(IOS)
    WEBCORE_EXPORT PlatformPasteboard();
#endif
    WEBCORE_EXPORT static String uniqueName();
    
    WEBCORE_EXPORT void getTypes(Vector<String>& types);
    WEBCORE_EXPORT RefPtr<SharedBuffer> bufferForType(const String& pasteboardType);
    WEBCORE_EXPORT void getPathnamesForType(Vector<String>& pathnames, const String& pasteboardType);
    WEBCORE_EXPORT String stringForType(const String& pasteboardType);
    WEBCORE_EXPORT long changeCount() const;
    WEBCORE_EXPORT Color color();
    WEBCORE_EXPORT URL url();

    // Take ownership of the pasteboard, and return new change count.
    WEBCORE_EXPORT long addTypes(const Vector<String>& pasteboardTypes);
    WEBCORE_EXPORT long setTypes(const Vector<String>& pasteboardTypes);

    // These methods will return 0 if pasteboard ownership has been taken from us.
    WEBCORE_EXPORT long copy(const String& fromPasteboard);
    WEBCORE_EXPORT long setBufferForType(PassRefPtr<SharedBuffer>, const String& pasteboardType);
    WEBCORE_EXPORT long setPathnamesForType(const Vector<String>& pathnames, const String& pasteboardType);
    WEBCORE_EXPORT long setStringForType(const String&, const String& pasteboardType);
    WEBCORE_EXPORT void write(const PasteboardWebContent&);
    WEBCORE_EXPORT void write(const PasteboardImage&);
    WEBCORE_EXPORT void write(const String& pasteboardType, const String&);
    WEBCORE_EXPORT RefPtr<SharedBuffer> readBuffer(int index, const String& pasteboardType);
    WEBCORE_EXPORT String readString(int index, const String& pasteboardType);
    WEBCORE_EXPORT URL readURL(int index, const String& pasteboardType);
    WEBCORE_EXPORT int count();

private:
#if PLATFORM(MAC)
    RetainPtr<NSPasteboard> m_pasteboard;
#endif
#if PLATFORM(IOS)
    RetainPtr<UIPasteboard> m_pasteboard;
#endif
};

}

#endif // !PlatformPasteboard_h
