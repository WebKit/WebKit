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
#include <mutex>
#include <wtf/MallocPtr.h>
#include <wtf/cf/TypeCasts.h>
#include <wtf/text/StringView.h>

namespace WebKit {

// Session state keys.
static const uint32_t sessionStateDataVersion = 2;

static const CFStringRef sessionHistoryKey = CFSTR("SessionHistory");
static const CFStringRef provisionalURLKey = CFSTR("ProvisionalURL");

// Session history keys.
static const uint32_t sessionHistoryVersion = 1;

static const CFStringRef sessionHistoryVersionKey = CFSTR("SessionHistoryVersion");
static const CFStringRef sessionHistoryCurrentIndexKey = CFSTR("SessionHistoryCurrentIndex");
static const CFStringRef sessionHistoryEntriesKey = CFSTR("SessionHistoryEntries");

// Session history entry keys.
static const CFStringRef sessionHistoryEntryURLKey = CFSTR("SessionHistoryEntryURL");
static CFStringRef sessionHistoryEntryTitleKey = CFSTR("SessionHistoryEntryTitle");
static CFStringRef sessionHistoryEntryOriginalURLKey = CFSTR("SessionHistoryEntryOriginalURL");
static CFStringRef sessionHistoryEntryDataKey = CFSTR("SessionHistoryEntryData");

// Session history entry data.
const uint32_t sessionHistoryEntryDataVersion = 2;

template<typename T> void isValidEnum(T);

class HistoryEntryDataEncoder {
public:
    HistoryEntryDataEncoder()
        : m_bufferSize(0)
        , m_bufferCapacity(512)
        , m_buffer(MallocPtr<uint8_t>::malloc(m_bufferCapacity))
        , m_bufferPointer(m_buffer.get())
    {
        // Keep format compatibility by encoding an unused uint64_t here.
        *this << static_cast<uint64_t>(0);
    }

