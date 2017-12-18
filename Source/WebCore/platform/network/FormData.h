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
#include "URL.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class DOMFormData;
class Document;
class File;
class SharedBuffer;
class TextEncoding;

// FIXME: Convert this to a Variant of structs and remove "Type" and also the "m_" prefixes from the data members.
// The member functions can become non-member fucntions.
class FormDataElement {
public:
    enum class Type { Data, EncodedFile, EncodedBlob };

    FormDataElement()
        : m_type(Type::Data)
    {
    }

    explicit FormDataElement(const Vector<char>& array)
        : m_type(Type::Data)
        , m_data(array)
    {
    }

    FormDataElement(const String& filename, long long fileStart, long long fileLength, double expectedFileModificationTime, bool shouldGenerateFile)
        : m_type(Type::EncodedFile)
        , m_filename(filename)
        , m_fileStart(fileStart)
        , m_fileLength(fileLength)
        , m_expectedFileModificationTime(expectedFileModificationTime)
        , m_shouldGenerateFile(shouldGenerateFile)
        , m_ownsGeneratedFile(false)
    {
    }

    explicit FormDataElement(const URL& blobURL)
        : m_type(Type::EncodedBlob)
        , m_url(blobURL)
    {
    }

    uint64_t lengthInBytes() const;

    FormDataElement isolatedCopy() const;

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<FormDataElement> decode(Decoder&);

    Type m_type;
    Vector<char> m_data;
    String m_filename;
    URL m_url; // For Blob or URL.
    int64_t m_fileStart;
    int64_t m_fileLength;
    double m_expectedFileModificationTime;
    // FIXME: Generated file support in FormData is almost identical to Blob, they should be merged.
    // We can't just switch to using Blobs for all files because EncodedFile form data elements do not
    // have a valid m_expectedFileModificationTime, meaning we always upload the latest content from disk.
    String m_generatedFilename;
    bool m_shouldGenerateFile;
    bool m_ownsGeneratedFile;
};

inline bool operator==(const FormDataElement& a, const FormDataElement& b)
{
    if (&a == &b)
        return true;

    if (a.m_type != b.m_type)
        return false;
    if (a.m_type == FormDataElement::Type::Data)
        return a.m_data == b.m_data;
    if (a.m_type == FormDataElement::Type::EncodedFile)
        return a.m_filename == b.m_filename && a.m_fileStart == b.m_fileStart && a.m_fileLength == b.m_fileLength && a.m_expectedFileModificationTime == b.m_expectedFileModificationTime;
    if (a.m_type == FormDataElement::Type::EncodedBlob)
        return a.m_url == b.m_url;

    return true;
}

inline bool operator!=(const FormDataElement& a, const FormDataElement& b)
{
    return !(a == b);
}

template<typename Encoder>
void FormDataElement::encode(Encoder& encoder) const
{
    encoder.encodeEnum(m_type);

    switch (m_type) {
    case Type::Data:
        encoder << m_data;
        break;

    case Type::EncodedFile:
        encoder << m_filename;
        encoder << m_generatedFilename;
        encoder << m_shouldGenerateFile;
        encoder << m_fileStart;
        encoder << m_fileLength;
        encoder << m_expectedFileModificationTime;
        break;

    case Type::EncodedBlob:
        encoder << m_url.string();
        break;
    }
}

template<typename Decoder>
std::optional<FormDataElement> FormDataElement::decode(Decoder& decoder)
{
    FormDataElement result;
    if (!decoder.decodeEnum(result.m_type))
        return std::nullopt;

    switch (result.m_type) {
    case Type::Data:
        if (!decoder.decode(result.m_data))
            return std::nullopt;

        return WTFMove(result);

    case Type::EncodedFile:
        if (!decoder.decode(result.m_filename))
            return std::nullopt;
        if (!decoder.decode(result.m_generatedFilename))
            return std::nullopt;
        if (!decoder.decode(result.m_shouldGenerateFile))
            return std::nullopt;
        result.m_ownsGeneratedFile = false;
        if (!decoder.decode(result.m_fileStart))
            return std::nullopt;
        if (!decoder.decode(result.m_fileLength))
            return std::nullopt;

        if (result.m_fileLength != BlobDataItem::toEndOfFile && result.m_fileLength < result.m_fileStart)
            return std::nullopt;

        if (!decoder.decode(result.m_expectedFileModificationTime))
            return std::nullopt;

        return WTFMove(result);

    case Type::EncodedBlob: {
        String blobURLString;
        if (!decoder.decode(blobURLString))
            return std::nullopt;

        result.m_url = URL(URL(), blobURLString);

        return WTFMove(result);
    }
    }

    return std::nullopt;
}

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
    WEBCORE_EXPORT void appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime, bool shouldGenerateFile = false);
    WEBCORE_EXPORT void appendBlob(const URL& blobURL);
    char* expandDataStore(size_t);

    Vector<char> flatten() const; // omits files
    String flattenToString() const; // omits files

    // Resolve all blob references so we only have file and data.
    // If the FormData has no blob references to resolve, this is returned.
    Ref<FormData> resolveBlobReferences();

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
    // FIXME: Does not encode m_containsPasswordData. Why is that OK?
}

template<typename Decoder>
RefPtr<FormData> FormData::decode(Decoder& decoder)
{
    RefPtr<FormData> data = FormData::create();

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

