/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LegacySessionStateCoding.h"

#include "APIData.h"
#include "SessionState.h"
#include <wtf/cf/TypeCasts.h>

namespace WebKit {

// Session state keys.
static const uint32_t sessionStateDataVersion = 2;

static const CFStringRef sessionHistoryKey = CFSTR("SessionHistory");
static const CFStringRef provisionalURLKey = CFSTR("ProvisionalURL");

// Session history keys.
static const CFStringRef sessionHistoryVersionKey = CFSTR("SessionHistoryVersion");
static const CFStringRef sessionHistoryCurrentIndexKey = CFSTR("SessionHistoryCurrentIndex");
static const CFStringRef sessionHistoryEntriesKey = CFSTR("SessionHistoryEntries");

// Session history entry keys.
static const CFStringRef sessionHistoryEntryURLKey = CFSTR("SessionHistoryEntryURL");
static CFStringRef sessionHistoryEntryTitleKey = CFSTR("SessionHistoryEntryTitle");
static CFStringRef sessionHistoryEntryOriginalURLKey = CFSTR("SessionHistoryEntryOriginalURL");
static CFStringRef sessionHistoryEntrySnapshotUUIDKey = CFSTR("SessionHistoryEntrySnapshotUUID");
static CFStringRef sessionHistoryEntryDataKey = CFSTR("SessionHistoryEntryData");

// Session history entry data.
const uint32_t sessionHistoryEntryDataVersion = 2;

LegacySessionStateDecoder::LegacySessionStateDecoder(API::Data* data)
    : m_data(data)
{
}

LegacySessionStateDecoder::~LegacySessionStateDecoder()
{
}

bool LegacySessionStateDecoder::decodeSessionState(SessionState& sessionState) const
{
    if (!m_data)
        return false;

    size_t size = m_data->size();
    const uint8_t* bytes = m_data->bytes();

    if (size < sizeof(uint32_t))
        return false;

    uint32_t versionNumber = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];

    if (versionNumber != sessionStateDataVersion)
        return false;

    auto data = adoptCF(CFDataCreate(kCFAllocatorDefault, bytes + sizeof(uint32_t), size - sizeof(uint32_t)));

    auto sessionStateDictionary = adoptCF(dynamic_cf_cast<CFDictionaryRef>(CFPropertyListCreateWithData(kCFAllocatorDefault, data.get(), kCFPropertyListImmutable, nullptr, nullptr)));
    if (!sessionStateDictionary)
        return false;

    if (auto backForwardListDictionary = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(sessionStateDictionary.get(), sessionHistoryKey))) {
        if (!decodeSessionHistory(backForwardListDictionary, sessionState.backForwardListState))
            return false;
    }

    if (auto provisionalURLString = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(sessionStateDictionary.get(), provisionalURLKey))) {
        sessionState.provisionalURL = WebCore::URL(WebCore::URL(), provisionalURLString);
        if (!sessionState.provisionalURL.isValid())
            return false;
    }

    return true;
}

bool LegacySessionStateDecoder::decodeSessionHistory(CFDictionaryRef backForwardListDictionary, BackForwardListState& backForwardListState) const
{
    auto sessionHistoryVersionNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(backForwardListDictionary, sessionHistoryVersionKey));
    if (!sessionHistoryVersionNumber) {
        // Version 0 session history dictionaries did not contain a version number.
        return decodeV0SessionHistory(backForwardListDictionary, backForwardListState);
    }

    CFIndex sessionHistoryVersion;
    if (!CFNumberGetValue(sessionHistoryVersionNumber, kCFNumberCFIndexType, &sessionHistoryVersion))
        return false;

    if (sessionHistoryVersion == 1)
        return decodeV1SessionHistory(backForwardListDictionary, backForwardListState);

    return false;
}

bool LegacySessionStateDecoder::decodeV0SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState) const
{
    // FIXME: Implement.
    return false;
}

bool LegacySessionStateDecoder::decodeV1SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState) const
{
    auto currentIndexNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryCurrentIndexKey));
    if (!currentIndexNumber) {
        // No current index means the dictionary represents an empty session.
        backForwardListState.currentIndex = 0;
        backForwardListState.items = { };
        return true;
    }

    CFIndex currentIndex;
    if (!CFNumberGetValue(currentIndexNumber, kCFNumberCFIndexType, &currentIndex))
        return false;

    if (currentIndex < 0)
        return false;

    auto historyEntries = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryEntriesKey));
    if (!historyEntries)
        return false;

    if (!decodeSessionHistoryEntries(historyEntries, backForwardListState.items))
        return false;

    backForwardListState.currentIndex = static_cast<uint32_t>(currentIndex);
    if (backForwardListState.currentIndex >= backForwardListState.items.size())
        return false;

    return true;
}

bool LegacySessionStateDecoder::decodeSessionHistoryEntries(CFArrayRef entriesArray, Vector<PageState>& entries) const
{
    for (CFIndex i = 0, size = CFArrayGetCount(entriesArray); i < size; ++i) {
        auto entryDictionary = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(entriesArray, i));
        if (!entryDictionary)
            return false;

        PageState entry;
        if (!decodeSessionHistoryEntry(entryDictionary, entry))
            return false;

        entries.append(std::move(entry));
    }

    return true;
}