    HistoryEntryDataEncoder& operator<<(uint32_t value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(int32_t value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(uint64_t value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(int64_t value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(float value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(double value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(bool value)
    {
        return encodeArithmeticType(value);
    }

    HistoryEntryDataEncoder& operator<<(const String& value)
    {
        // Special case the null string.
        if (value.isNull())
            return *this << std::numeric_limits<uint32_t>::max();

        uint32_t length = value.length();
        *this << length;

        *this << static_cast<uint64_t>(length * sizeof(UChar));
        encodeFixedLengthData(reinterpret_cast<const uint8_t*>(StringView(value).upconvertedCharacters().get()), length * sizeof(UChar), alignof(UChar));

        return *this;
    }

    HistoryEntryDataEncoder& operator<<(const Vector<uint8_t>& value)
    {
        *this << static_cast<uint64_t>(value.size());
        encodeFixedLengthData(value.data(), value.size(), 1);

        return *this;
    }

    HistoryEntryDataEncoder& operator<<(const Vector<char>& value)
    {
        *this << static_cast<uint64_t>(value.size());
        encodeFixedLengthData(reinterpret_cast<const uint8_t*>(value.data()), value.size(), 1);

        return *this;
    }

#if PLATFORM(IOS)
    HistoryEntryDataEncoder& operator<<(WebCore::FloatRect value)
    {
        *this << value.x();
        *this << value.y();
        *this << value.width();
        *this << value.height();

        return *this;
    }

    HistoryEntryDataEncoder& operator<<(WebCore::IntRect value)
    {
        *this << value.x();
        *this << value.y();
        *this << value.width();
        *this << value.height();

        return *this;
    }

    HistoryEntryDataEncoder& operator<<(WebCore::FloatSize value)
    {
        *this << value.width();
        *this << value.height();

        return *this;
    }

    HistoryEntryDataEncoder& operator<<(WebCore::IntSize value)
    {
        *this << value.width();
        *this << value.height();

        return *this;
    }
#endif

    template<typename T>
    auto operator<<(T value) -> typename std::enable_if<std::is_enum<T>::value, HistoryEntryDataEncoder&>::type
    {
        return *this << static_cast<uint32_t>(value);
    }

    MallocPtr<uint8_t> finishEncoding(size_t& size)
    {
        size = m_bufferSize;
        return WTF::move(m_buffer);
    }

private:
    template<typename Type>
    HistoryEntryDataEncoder& encodeArithmeticType(Type value)
    {
        static_assert(std::is_arithmetic<Type>::value, "");

        encodeFixedLengthData(reinterpret_cast<uint8_t*>(&value), sizeof(value), sizeof(value));
        return *this;
    }

    void encodeFixedLengthData(const uint8_t* data, size_t size, unsigned alignment)
    {
        ASSERT(!(reinterpret_cast<uintptr_t>(data) % alignment));

        uint8_t* buffer = grow(alignment, size);
        memcpy(buffer, data, size);
    }

    uint8_t* grow(unsigned alignment, size_t size)
    {
        size_t alignedSize = ((m_bufferSize + alignment - 1) / alignment) * alignment;

        growCapacity(alignedSize + size);

        m_bufferSize = alignedSize + size;
        m_bufferPointer = m_buffer.get() + m_bufferSize;

        return m_buffer.get() + alignedSize;
    }

    void growCapacity(size_t newSize)
    {
        if (newSize <= m_bufferCapacity)
            return;

        size_t newCapacity = m_bufferCapacity * 2;
        while (newCapacity < newSize)
            newCapacity *= 2;

        m_buffer.realloc(newCapacity);
        m_bufferCapacity = newCapacity;
    }

    size_t m_bufferSize;
    size_t m_bufferCapacity;
    MallocPtr<uint8_t> m_buffer;
    uint8_t* m_bufferPointer;
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

static void encodeFormDataElement(HistoryEntryDataEncoder& encoder, const HTTPBody::Element& element)
{
    switch (element.type) {
    case HTTPBody::Element::Type::Data:
        encoder << FormDataElementType::Data;
        encoder << element.data;
        break;

    case HTTPBody::Element::Type::File:
        encoder << FormDataElementType::EncodedFile;
        encoder << element.filePath;

        // Used to be generatedFilename.
        encoder << String();

        // Used to be shouldGenerateFile.
        encoder << false;

        encoder << element.fileStart;
        encoder << element.fileLength.valueOr(-1);
        encoder << element.expectedFileModificationTime.valueOr(std::numeric_limits<double>::quiet_NaN());
        break;

    case HTTPBody::Element::Type::Blob:
        encoder << FormDataElementType::EncodedBlob;
        encoder << element.blobURLString;
        break;
    }
}

static void encodeFormData(HistoryEntryDataEncoder& encoder, const HTTPBody& formData)
{
    // Used to be alwaysStream.
    encoder << false;

    // Used to be boundary.
    encoder << Vector<uint8_t>();

    encoder << static_cast<uint64_t>(formData.elements.size());
    for (const auto& element : formData.elements)
        encodeFormDataElement(encoder, element);

    // Used to be hasGeneratedFiles.
    encoder << false;

    // Used to be identifier.
    encoder << static_cast<int64_t>(0);
}

static void encodeFrameStateNode(HistoryEntryDataEncoder& encoder, const FrameState& frameState)
{
    encoder << static_cast<uint64_t>(frameState.children.size());

    for (const auto& childFrameState : frameState.children) {
        encoder << childFrameState.originalURLString;
        encoder << childFrameState.urlString;

        encodeFrameStateNode(encoder, childFrameState);
    }

    encoder << frameState.documentSequenceNumber;

    encoder << static_cast<uint64_t>(frameState.documentState.size());
    for (const auto& documentState : frameState.documentState)
        encoder << documentState;

    if (frameState.httpBody) {
        encoder << frameState.httpBody.value().contentType;
        encoder << true;

        encodeFormData(encoder, frameState.httpBody.value());
    } else {
        encoder << String();
        encoder << false;
    }

    encoder << frameState.itemSequenceNumber;

    encoder << frameState.referrer;

    encoder << frameState.scrollPoint.x();
    encoder << frameState.scrollPoint.y();

    encoder << frameState.pageScaleFactor;

    encoder << !!frameState.stateObjectData;
    if (frameState.stateObjectData)
        encoder << frameState.stateObjectData.value();

    encoder << frameState.target;

#if PLATFORM(IOS)
    // FIXME: iOS should not use the legacy session state encoder.
    encoder << frameState.exposedContentRect;
    encoder << frameState.unobscuredContentRect;
    encoder << frameState.minimumLayoutSizeInScrollViewCoordinates;
    encoder << frameState.contentSize;
    encoder << frameState.scaleIsInitial;
#endif
}

static MallocPtr<uint8_t> encodeSessionHistoryEntryData(const FrameState& frameState, size_t& bufferSize)
{
    HistoryEntryDataEncoder encoder;

    encoder << sessionHistoryEntryDataVersion;
    encodeFrameStateNode(encoder, frameState);

    return encoder.finishEncoding(bufferSize);
}

static RetainPtr<CFDataRef> encodeSessionHistoryEntryData(const FrameState& frameState)
{
    static CFAllocatorRef fastMallocDeallocator;

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        CFAllocatorContext context = {
            0, // version
            nullptr, // info
            nullptr, // retain
            nullptr, // release
            nullptr, // copyDescription
            nullptr, // allocate
            nullptr, // reallocate
            [](void *ptr, void *info) {
                WTF::fastFree(ptr);
            },
            nullptr, // preferredSize
        };
        fastMallocDeallocator = CFAllocatorCreate(kCFAllocatorDefault, &context);
    });

    size_t bufferSize;
    auto buffer = encodeSessionHistoryEntryData(frameState, bufferSize);

    return adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer.leakPtr(), bufferSize, fastMallocDeallocator));
}

static RetainPtr<CFDictionaryRef> createDictionary(std::initializer_list<std::pair<CFStringRef, CFTypeRef>> keyValuePairs)
{
    Vector<CFTypeRef> keys;
    Vector<CFTypeRef> values;

    keys.reserveInitialCapacity(keyValuePairs.size());
    values.reserveInitialCapacity(keyValuePairs.size());

    for (const auto& keyValuePair : keyValuePairs) {
        keys.uncheckedAppend(keyValuePair.first);
        values.uncheckedAppend(keyValuePair.second);
    }

    return adoptCF(CFDictionaryCreate(kCFAllocatorDefault, keys.data(), values.data(), keyValuePairs.size(), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
}

static RetainPtr<CFDictionaryRef> encodeSessionHistory(const BackForwardListState& backForwardListState)
{
    ASSERT(!backForwardListState.currentIndex || backForwardListState.currentIndex.value() < backForwardListState.items.size());

    auto sessionHistoryVersionNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &sessionHistoryVersion));

    if (!backForwardListState.currentIndex)
        return createDictionary({ { sessionHistoryVersionKey, sessionHistoryVersionNumber.get() } });

    auto entries = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, backForwardListState.items.size(), &kCFTypeArrayCallBacks));

    for (const auto& item : backForwardListState.items) {
        auto url = item.pageState.mainFrameState.urlString.createCFString();
        auto title = item.pageState.title.createCFString();
        auto originalURL = item.pageState.mainFrameState.originalURLString.createCFString();
        auto data = encodeSessionHistoryEntryData(item.pageState.mainFrameState);

        auto entryDictionary = createDictionary({ { sessionHistoryEntryURLKey, url.get() }, { sessionHistoryEntryTitleKey, title.get() }, { sessionHistoryEntryOriginalURLKey, originalURL.get() }, { sessionHistoryEntryDataKey, data.get() } });

        CFArrayAppendValue(entries.get(), entryDictionary.get());
    }

    uint32_t currentIndex = backForwardListState.currentIndex.value();
    auto currentIndexNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &currentIndex));

