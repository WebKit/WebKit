/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#import "HistoryPropertyList.h"

#import "WebHistoryItemInternal.h"
#import <WebCore/HistoryItem.h>
#import <wtf/cf/VectorCF.h>

using namespace WebCore;

static const int currentFileVersion = 1;

struct CFMalloc {
    static void* malloc(size_t size) { return CFAllocatorAllocate(nullptr, size, 0); }
    static void free(void* p) { CFAllocatorDeallocate(nullptr, p); }
};

HistoryPropertyListWriter::HistoryPropertyListWriter()
    : m_displayTitleKey("displayTitle"_s)
    , m_lastVisitWasFailureKey("lastVisitWasFailure"_s)
    , m_lastVisitedDateKey("lastVisitedDate"_s)
    , m_redirectURLsKey("redirectURLs"_s)
    , m_titleKey("title"_s)
    , m_urlKey(emptyString())
{
}

HistoryPropertyListWriter::~HistoryPropertyListWriter() = default;

std::span<UInt8> HistoryPropertyListWriter::buffer(size_t size)
{
    ASSERT(!m_buffer);
    m_buffer = MallocSpan<UInt8, CFMalloc>::malloc(size);
    return m_buffer.mutableSpan();
}

RetainPtr<CFDataRef> HistoryPropertyListWriter::releaseData()
{
    if (!m_buffer)
        return nullptr;
    auto span = m_buffer.leakSpan();
    RetainPtr data = toCFDataNoCopy(span, kCFAllocatorNull);
    if (!data)
        adoptMallocSpan<UInt8, CFMalloc>(span); // We need to make sure we free the data if creating the CFData failed.
    return data;
}

void HistoryPropertyListWriter::writeObjects(BinaryPropertyListObjectStream& stream)
{
    size_t outerDictionaryStart = stream.writeDictionaryStart();

    stream.writeString("WebHistoryFileVersion"_s);
    stream.writeString("WebHistoryDates"_s);

    stream.writeInteger(currentFileVersion);
    size_t outerDateArrayStart = stream.writeArrayStart();
    writeHistoryItems(stream);
    stream.writeArrayEnd(outerDateArrayStart);

    stream.writeDictionaryEnd(outerDictionaryStart);
}

void HistoryPropertyListWriter::writeHistoryItem(BinaryPropertyListObjectStream& stream, WebHistoryItem* webHistoryItem)
{
    HistoryItem* item = core(webHistoryItem);

    size_t itemDictionaryStart = stream.writeDictionaryStart();

    const String& title = item->title();
    const String& displayTitle = item->alternateTitle();
    double lastVisitedDate = webHistoryItem->_private->_lastVisitedTime;
    Vector<String>* redirectURLs = webHistoryItem->_private->_redirectURLs.get();

    // keys
    stream.writeString(m_urlKey);
    if (!title.isEmpty())
        stream.writeString(m_titleKey);
    if (!displayTitle.isEmpty())
        stream.writeString(m_displayTitleKey);
    if (lastVisitedDate)
        stream.writeString(m_lastVisitedDateKey);
    if (item->lastVisitWasFailure())
        stream.writeString(m_lastVisitWasFailureKey);
    if (redirectURLs)
        stream.writeString(m_redirectURLsKey);

    // values
    stream.writeUniqueString(item->urlString());
    if (!title.isEmpty())
        stream.writeString(title);
    if (!displayTitle.isEmpty())
        stream.writeString(displayTitle);
    if (lastVisitedDate) {
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "%.1lf", lastVisitedDate);
        stream.writeUniqueString(buffer);
    }
    if (item->lastVisitWasFailure())
        stream.writeBooleanTrue();
    if (redirectURLs) {
        size_t redirectArrayStart = stream.writeArrayStart();
        size_t size = redirectURLs->size();
        ASSERT(size);
        for (size_t i = 0; i < size; ++i)
            stream.writeUniqueString(redirectURLs->at(i));
        stream.writeArrayEnd(redirectArrayStart);
    }

    stream.writeDictionaryEnd(itemDictionaryStart);
}

