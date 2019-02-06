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

class BlobRegistry;
class DOMFormData;
class Document;
class File;
class SharedBuffer;
class TextEncoding;

struct FormDataElement {
    struct EncodedFileData;
    struct EncodedBlobData;
    using Data = Variant<Vector<char>, EncodedFileData, EncodedBlobData>;

    FormDataElement() = default;
    explicit FormDataElement(Data&& data)
        : data(WTFMove(data)) { }
    explicit FormDataElement(Vector<char>&& array)
        : data(WTFMove(array)) { }
    FormDataElement(const String& filename, int64_t fileStart, int64_t fileLength, Optional<WallTime> expectedFileModificationTime, bool shouldGenerateFile)
        : data(EncodedFileData { filename, fileStart, fileLength, expectedFileModificationTime, { }, shouldGenerateFile, false }) { }
    explicit FormDataElement(const URL& blobURL)
        : data(EncodedBlobData { blobURL }) { }

    uint64_t lengthInBytes() const;

    FormDataElement isolatedCopy() const;

    template<typename Encoder> void encode(Encoder& encoder) const
    {
        encoder << data;
    }
    template<typename Decoder> static Optional<FormDataElement> decode(Decoder& decoder)
    {
        Optional<Data> data;
        decoder >> data;
        if (!data)
            return WTF::nullopt;
        return FormDataElement(WTFMove(*data));
    }

    struct EncodedFileData {
        String filename;
        int64_t fileStart { 0 };
        int64_t fileLength { 0 };
        Optional<WallTime> expectedFileModificationTime;
        String generatedFilename;
        bool shouldGenerateFile { false };
        bool ownsGeneratedFile { false };

        // FIXME: Generated file support in FormData is almost identical to Blob, they should be merged.
        // We can't just switch to using Blobs for all files because EncodedFile form data elements do not
        // have a valid expectedFileModificationTime, meaning we always upload the latest content from disk.

        EncodedFileData isolatedCopy() const
        {
            return { filename.isolatedCopy(), fileStart, fileLength, expectedFileModificationTime, generatedFilename.isolatedCopy(), shouldGenerateFile, ownsGeneratedFile };
        }
        