bool LegacySessionStateDecoder::decodeSessionHistoryEntry(CFDictionaryRef entryDictionary, PageState& pageState) const
{
    auto title = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(entryDictionary, sessionHistoryEntryTitleKey));
    if (!title)
        return false;

    auto urlString = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(entryDictionary, sessionHistoryEntryURLKey));
    if (!urlString)
        return false;

    auto originalURLString = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(entryDictionary, sessionHistoryEntryOriginalURLKey));
    if (!originalURLString)
        return false;

    auto historyEntryData = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(entryDictionary, sessionHistoryEntryDataKey));
    if (!historyEntryData)
        return false;

    if (!decodeSessionHistoryEntryData(historyEntryData, pageState.mainFrameState))
        return false;

    pageState.title = title;
    pageState.mainFrameState.urlString = urlString;
    pageState.mainFrameState.originalURLString = originalURLString;

    return true;
}

template<typename T> void isValidEnum(T);

class HistoryEntryDataDecoder {
public:
    HistoryEntryDataDecoder(const uint8_t* buffer, size_t bufferSize)
        : m_buffer(buffer)
        , m_bufferEnd(buffer + bufferSize)
    {
        // Keep format compatibility by decoding an unused uint64_t here.
        uint64_t value;
        *this >> value;
    }

    HistoryEntryDataDecoder& operator>>(bool& value)
    {
        return decodeArithmeticType(value);
    }

    HistoryEntryDataDecoder& operator>>(uint32_t& value)
    {
        return decodeArithmeticType(value);
    }

    HistoryEntryDataDecoder& operator>>(int32_t& value)
    {
        return *this >> reinterpret_cast<uint32_t&>(value);
    }

    HistoryEntryDataDecoder& operator>>(uint64_t& value)
    {
        return decodeArithmeticType(value);
    }

    HistoryEntryDataDecoder& operator>>(int64_t& value)
    {
        return *this >> reinterpret_cast<uint64_t&>(value);
    }

    HistoryEntryDataDecoder& operator>>(float& value)
    {
        return decodeArithmeticType(value);
    }

    HistoryEntryDataDecoder& operator>>(String& value)
    {
        value = String();

        uint32_t length;
        *this >> length;

        if (length == std::numeric_limits<uint32_t>::max()) {
            // This is the null string.
            value = String();
            return *this;
        }

        uint64_t lengthInBytes;
        *this >> lengthInBytes;

        if (lengthInBytes % sizeof(UChar) || lengthInBytes / sizeof(UChar) != length) {
            markInvalid();
            return *this;
        }

        if (!bufferIsLargeEnoughToContain<UChar>(length)) {
            markInvalid();
            return *this;
        }

        UChar* buffer;
        auto string = String::createUninitialized(length, buffer);
        decodeFixedLengthData(reinterpret_cast<uint8_t*>(buffer), length * sizeof(UChar), alignof(UChar));

        value = string;
        return *this;
    }

    HistoryEntryDataDecoder& operator>>(Vector<uint8_t>& value)
    {
        value = { };

        uint64_t size;
        *this >> size;

        if (!alignBufferPosition(1, size))
            return *this;

        const uint8_t* data = m_buffer;
        m_buffer += size;

        value.append(data, size);
        return *this;
    }

    HistoryEntryDataDecoder& operator>>(Vector<char>& value)
    {
        value = { };

        uint64_t size;
        *this >> size;

        if (!alignBufferPosition(1, size))
            return *this;

        const uint8_t* data = m_buffer;
        m_buffer += size;

        value.append(data, size);
        return *this;
    }

    template<typename T>
    auto operator>>(Optional<T>& value) -> typename std::enable_if<std::is_enum<T>::value, HistoryEntryDataDecoder&>::type
    {
        uint32_t underlyingEnumValue;
        *this >> underlyingEnumValue;

        if (!isValid() || !isValidEnum(static_cast<T>(underlyingEnumValue)))
            value = Nullopt;
        else
            value = static_cast<T>(underlyingEnumValue);

        return *this;
    }

    bool isValid() const { return m_buffer <= m_bufferEnd; }

    bool finishDecoding() { return m_buffer == m_bufferEnd; }

private:
    template<typename Type>
    HistoryEntryDataDecoder& decodeArithmeticType(Type& value)
    {
        static_assert(std::is_arithmetic<Type>::value, "");
        value = Type();

        decodeFixedLengthData(reinterpret_cast<uint8_t*>(&value), sizeof(value), sizeof(value));
        return *this;
    }

    void decodeFixedLengthData(uint8_t* data, size_t size, unsigned alignment)
    {
        if (!alignBufferPosition(alignment, size))
            return;

        memcpy(data, m_buffer, size);
        m_buffer += size;
    }

    bool alignBufferPosition(unsigned alignment, size_t size)
    {
        const uint8_t* alignedPosition = alignedBuffer(alignment);
        if (!alignedBufferIsLargeEnoughToContain(alignedPosition, size)) {
            // We've walked off the end of this buffer.
            markInvalid();
            return false;
        }

        m_buffer = alignedPosition;
        return true;
    }

