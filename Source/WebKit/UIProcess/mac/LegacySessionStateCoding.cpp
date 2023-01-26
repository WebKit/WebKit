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
#include <wtf/CheckedArithmetic.h>
#include <wtf/MallocPtr.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/StringView.h>

namespace WebKit {

// Session state keys.
static const uint32_t sessionStateDataVersion = 2;

static const CFStringRef sessionHistoryKey = CFSTR("SessionHistory");
static const CFStringRef provisionalURLKey = CFSTR("ProvisionalURL");
static const CFStringRef renderTreeSizeKey = CFSTR("RenderTreeSize");
static const CFStringRef isAppInitiatedKey = CFSTR("IsAppInitiated");

// Session history keys.
static const uint32_t sessionHistoryVersion = 1;

static const CFStringRef sessionHistoryVersionKey = CFSTR("SessionHistoryVersion");
static const CFStringRef sessionHistoryCurrentIndexKey = CFSTR("SessionHistoryCurrentIndex");
static const CFStringRef sessionHistoryEntriesKey = CFSTR("SessionHistoryEntries");

// Session history entry keys.
static const CFStringRef sessionHistoryEntryURLKey = CFSTR("SessionHistoryEntryURL");
static const CFStringRef sessionHistoryEntryTitleKey = CFSTR("SessionHistoryEntryTitle");
static const CFStringRef sessionHistoryEntryOriginalURLKey = CFSTR("SessionHistoryEntryOriginalURL");
static const CFStringRef sessionHistoryEntryDataKey = CFSTR("SessionHistoryEntryData");
static const CFStringRef sessionHistoryEntryShouldOpenExternalURLsPolicyKey = CFSTR("SessionHistoryEntryShouldOpenExternalURLsPolicyKey");

// Session history entry data.
const uint32_t sessionHistoryEntryDataVersion = 2;

// Maximum size for subframe session data.
#if PLATFORM(IOS_FAMILY)
static const uint32_t maximumSessionStateDataSize = 2 * 1024 * 1024;
#else
static const uint32_t maximumSessionStateDataSize = std::numeric_limits<uint32_t>::max();
#endif

template<typename T> void isValidEnum(T);


DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HistoryEntryDataEncoder);
DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HistoryEntryDataEncoder);

class HistoryEntryDataEncoder {
public:
    HistoryEntryDataEncoder()
        : m_bufferSize(0)
        , m_bufferCapacity(512)
        , m_buffer(MallocPtr<uint8_t, HistoryEntryDataEncoderMalloc>::malloc(m_bufferCapacity))
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

#if PLATFORM(IOS_FAMILY)
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

    MallocPtr<uint8_t, HistoryEntryDataEncoderMalloc> finishEncoding(size_t& size)
    {
        size = m_bufferSize;
        return WTFMove(m_buffer);
    }

private:
    template<typename Type>
    HistoryEntryDataEncoder& encodeArithmeticType(Type value)
    {
        static_assert(std::is_arithmetic<Type>::value);

        encodeFixedLengthData(reinterpret_cast<uint8_t*>(&value), sizeof(value), sizeof(value));
        return *this;
    }

    void encodeFixedLengthData(const uint8_t* data, size_t size, unsigned alignment)
    {
        RELEASE_ASSERT(data || !size);
        ASSERT(!(reinterpret_cast<uintptr_t>(data) % alignment));

        uint8_t* buffer = grow(alignment, size);
        memcpy(buffer, data, size);
    }

    uint8_t* grow(unsigned alignment, size_t size)
    {
        size_t alignedSize = ((m_bufferSize + alignment - 1) / alignment) * alignment;

        Checked<size_t> bufferSize = size;
        bufferSize += alignedSize;

        growCapacity(bufferSize.value());

        std::memset(m_buffer.get() + m_bufferSize, 0, alignedSize - m_bufferSize);

        m_bufferSize = bufferSize.value();
        m_bufferPointer = m_buffer.get() + m_bufferSize;

        return m_buffer.get() + alignedSize;
    }