    return createDictionary({ { sessionHistoryVersionKey, sessionHistoryVersionNumber.get() }, { sessionHistoryCurrentIndexKey, currentIndexNumber.get() }, { sessionHistoryEntriesKey, entries.get() } });
}

RefPtr<API::Data> encodeLegacySessionState(const SessionState& sessionState)
{
    auto sessionHistoryDictionary = encodeSessionHistory(sessionState.backForwardListState);
    auto provisionalURLString = sessionState.provisionalURL.isNull() ? nullptr : sessionState.provisionalURL.string().createCFString();

    RetainPtr<CFDictionaryRef> stateDictionary;
    if (provisionalURLString)
        stateDictionary = createDictionary({ { sessionHistoryKey, sessionHistoryDictionary.get() }, { provisionalURLKey, provisionalURLString.get() } });
    else
        stateDictionary = createDictionary({ { sessionHistoryKey, sessionHistoryDictionary.get() } });

    auto writeStream = adoptCF(CFWriteStreamCreateWithAllocatedBuffers(kCFAllocatorDefault, nullptr));
    if (!writeStream)
        return nullptr;

    if (!CFWriteStreamOpen(writeStream.get()))
        return nullptr;

    if (!CFPropertyListWrite(stateDictionary.get(), writeStream.get(), kCFPropertyListBinaryFormat_v1_0, 0, nullptr))
        return nullptr;

    auto data = adoptCF(static_cast<CFDataRef>(CFWriteStreamCopyProperty(writeStream.get(), kCFStreamPropertyDataWritten)));

    CFIndex length = CFDataGetLength(data.get());

    size_t bufferSize = length + sizeof(uint32_t);
    auto buffer = MallocPtr<uint8_t>::malloc(bufferSize);

    // Put the session state version number at the start of the buffer
    buffer.get()[0] = (sessionStateDataVersion & 0xff000000) >> 24;
    buffer.get()[1] = (sessionStateDataVersion & 0x00ff0000) >> 16;
    buffer.get()[2] = (sessionStateDataVersion & 0x0000ff00) >> 8;
    buffer.get()[3] = (sessionStateDataVersion & 0x000000ff);

    // Copy in the actual session state data
    CFDataGetBytes(data.get(), CFRangeMake(0, length), buffer.get() + sizeof(uint32_t));

    return API::Data::createWithoutCopying(buffer.leakPtr(), bufferSize, [] (unsigned char* buffer, const void* context) {
        fastFree(buffer);
    }, nullptr);
}

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

    HistoryEntryDataDecoder& operator>>(double& value)
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

