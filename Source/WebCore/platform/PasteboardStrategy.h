/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

class Color;
class LegacyWebArchive;
class SharedBuffer;
class PasteboardContext;
class PasteboardCustomData;
class SelectionData;
class FragmentedSharedBuffer;
struct PasteboardImage;
struct PasteboardItemInfo;
struct PasteboardURL;
struct PasteboardWebContent;

class PasteboardStrategy : public CanMakeCheckedPtr<PasteboardStrategy> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PasteboardStrategy);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PasteboardStrategy);
public:
#if PLATFORM(IOS_FAMILY)
    virtual void writeToPasteboard(const PasteboardURL&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual void writeToPasteboard(const PasteboardWebContent&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual void writeToPasteboard(const PasteboardImage&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual void writeToPasteboard(StringImpl* pasteboardType, StringImpl*, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual void updateSupportedTypeIdentifiers(const Vector<RefPtr<StringImpl>>& identifiers, StringImpl* pasteboardName, const PasteboardContext*) = 0;
#endif // PLATFORM(IOS_FAMILY)
#if PLATFORM(COCOA)
    virtual void getTypes(Vector<RefPtr<StringImpl>>& types, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual RefPtr<SharedBuffer> bufferForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual void getPathnamesForType(Vector<RefPtr<StringImpl>>& pathnames, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual RefPtr<StringImpl> stringForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual Vector<RefPtr<StringImpl>> allStringsForType(StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t changeCount(StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual Color color(StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual URL url(StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int getNumberOfFiles(StringImpl* pasteboardName, const PasteboardContext*) = 0;

    virtual int64_t addTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t setTypes(const Vector<RefPtr<StringImpl>>& pasteboardTypes, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t setBufferForType(SharedBuffer*, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t setURL(const PasteboardURL&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t setColor(const Color&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t setStringForType(StringImpl*, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual int64_t writeWebArchive(LegacyWebArchive&, StringImpl* pasteboardName) = 0;

    virtual bool containsURLStringSuitableForLoading(StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual RefPtr<StringImpl> urlStringSuitableForLoading(StringImpl* pasteboardName, RefPtr<StringImpl>& title, const PasteboardContext*) = 0;
#endif
    virtual RefPtr<StringImpl> readStringFromPasteboard(size_t index, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual RefPtr<SharedBuffer> readBufferFromPasteboard(std::optional<size_t> index, StringImpl* pasteboardType, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual URL readURLFromPasteboard(size_t index, StringImpl* pasteboardName, RefPtr<StringImpl>& title, const PasteboardContext*) = 0;
    virtual std::optional<PasteboardItemInfo> informationForItemAtIndex(size_t index, StringImpl* pasteboardName, int64_t changeCount, const PasteboardContext*) = 0;
    virtual std::optional<Vector<PasteboardItemInfo>> allPasteboardItemInfo(StringImpl* pasteboardName, int64_t changeCount, const PasteboardContext*) = 0;
    virtual int getPasteboardItemsCount(StringImpl* pasteboardName, const PasteboardContext*) = 0;

    virtual Vector<RefPtr<StringImpl>> typesSafeForDOMToReadAndWrite(StringImpl* pasteboardName, StringImpl* origin, const PasteboardContext*) = 0;
    virtual int64_t writeCustomData(const Vector<PasteboardCustomData>&, StringImpl* pasteboardName, const PasteboardContext*) = 0;
    virtual bool containsStringSafeForDOMToReadForType(StringImpl*, StringImpl* pasteboardName, const PasteboardContext*) = 0;

#if PLATFORM(GTK) || PLATFORM(WPE)
    virtual Vector<RefPtr<StringImpl>> types(StringImpl* pasteboardName) = 0;
    virtual RefPtr<StringImpl> readTextFromClipboard(StringImpl* pasteboardName, StringImpl* pasteboardType) = 0;
    virtual Vector<RefPtr<StringImpl>> readFilePathsFromClipboard(StringImpl* pasteboardName) = 0;
    virtual RefPtr<SharedBuffer> readBufferFromClipboard(StringImpl* pasteboardName, StringImpl* pasteboardType) = 0;
    virtual void writeToClipboard(StringImpl* pasteboardName, SelectionData&&) = 0;
    virtual void clearClipboard(StringImpl* pasteboardName) = 0;
#elif USE(LIBWPE)
    virtual void getTypes(Vector<RefPtr<StringImpl>>& types) = 0;
    virtual void writeToPasteboard(const PasteboardWebContent&) = 0;
    virtual void writeToPasteboard(StringImpl* pasteboardType, StringImpl*) = 0;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE) || PLATFORM(WIN)
    virtual int64_t changeCount(StringImpl* pasteboardName) = 0;
#endif

protected:
    virtual ~PasteboardStrategy()
    {
    }
};

}

#endif // !PasteboardStrategy_h