    void growCapacity(size_t newSize)
    {
        if (newSize <= m_bufferCapacity)
            return;

        Checked<size_t> newCapacity = m_bufferCapacity;
        while (newCapacity < newSize)
            newCapacity *= 2U;

        m_buffer.realloc(newCapacity.value());
        m_bufferCapacity = newCapacity.value();
    }

    size_t m_bufferSize;
    size_t m_bufferCapacity;
    MallocPtr<uint8_t, HistoryEntryDataEncoderMalloc> m_buffer;
    uint8_t* m_bufferPointer;
};

enum class FormDataElementType {
    Data = 0,
    EncodedFile = 1,
    EncodedBlob = 2,
};

static void encodeFormDataElement(HistoryEntryDataEncoder& encoder, const HTTPBody::Element& element)
{
    encoder << static_cast<uint32_t>(element.data.index());
    WTF::switchOn(element.data, [&] (const Vector<uint8_t>& data) {
        encoder << data;
    }, [&] (const HTTPBody::Element::FileData& fileData) {
        encoder << fileData.filePath;

        // Used to be generatedFilename.
        encoder << String();

        // Used to be shouldGenerateFile.
        encoder << false;

        encoder << fileData.fileStart;
        encoder << fileData.fileLength.value_or(-1);
        encoder << fileData.expectedFileModificationTime.value_or(WallTime::nan()).secondsSinceEpoch().value();

    }, [&] (const String& blobURLString) {
        encoder << blobURLString;
    });
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

    FrameState::validateDocumentState(frameState.documentState());
    encoder << static_cast<uint64_t>(frameState.documentState().size());
    for (const auto& documentState : frameState.documentState())
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

    encoder << frameState.scrollPosition.x();
    encoder << frameState.scrollPosition.y();

    encoder << frameState.pageScaleFactor;

    encoder << !!frameState.stateObjectData;
    if (frameState.stateObjectData)
        encoder << frameState.stateObjectData.value();

    encoder << frameState.target;

#if PLATFORM(IOS_FAMILY)
    // FIXME: iOS should not use the legacy session state encoder.
    encoder << frameState.exposedContentRect;
    encoder << frameState.unobscuredContentRect;
    encoder << frameState.minimumLayoutSizeInScrollViewCoordinates;
    encoder << frameState.contentSize;
    encoder << frameState.scaleIsInitial;
#endif
}

static MallocPtr<uint8_t, HistoryEntryDataEncoderMalloc> encodeSessionHistoryEntryData(const FrameState& frameState, size_t& bufferSize)
{
    HistoryEntryDataEncoder encoder;

    encoder << sessionHistoryEntryDataVersion;
    encodeFrameStateNode(encoder, frameState);

    return encoder.finishEncoding(bufferSize);
}