#if PLATFORM(IOS)
    HistoryEntryDataDecoder& operator>>(WebCore::FloatRect& value)
    {
        value = WebCore::FloatRect();

        float x;
        *this >> x;

        float y;
        *this >> y;

        float width;
        *this >> width;

        float height;
        *this >> height;

        value = WebCore::FloatRect(x, y, width, height);
        return *this;
    }

    HistoryEntryDataDecoder& operator>>(WebCore::IntRect& value)
    {
        value = WebCore::IntRect();

        int32_t x;
        *this >> x;

        int32_t y;
        *this >> y;

        int32_t width;
        *this >> width;

        int32_t height;
        *this >> height;

        value = WebCore::IntRect(x, y, width, height);
        return *this;
    }

    HistoryEntryDataDecoder& operator>>(WebCore::FloatSize& value)
    {
        value = WebCore::FloatSize();

        float width;
        *this >> width;

        float height;
        *this >> height;

        value = WebCore::FloatSize(width, height);
        return *this;
    }

    HistoryEntryDataDecoder& operator>>(WebCore::IntSize& value)
    {
        value = WebCore::IntSize();

        int32_t width;
        *this >> width;

        int32_t height;
        *this >> height;

        value = WebCore::IntSize(width, height);
        return *this;
    }
#endif

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
    void markInvalid() { m_buffer = m_bufferEnd + 1; }

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

    const uint8_t* m_buffer;
    const uint8_t* m_bufferEnd;
};

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

    case FormDataElementType::EncodedFile: {
        decoder >> formDataElement.filePath;

        String generatedFilename;
        decoder >> generatedFilename;

        bool shouldGenerateFile;
        decoder >> shouldGenerateFile;

        decoder >> formDataElement.fileStart;
        if (formDataElement.fileStart < 0) {
            decoder.markInvalid();
            return;
        }

        int64_t fileLength;
        decoder >> fileLength;
        if (fileLength != -1) {
            if (fileLength < formDataElement.fileStart)
                return;

            formDataElement.fileLength = fileLength;
        }


        double expectedFileModificationTime;
        decoder >> expectedFileModificationTime;
        if (expectedFileModificationTime != std::numeric_limits<double>::quiet_NaN())
            formDataElement.expectedFileModificationTime = expectedFileModificationTime;

        break;
    }

    case FormDataElementType::EncodedBlob:
        decoder >> formDataElement.blobURLString;
        break;
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

        if (!decoder.isValid())
            return;

        formData.elements.append(WTF::move(formDataElement));
    }

    bool hasGeneratedFiles;
    decoder >> hasGeneratedFiles;

    int64_t identifier;
    decoder >> identifier;
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

        if (!decoder.isValid())
            return;

        frameState.children.append(WTF::move(childFrameState));
    }

    decoder >> frameState.documentSequenceNumber;

    uint64_t documentStateVectorSize;
    decoder >> documentStateVectorSize;

    for (uint64_t i = 0; i < documentStateVectorSize; ++i) {
        String state;
        decoder >> state;

        if (!decoder.isValid())
            return;

        frameState.documentState.append(WTF::move(state));
    }

    String formContentType;
    decoder >> formContentType;

    bool hasFormData;
    decoder >> hasFormData;

    if (hasFormData) {
        HTTPBody httpBody;
        httpBody.contentType = WTF::move(formContentType);

        decodeFormData(decoder, httpBody);

        frameState.httpBody = WTF::move(httpBody);
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
        Vector<uint8_t> stateObjectData;
        decoder >> stateObjectData;

        frameState.stateObjectData = WTF::move(stateObjectData);
    }

    decoder >> frameState.target;

    // FIXME: iOS should not use the legacy session state decoder.