    const uint8_t* alignedBuffer(unsigned alignment) const
    {
        ASSERT(alignment && !(alignment & (alignment - 1)));

        uintptr_t alignmentMask = alignment - 1;
        return reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(m_buffer) + alignmentMask) & ~alignmentMask);
    }

    template<typename T>
    bool bufferIsLargeEnoughToContain(size_t numElements) const
    {
        static_assert(std::is_arithmetic<T>::value, "Type T must have a fixed, known encoded size!");

        if (numElements > std::numeric_limits<size_t>::max() / sizeof(T))
            return false;

        return bufferIsLargeEnoughToContain(alignof(T), numElements * sizeof(T));
    }

    bool bufferIsLargeEnoughToContain(unsigned alignment, size_t size) const
    {
        return alignedBufferIsLargeEnoughToContain(alignedBuffer(alignment), size);
    }

    inline bool alignedBufferIsLargeEnoughToContain(const uint8_t* alignedPosition, size_t size) const
    {
        return m_bufferEnd >= alignedPosition && static_cast<size_t>(m_bufferEnd - alignedPosition) >= size;
    }

    void markInvalid() { m_buffer = m_bufferEnd + 1; }

    const uint8_t* m_buffer;
    const uint8_t* m_bufferEnd;
};

enum class FormDataElementType {
    Data = 0,
    EncodedFile = 1,
    EncodedBlob = 2,
};

static bool isValidEnum(FormDataElementType type)
{
    switch (type) {
    case FormDataElementType::Data:
    case FormDataElementType::EncodedFile:
    case FormDataElementType::EncodedBlob:
        return true;
    }

    return false;
}

static void decodeFormDataElement(HistoryEntryDataDecoder& decoder, HTTPBody::Element& formDataElement)
{
    Optional<FormDataElementType> elementType;
    decoder >> elementType;
    if (!elementType)
        return;

    switch (elementType.value()) {
    case FormDataElementType::Data:
        formDataElement.type = HTTPBody::Element::Type::Data;
        decoder >> formDataElement.data;
        break;

    case FormDataElementType::EncodedFile:
    case FormDataElementType::EncodedBlob:
        // FIXME: Implement.
        ASSERT_NOT_REACHED();
    }
}

static void decodeFormData(HistoryEntryDataDecoder& decoder, HTTPBody& formData)
{
    bool alwaysStream;
    decoder >> alwaysStream;

    Vector<uint8_t> boundary;
    decoder >> boundary;

    uint64_t formDataElementCount;
    decoder >> formDataElementCount;

    for (uint64_t i = 0; i < formDataElementCount; ++i) {
        HTTPBody::Element formDataElement;
        decodeFormDataElement(decoder, formDataElement);

        formData.elements.append(std::move(formDataElement));
    }
}

static void decodeBackForwardTreeNode(HistoryEntryDataDecoder& decoder, FrameState& frameState)
{
    uint64_t childCount;
    decoder >> childCount;

    for (uint64_t i = 0; i < childCount; ++i) {
        FrameState childFrameState;
        decoder >> childFrameState.originalURLString;
        decoder >> childFrameState.urlString;

        decodeBackForwardTreeNode(decoder, childFrameState);
        frameState.children.append(std::move(childFrameState));
    }

    decoder >> frameState.documentSequenceNumber;

    uint64_t documentStateVectorSize;
    decoder >> documentStateVectorSize;

    for (uint64_t i = 0; i < documentStateVectorSize; ++i) {
        // FIXME: Implement.
        ASSERT_NOT_REACHED();
    }

    String formContentType;
    decoder >> formContentType;

    bool hasFormData;
    decoder >> hasFormData;

    if (hasFormData) {
        HTTPBody httpBody;
        httpBody.contentType = std::move(formContentType);

        decodeFormData(decoder, httpBody);

        frameState.httpBody = std::move(httpBody);
    }

    decoder >> frameState.itemSequenceNumber;

    decoder >> frameState.referrer;

    int32_t scrollPointX;
    decoder >> scrollPointX;

    int32_t scrollPointY;
    decoder >> scrollPointY;

    frameState.scrollPoint = WebCore::IntPoint(scrollPointX, scrollPointY);

    decoder >> frameState.pageScaleFactor;

    bool hasStateObject;
    decoder >> hasStateObject;

    if (hasStateObject) {
        // FIXME: Implement.
        ASSERT_NOT_REACHED();
    }

    decoder >> frameState.target;
}

bool LegacySessionStateDecoder::decodeSessionHistoryEntryData(CFDataRef historyEntryData, FrameState& mainFrameState) const
{
    HistoryEntryDataDecoder decoder { CFDataGetBytePtr(historyEntryData), static_cast<size_t>(CFDataGetLength(historyEntryData)) };

    uint32_t version;
    decoder >> version;

    if (version != sessionHistoryEntryDataVersion)
        return false;

    decodeBackForwardTreeNode(decoder, mainFrameState);

    return decoder.finishDecoding();
}


} // namespace WebKit