static RetainPtr<CFDataRef> encodeSessionHistoryEntryData(const FrameState& frameState)
{
    static NeverDestroyed<RetainPtr<CFAllocatorRef>> fastMallocDeallocator;

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
                HistoryEntryDataEncoderMalloc::free(ptr);
            },
            nullptr, // preferredSize
        };
        fastMallocDeallocator.get() = adoptCF(CFAllocatorCreate(kCFAllocatorDefault, &context));
    });

    size_t bufferSize;
    auto buffer = encodeSessionHistoryEntryData(frameState, bufferSize);

    return adoptCF(CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, buffer.leakPtr(), bufferSize, fastMallocDeallocator.get().get()));
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
    size_t totalDataSize = 0;

    for (const auto& item : backForwardListState.items) {
        auto url = item.pageState.mainFrameState.urlString.createCFString();
        auto title = item.pageState.title.createCFString();
        auto originalURL = item.pageState.mainFrameState.originalURLString.createCFString();
        auto data = totalDataSize <= maximumSessionStateDataSize ? encodeSessionHistoryEntryData(item.pageState.mainFrameState) : nullptr;
        auto shouldOpenExternalURLsPolicyValue = static_cast<uint64_t>(item.pageState.shouldOpenExternalURLsPolicy);
        auto shouldOpenExternalURLsPolicy = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &shouldOpenExternalURLsPolicyValue));

        RetainPtr<CFDictionaryRef> entryDictionary;

        if (data) {
            totalDataSize += CFDataGetLength(data.get());

            entryDictionary = createDictionary({
                { sessionHistoryEntryURLKey, url.get() },
                { sessionHistoryEntryTitleKey, title.get() },
                { sessionHistoryEntryOriginalURLKey, originalURL.get() },
                { sessionHistoryEntryDataKey, data.get() },
                { sessionHistoryEntryShouldOpenExternalURLsPolicyKey, shouldOpenExternalURLsPolicy.get() },
            });
        } else {
            entryDictionary = createDictionary({
                { sessionHistoryEntryURLKey, url.get() },
                { sessionHistoryEntryTitleKey, title.get() },
                { sessionHistoryEntryOriginalURLKey, originalURL.get() },
                { sessionHistoryEntryShouldOpenExternalURLsPolicyKey, shouldOpenExternalURLsPolicy.get() },
            });
        }

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
    RetainPtr<CFNumberRef> renderTreeSizeNumber(adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &sessionState.renderTreeSize)));
    RetainPtr<CFBooleanRef> isAppInitiated = adoptCF(sessionState.isAppInitiated ? kCFBooleanTrue : kCFBooleanFalse);

    RetainPtr<CFDictionaryRef> stateDictionary;
    if (provisionalURLString) {
        stateDictionary = createDictionary({
            { sessionHistoryKey, sessionHistoryDictionary.get() },
            { provisionalURLKey, provisionalURLString.get() },
            { renderTreeSizeKey, renderTreeSizeNumber.get() },
            { isAppInitiatedKey, isAppInitiated.get() }
        });
    } else {
        stateDictionary = createDictionary({
            { sessionHistoryKey, sessionHistoryDictionary.get() },
            { renderTreeSizeKey, renderTreeSizeNumber.get() },
            { isAppInitiatedKey, isAppInitiated.get() }
        });
    }

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
    auto buffer = MallocPtr<uint8_t, HistoryEntryDataEncoderMalloc>::tryMalloc(bufferSize);
    if (!buffer)
        return nullptr;

    // Put the session state version number at the start of the buffer
    buffer.get()[0] = (sessionStateDataVersion & 0xff000000) >> 24;
    buffer.get()[1] = (sessionStateDataVersion & 0x00ff0000) >> 16;
    buffer.get()[2] = (sessionStateDataVersion & 0x0000ff00) >> 8;
    buffer.get()[3] = (sessionStateDataVersion & 0x000000ff);

    // Copy in the actual session state data
    CFDataGetBytes(data.get(), CFRangeMake(0, length), buffer.get() + sizeof(uint32_t));

    return API::Data::createWithoutCopying(buffer.leakPtr(), bufferSize, [] (unsigned char* buffer, const void* context) {
        HistoryEntryDataEncoderMalloc::free(buffer);
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

    HistoryEntryDataDecoder& operator>>(AtomString& value)
    {
        // FIXME: This could be more efficient but this matches what the IPC decoder currently does.
        String string;
        *this >> string;
        value = AtomString { string };
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

#if PLATFORM(IOS_FAMILY)
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
    auto operator>>(std::optional<T>& value) -> typename std::enable_if<std::is_enum<T>::value, HistoryEntryDataDecoder&>::type
    {
        uint32_t underlyingEnumValue;
        *this >> underlyingEnumValue;

        if (!isValid() || !isValidEnum(static_cast<T>(underlyingEnumValue)))
            value = std::nullopt;
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
        static_assert(std::is_arithmetic<Type>::value);
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
    uint32_t elementType;
    decoder >> elementType;
    if (!decoder.isValid())
        return;

    switch (elementType) {
    case WTF::alternativeIndexV<Vector<uint8_t>, HTTPBody::Element::Data>: {
        Vector<uint8_t> data;
        decoder >> data;
        formDataElement.data = WTFMove(data);
        break;
    }

    case WTF::alternativeIndexV<HTTPBody::Element::FileData, HTTPBody::Element::Data>: {
        HTTPBody::Element::FileData fileData;
        decoder >> fileData.filePath;

        String generatedFilename;
        decoder >> generatedFilename;

        bool shouldGenerateFile;
        decoder >> shouldGenerateFile;

        decoder >> fileData.fileStart;
        if (fileData.fileStart < 0) {
            decoder.markInvalid();
            return;
        }

        int64_t fileLength;
        decoder >> fileLength;
        if (fileLength != -1) {
            if (fileLength < fileData.fileStart)
                return;

            fileData.fileLength = fileLength;
        }

        double expectedFileModificationTime;
        decoder >> expectedFileModificationTime;
        if (!std::isnan(expectedFileModificationTime))
            fileData.expectedFileModificationTime = WallTime::fromRawSeconds(expectedFileModificationTime);

        formDataElement.data = WTFMove(fileData);
        break;
    }

    case WTF::alternativeIndexV<String, HTTPBody::Element::Data>: {
        String blobURLString;
        decoder >> blobURLString;
        formDataElement.data = WTFMove(blobURLString);
        break;
    }
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

        formData.elements.append(WTFMove(formDataElement));
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

        frameState.children.append(WTFMove(childFrameState));
    }

    decoder >> frameState.documentSequenceNumber;

    uint64_t documentStateVectorSize;
    decoder >> documentStateVectorSize;

    Vector<AtomString> documentState;
    for (uint64_t i = 0; i < documentStateVectorSize; ++i) {
        AtomString state;
        decoder >> state;

        if (!decoder.isValid())
            return;

        documentState.append(WTFMove(state));
    }
    frameState.setDocumentState(documentState, FrameState::ShouldValidate::Yes);

    String formContentType;
    decoder >> formContentType;

    bool hasFormData;
    decoder >> hasFormData;

    if (hasFormData) {
        HTTPBody httpBody;
        httpBody.contentType = WTFMove(formContentType);

        decodeFormData(decoder, httpBody);

        frameState.httpBody = WTFMove(httpBody);
    }

    decoder >> frameState.itemSequenceNumber;

    decoder >> frameState.referrer;

    int32_t scrollPositionX;
    decoder >> scrollPositionX;

    int32_t scrollPositionY;
    decoder >> scrollPositionY;

    frameState.scrollPosition = WebCore::IntPoint(scrollPositionX, scrollPositionY);

    decoder >> frameState.pageScaleFactor;

    bool hasStateObject;
    decoder >> hasStateObject;

    if (hasStateObject) {
        Vector<uint8_t> stateObjectData;
        decoder >> stateObjectData;

        frameState.stateObjectData = WTFMove(stateObjectData);
    }

    decoder >> frameState.target;

#if PLATFORM(IOS_FAMILY)
    // FIXME: iOS should not use the legacy session state decoder.
    decoder >> frameState.exposedContentRect;
    decoder >> frameState.unobscuredContentRect;
    decoder >> frameState.minimumLayoutSizeInScrollViewCoordinates;
    decoder >> frameState.contentSize;
    decoder >> frameState.scaleIsInitial;
#endif
}

static WARN_UNUSED_RETURN bool decodeSessionHistoryEntryData(const uint8_t* buffer, size_t bufferSize, FrameState& mainFrameState)
{
    HistoryEntryDataDecoder decoder { buffer, bufferSize };

    uint32_t version;
    decoder >> version;

    if (version != sessionHistoryEntryDataVersion)
        return false;

    decodeBackForwardTreeNode(decoder, mainFrameState);

    return decoder.finishDecoding();
}

static WARN_UNUSED_RETURN bool decodeSessionHistoryEntryData(CFDataRef historyEntryData, FrameState& mainFrameState)
{
    return decodeSessionHistoryEntryData(CFDataGetBytePtr(historyEntryData), static_cast<size_t>(CFDataGetLength(historyEntryData)), mainFrameState);
}

static WARN_UNUSED_RETURN bool decodeSessionHistoryEntry(CFDictionaryRef entryDictionary, BackForwardListItemState& backForwardListItemState)
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

    auto rawShouldOpenExternalURLsPolicy = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(entryDictionary, sessionHistoryEntryShouldOpenExternalURLsPolicyKey));
    WebCore::ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy;
    if (rawShouldOpenExternalURLsPolicy) {
        uint64_t value;
        CFNumberGetValue(rawShouldOpenExternalURLsPolicy, kCFNumberSInt64Type, &value);
        shouldOpenExternalURLsPolicy = static_cast<WebCore::ShouldOpenExternalURLsPolicy>(value);
    } else
        shouldOpenExternalURLsPolicy = WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemesButNotAppLinks;

    if (!decodeSessionHistoryEntryData(historyEntryData, backForwardListItemState.pageState.mainFrameState))
        return false;

    backForwardListItemState.pageState.title = title;
    backForwardListItemState.pageState.shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    backForwardListItemState.pageState.mainFrameState.urlString = urlString;
    backForwardListItemState.pageState.mainFrameState.originalURLString = originalURLString;

    return true;
}

static WARN_UNUSED_RETURN bool decodeSessionHistoryEntries(CFArrayRef entriesArray, Vector<BackForwardListItemState>& entries)
{
    for (CFIndex i = 0, size = CFArrayGetCount(entriesArray); i < size; ++i) {
        auto entryDictionary = dynamic_cf_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(entriesArray, i));
        if (!entryDictionary)
            return false;

        BackForwardListItemState entry;
        if (!decodeSessionHistoryEntry(entryDictionary, entry))
            return false;

        entries.append(WTFMove(entry));
    }

    return true;
}

