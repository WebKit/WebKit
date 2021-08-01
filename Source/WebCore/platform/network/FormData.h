/*
 * Copyright (C) 2004, 2006, 2008, 2011 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB. If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "BlobData.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class BlobRegistryImpl;
class DOMFormData;
class File;
class SharedBuffer;
class TextEncoding;

struct FormDataElement {
    struct EncodedFileData;
    struct EncodedBlobData;
    using Data = Variant<Vector<uint8_t>, EncodedFileData, EncodedBlobData>;

    FormDataElement() = default;
    explicit FormDataElement(Data&& data)
        : data(WTFMove(data)) { }
    explicit FormDataElement(Vector<uint8_t>&& array)
        : data(WTFMove(array)) { }
    FormDataElement(const String& filename, int64_t fileStart, int64_t fileLength, std::optional<WallTime> expectedFileModificationTime)
        : data(EncodedFileData { filename, fileStart, fileLength, expectedFileModificationTime }) { }
    explicit FormDataElement(const URL& blobURL)
        : data(EncodedBlobData { blobURL }) { }

    uint64_t lengthInBytes(const Function<uint64_t(const URL&)>&) const;
    uint64_t lengthInBytes() const;

    FormDataElement isolatedCopy() const;

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        encoder << data;
    }
    template<typename Decoder> static std::optional<FormDataElement> decode(Decoder& decoder)
    {
        std::optional<Data> data;
        decoder >> data;
        if (!data)
            return std::nullopt;
        return FormDataElement(WTFMove(*data));
    }

    struct EncodedFileData {
        String filename;
        int64_t fileStart { 0 };
        int64_t fileLength { 0 };
        std::optional<WallTime> expectedFileModificationTime;

        bool fileModificationTimeMatchesExpectation() const;

        EncodedFileData isolatedCopy() const
        {
            return { filename.isolatedCopy(), fileStart, fileLength, expectedFileModificationTime };
        }
        
        bool operator==(const EncodedFileData& other) const
        {
            return filename == other.filename
                && fileStart == other.fileStart
                && fileLength == other.fileLength
                && expectedFileModificationTime == other.expectedFileModificationTime;
        }
        template<typename Encoder> void encode(Encoder& encoder) const
        {
            encoder << filename << fileStart << fileLength << expectedFileModificationTime;
        }
        template<typename Decoder> static std::optional<EncodedFileData> decode(Decoder& decoder)
        {
            std::optional<String> filename;
            decoder >> filename;
            if (!filename)
                return std::nullopt;
            
            std::optional<int64_t> fileStart;
            decoder >> fileStart;
            if (!fileStart)
                return std::nullopt;
            
            std::optional<int64_t> fileLength;
            decoder >> fileLength;
            if (!fileLength)
                return std::nullopt;
            
            std::optional<std::optional<WallTime>> expectedFileModificationTime;
            decoder >> expectedFileModificationTime;
            if (!expectedFileModificationTime)
                return std::nullopt;

            return {{
                WTFMove(*filename),
                WTFMove(*fileStart),
                WTFMove(*fileLength),
                WTFMove(*expectedFileModificationTime)
            }};
        }

    };
    
    struct EncodedBlobData {
        URL url;

        bool operator==(const EncodedBlobData& other) const
        {
            return url == other.url;
        }
        template<typename Encoder> void encode(Encoder& encoder) const
        {
            encoder << url;
        }
        template<typename Decoder> static std::optional<EncodedBlobData> decode(Decoder& decoder)
        {
            std::optional<URL> url;
            decoder >> url;
            if (!url)
                return std::nullopt;

            return {{ WTFMove(*url) }};
        }
    };
    
    bool operator==(const FormDataElement& other) const
    {
        if (&other == this)
            return true;
        if (data.index() != other.data.index())
            return false;
        if (!data.index())
            return WTF::get<0>(data) == WTF::get<0>(other.data);
        if (data.index() == 1)
            return WTF::get<1>(data) == WTF::get<1>(other.data);
        return WTF::get<2>(data) == WTF::get<2>(other.data);
    }
    bool operator!=(const FormDataElement& other) const
    {
        return !(*this == other);
    }
    
    Data data;
};

class FormData;

struct FormDataForUpload {
public:
    FormDataForUpload(FormDataForUpload&&) = default;
    ~FormDataForUpload();

    FormData& data() { return m_data.get(); }
private:
    friend class FormData;
    FormDataForUpload(FormData&, Vector<String>&&);
    
    Ref<FormData> m_data;
    Vector<String> m_temporaryZipFiles;
};

class FormData : public RefCounted<FormData> {
public:
    enum EncodingType {
        FormURLEncoded, // for application/x-www-form-urlencoded
        TextPlain, // for text/plain
        MultipartFormData // for multipart/form-data
    };

    WEBCORE_EXPORT static Ref<FormData> create();
    WEBCORE_EXPORT static Ref<FormData> create(const void*, size_t);
    WEBCORE_EXPORT static Ref<FormData> create(const CString&);
    static Ref<FormData> create(Vector<uint8_t>&&);
    static Ref<FormData> create(const Vector<char>&);
    static Ref<FormData> create(const Vector<uint8_t>&);
    static Ref<FormData> create(const DOMFormData&, EncodingType = FormURLEncoded);
    static Ref<FormData> createMultiPart(const DOMFormData&);
    WEBCORE_EXPORT ~FormData();

    // FIXME: Both these functions perform a deep copy of m_elements, but differ in handling of other data members.
    // How much of that is intentional? We need better names that explain the difference.
    Ref<FormData> copy() const;
    WEBCORE_EXPORT Ref<FormData> isolatedCopy() const;

    template<typename Encoder>
    void encode(Encoder&) const;
    template<typename Decoder>
    static RefPtr<FormData> decode(Decoder&);

    WEBCORE_EXPORT void appendData(const void* data, size_t);
    void appendFile(const String& filePath);
    WEBCORE_EXPORT void appendFileRange(const String& filename, long long start, long long length, std::optional<WallTime> expectedModificationTime);
    WEBCORE_EXPORT void appendBlob(const URL& blobURL);

    WEBCORE_EXPORT Vector<uint8_t> flatten() const; // omits files
    String flattenToString() const; // omits files

    // Resolve all blob references so we only have file and data.
    // If the FormData has no blob references to resolve, this is returned.
    WEBCORE_EXPORT Ref<FormData> resolveBlobReferences(BlobRegistryImpl* = nullptr);
    bool containsBlobElement() const;

    WEBCORE_EXPORT FormDataForUpload prepareForUpload();

    bool isEmpty() const { return m_elements.isEmpty(); }
    const Vector<FormDataElement>& elements() const { return m_elements; }
    const Vector<char>& boundary() const { return m_boundary; }

    WEBCORE_EXPORT RefPtr<SharedBuffer> asSharedBuffer() const;

    bool alwaysStream() const { return m_alwaysStream; }
    void setAlwaysStream(bool alwaysStream) { m_alwaysStream = alwaysStream; }

    // Identifies a particular form submission instance.  A value of 0 is used
    // to indicate an unspecified identifier.
    void setIdentifier(int64_t identifier) { m_identifier = identifier; }
    int64_t identifier() const { return m_identifier; }

    unsigned imageOrMediaFilesCount() const;

    static EncodingType parseEncodingType(const String& type)
    {
        if (equalLettersIgnoringASCIICase(type, "text/plain"))
            return TextPlain;
        if (equalLettersIgnoringASCIICase(type, "multipart/form-data"))
            return MultipartFormData;
        return FormURLEncoded;
    }

    WEBCORE_EXPORT uint64_t lengthInBytes() const;

    WEBCORE_EXPORT URL asBlobURL() const;

private:
    FormData();
    FormData(const FormData&);

    void appendMultiPartFileValue(const File&, Vector<char>& header, TextEncoding&);
    void appendMultiPartStringValue(const String&, Vector<char>& header, TextEncoding&);
    void appendMultiPartKeyValuePairItems(const DOMFormData&);
    void appendNonMultiPartKeyValuePairItems(const DOMFormData&, EncodingType);

    Vector<FormDataElement> m_elements;

    int64_t m_identifier { 0 };
    bool m_alwaysStream { false };
    Vector<char> m_boundary;
    mutable std::optional<uint64_t> m_lengthInBytes;
};

inline bool operator==(const FormData& a, const FormData& b)
{
    return a.elements() == b.elements();
}

inline bool operator!=(const FormData& a, const FormData& b)
{
    return !(a == b);
}

template<typename Encoder>
void FormData::encode(Encoder& encoder) const
{
    encoder << m_alwaysStream;
    encoder << m_boundary;
    encoder << m_elements;
    encoder << m_identifier;
}

template<typename Decoder>
RefPtr<FormData> FormData::decode(Decoder& decoder)
{
    auto data = FormData::create();

    if (!decoder.decode(data->m_alwaysStream))
        return nullptr;

    if (!decoder.decode(data->m_boundary))
        return nullptr;

    if (!decoder.decode(data->m_elements))
        return nullptr;

    if (!decoder.decode(data->m_identifier))
        return nullptr;

    return data;
}

} // namespace WebCore