        bool operator==(const EncodedFileData& other) const
        {
            return filename == other.filename
                && fileStart == other.fileStart
                && fileLength == other.fileLength
                && expectedFileModificationTime == other.expectedFileModificationTime
                && generatedFilename == other.generatedFilename
                && shouldGenerateFile == other.shouldGenerateFile
                && ownsGeneratedFile == other.ownsGeneratedFile;
        }
        template<typename Encoder> void encode(Encoder& encoder) const
        {
            encoder << filename << fileStart << fileLength << expectedFileModificationTime << generatedFilename << shouldGenerateFile;
        }
        template<typename Decoder> static Optional<EncodedFileData> decode(Decoder& decoder)
        {
            Optional<String> filename;
            decoder >> filename;
            if (!filename)
                return WTF::nullopt;
            
            Optional<int64_t> fileStart;
            decoder >> fileStart;
            if (!fileStart)
                return WTF::nullopt;
            
            Optional<int64_t> fileLength;
            decoder >> fileLength;
            if (!fileLength)
                return WTF::nullopt;
            
            Optional<Optional<WallTime>> expectedFileModificationTime;
            decoder >> expectedFileModificationTime;
            if (!expectedFileModificationTime)
                return WTF::nullopt;
            
            Optional<String> generatedFilename;
            decoder >> generatedFilename;
            if (!generatedFilename)
                return WTF::nullopt;

            Optional<bool> shouldGenerateFile;
            decoder >> shouldGenerateFile;
            if (!shouldGenerateFile)
                return WTF::nullopt;

            bool ownsGeneratedFile = false;
            
            return {{
                WTFMove(*filename),
                WTFMove(*fileStart),
                WTFMove(*fileLength),
                WTFMove(*expectedFileModificationTime),
                WTFMove(*generatedFilename),
                WTFMove(*shouldGenerateFile),
                WTFMove(ownsGeneratedFile)
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
        template<typename Decoder> static Optional<EncodedBlobData> decode(Decoder& decoder)
        {
            Optional<URL> url;
            decoder >> url;
            if (!url)
                return WTF::nullopt;

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

class FormData : public RefCounted<FormData> {
public:
    enum EncodingType {
        FormURLEncoded, // for application/x-www-form-urlencoded
        TextPlain, // for text/plain
        MultipartFormData // for multipart/form-data
    };

    WEBCORE_EXPORT static Ref<FormData> create();
    WEBCORE_EXPORT static Ref<FormData> create(const void*, size_t);
    static Ref<FormData> create(const CString&);
    static Ref<FormData> create(Vector<char>&&);
    static Ref<FormData> create(const Vector<char>&);
    static Ref<FormData> create(const Vector<uint8_t>&);
    static Ref<FormData> create(const DOMFormData&, EncodingType = FormURLEncoded);
    static Ref<FormData> createMultiPart(const DOMFormData&, Document*);
    WEBCORE_EXPORT ~FormData();

    // FIXME: Both these functions perform a deep copy of m_elements, but differ in handling of other data members.
    // How much of that is intentional? We need better names that explain the difference.
    Ref<FormData> copy() const;
    Ref<FormData> isolatedCopy() const;

    template<typename Encoder>
    void encode(Encoder&) const;
    template<typename Decoder>
    static RefPtr<FormData> decode(Decoder&);

    WEBCORE_EXPORT void appendData(const void* data, size_t);
    void appendFile(const String& filePath, bool shouldGenerateFile = false);
    WEBCORE_EXPORT void appendFileRange(const String& filename, long long start, long long length, Optional<WallTime> expectedModificationTime, bool shouldGenerateFile = false);
    WEBCORE_EXPORT void appendBlob(const URL& blobURL);

    WEBCORE_EXPORT Vector<char> flatten() const; // omits files
    String flattenToString() const; // omits files

    // Resolve all blob references so we only have file and data.
    // If the FormData has no blob references to resolve, this is returned.
    WEBCORE_EXPORT Ref<FormData> resolveBlobReferences(BlobRegistry&);

    bool isEmpty() const { return m_elements.isEmpty(); }
    const Vector<FormDataElement>& elements() const { return m_elements; }
    const Vector<char>& boundary() const { return m_boundary; }

    RefPtr<SharedBuffer> asSharedBuffer() const;

    void generateFiles(Document*);
    void removeGeneratedFilesIfNeeded();

    bool alwaysStream() const { return m_alwaysStream; }
    void setAlwaysStream(bool alwaysStream) { m_alwaysStream = alwaysStream; }

    // Identifies a particular form submission instance.  A value of 0 is used
    // to indicate an unspecified identifier.
    void setIdentifier(int64_t identifier) { m_identifier = identifier; }
    int64_t identifier() const { return m_identifier; }

    bool containsPasswordData() const { return m_containsPasswordData; }
    void setContainsPasswordData(bool containsPasswordData) { m_containsPasswordData = containsPasswordData; }

    static EncodingType parseEncodingType(const String& type)
    {
        if (equalLettersIgnoringASCIICase(type, "text/plain"))
            return TextPlain;
        if (equalLettersIgnoringASCIICase(type, "multipart/form-data"))
            return MultipartFormData;
        return FormURLEncoded;
    }

    uint64_t lengthInBytes() const;

    WEBCORE_EXPORT URL asBlobURL() const;

private:
    FormData();
    FormData(const FormData&);

    void appendMultiPartFileValue(const File&, Vector<char>& header, TextEncoding&, Document*);
    void appendMultiPartStringValue(const String&, Vector<char>& header, TextEncoding&);
    void appendMultiPartKeyValuePairItems(const DOMFormData&, Document*);
    void appendNonMultiPartKeyValuePairItems(const DOMFormData&, EncodingType);

    bool hasGeneratedFiles() const;
    bool hasOwnedGeneratedFiles() const;

    Vector<FormDataElement> m_elements;

    int64_t m_identifier { 0 };
    bool m_alwaysStream { false };
    Vector<char> m_boundary;
    bool m_containsPasswordData { false };
    mutable Optional<uint64_t> m_lengthInBytes;
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
    // FIXME: Does not encode m_containsPasswordData. Why is that OK?
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

    return WTFMove(data);
}

} // namespace WebCore