static WARN_UNUSED_RETURN bool decodeV0SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState)
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

static WARN_UNUSED_RETURN bool decodeV1SessionHistory(CFDictionaryRef sessionHistoryDictionary, BackForwardListState& backForwardListState)
{
    auto currentIndexNumber = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionHistoryDictionary, sessionHistoryCurrentIndexKey));
    if (!currentIndexNumber) {
        // No current index means the dictionary represents an empty session.
        backForwardListState.currentIndex = std::nullopt;
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

static WARN_UNUSED_RETURN bool decodeSessionHistory(CFDictionaryRef backForwardListDictionary, BackForwardListState& backForwardListState)
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

    auto cfPropertyList = adoptCF(CFPropertyListCreateWithData(kCFAllocatorDefault, adoptCF(CFDataCreate(kCFAllocatorDefault, bytes + sizeof(uint32_t), size - sizeof(uint32_t))).get(), kCFPropertyListImmutable, nullptr, nullptr));
    auto sessionStateDictionary = dynamic_cf_cast<CFDictionaryRef>(cfPropertyList.get());
    if (!sessionStateDictionary)
        return false;

    if (auto backForwardListDictionary = dynamic_cf_cast<CFDictionaryRef>(CFDictionaryGetValue(sessionStateDictionary, sessionHistoryKey))) {
        if (!decodeSessionHistory(backForwardListDictionary, sessionState.backForwardListState))
            return false;
    }

    if (auto provisionalURLString = dynamic_cf_cast<CFStringRef>(CFDictionaryGetValue(sessionStateDictionary, provisionalURLKey))) {
        sessionState.provisionalURL = URL { provisionalURLString };
        if (!sessionState.provisionalURL.isValid())
            return false;
    }

    if (auto renderTreeSize = dynamic_cf_cast<CFNumberRef>(CFDictionaryGetValue(sessionStateDictionary, renderTreeSizeKey)))
        CFNumberGetValue(renderTreeSize, kCFNumberSInt64Type, &sessionState.renderTreeSize);
    else
        sessionState.renderTreeSize = 0;

    if (auto isAppInitiated = dynamic_cf_cast<CFBooleanRef>(CFDictionaryGetValue(sessionStateDictionary, isAppInitiatedKey)))
        sessionState.isAppInitiated = isAppInitiated == kCFBooleanTrue;
    else
        sessionState.isAppInitiated = true;

    return true;
}

} // namespace WebKit