#if PLATFORM(IOS)
    decoder >> frameState.exposedContentRect;
    decoder >> frameState.unobscuredContentRect;
    decoder >> frameState.minimumLayoutSizeInScrollViewCoordinates;
    decoder >> frameState.contentSize;
    decoder >> frameState.scaleIsInitial;
#endif
}

static bool decodeSessionHistoryEntryData(const uint8_t* buffer, size_t bufferSize, FrameState& mainFrameState)
{
    HistoryEntryDataDecoder decoder { buffer, bufferSize };

    uint32_t version;
    decoder >> version;

    if (version != sessionHistoryEntryDataVersion)
        return false;

    decodeBackForwardTreeNode(decoder, mainFrameState);

    return decoder.finishDecoding();
}

static bool decodeSessionHistoryEntryData(CFDataRef historyEntryData, FrameState& mainFrameState)
{
    return decodeSessionHistoryEntryData(CFDataGetBytePtr(historyEntryData), static_cast<size_t>(CFDataGetLength(historyEntryData)), mainFrameState);
}

static bool decodeSessionHistoryEntry(CFDictionaryRef entryDictionary, BackForwardListItemState& backForwardListItemState)
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

    if (!decodeSessionHistoryEntryData(historyEntryData, backForwardListItemState.pageState.mainFrameState))
        return false;

    backForwardListItemState.pageState.title = title;
    backForwardListItemState.pageState.mainFrameState.urlString = urlString;
    backForwardListItemState.pageState.mainFrameState.originalURLString = originalURLString;

    return true;
}

static bool decodeSessionHistoryEntries(CFArrayRef entriesArray, Vector<BackForwardListItemState>& entries)
{
    for (CFIndex i = 0, size = CFArrayGetCount(entriesArray); i < size; ++i) {
        auto entryDictionary = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(entriesArray, i));
        if (!entryDictionary)
            return false;

        BackForwardListItemState entry;
        if (!decodeSessionHistoryEntry(entryDictionary, entry))
            return false;

        entries.append(WTF::move(entry));
    }

    return true;
}

static bool decodeV0SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState)
{
    auto currentIndexNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryCurrentIndexKey));
    if (!currentIndexNumber)
        return false;

    CFIndex currentIndex;
    if (!CFNumberGetValue(currentIndexNumber, kCFNumberCFIndexType, &currentIndex))
        return false;

    if (currentIndex < -1)
        return false;

    auto historyEntries = dynamic_cf_cast<CFArrayRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryEntriesKey));
    if (!historyEntries)
        return false;

    // Version 0 session history relied on currentIndex == -1 to represent the same thing as not having a current index.
    bool hasCurrentIndex = currentIndex != -1;

    if (!decodeSessionHistoryEntries(historyEntries, backForwardListState.items))
        return false;

    if (!hasCurrentIndex && CFArrayGetCount(historyEntries))
        return false;

    if (hasCurrentIndex) {
        if (static_cast<uint32_t>(currentIndex) >= backForwardListState.items.size())
            return false;

        backForwardListState.currentIndex = static_cast<uint32_t>(currentIndex);
    }

    return true;
}

static bool decodeV1SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState)
{
    auto currentIndexNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryCurrentIndexKey));
    if (!currentIndexNumber) {
        // No current index means the dictionary represents an empty session.
        backForwardListState.currentIndex = Nullopt;
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
    if (static_cast<uint32_t>(currentIndex) >= backForwardListState.items.size())
        return false;

    return true;
}

static bool decodeSessionHistory(CFDictionaryRef backForwardListDictionary, BackForwardListState& backForwardListState)
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

bool decodeLegacySessionState(const uint8_t* bytes, size_t size, SessionState& sessionState)
{
    if (size < sizeof(uint32_t))
        return false;

    uint32_t versionNumber = (bytes[0] << 24) + (bytes[1] << 16) + (bytes[2] << 8) + bytes[3];

    if (versionNumber != sessionStateDataVersion)
        return false;

    auto sessionStateDictionary = adoptCF(dynamic_cf_cast<CFDictionaryRef>(CFPropertyListCreateWithData(kCFAllocatorDefault, adoptCF(CFDataCreate(kCFAllocatorDefault, bytes + sizeof(uint32_t), size - sizeof(uint32_t))).get(), kCFPropertyListImmutable, nullptr, nullptr)));
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

} // namespace WebKit
