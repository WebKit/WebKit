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

#ifndef FormData_h
#define FormData_h

#include "BlobData.h"
#include "URL.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class FormDataList;
class TextEncoding;

class FormDataElement {
public:
    enum class Type {
        Data,
        EncodedFile,
        EncodedBlob,
    };

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

    template<typename Encoder>
    void encode(Encoder&) const;
    template<typename Decoder>
    static bool decode(Decoder&, FormDataElement& result);

    Type m_type;
    Vector<char> m_data;
    String m_filename;
    URL m_url; // For Blob or URL.
    int64_t m_fileStart;
    int64_t m_fileLength;
    double m_expectedFileModificationTime;
    // FIXME: Generated file support in FormData is almost identical to Blob, they should be merged.
    // We can't just switch to using Blobs for all files for two reasons:
    // 1. Not all platforms enable BLOB support.
    // 2. EncodedFile form data elements do not have a valid m_expectedFileModificationTime, meaning that we always upload the latest content from disk.
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
bool FormDataElement::decode(Decoder& decoder, FormDataElement& result)
{
    if (!decoder.decodeEnum(result.m_type))
        return false;

    switch (result.m_type) {
    case Type::Data:
        if (!decoder.decode(result.m_data))
            return false;

        return true;

    case Type::EncodedFile:
        if (!decoder.decode(result.m_filename))
            return false;
        if (!decoder.decode(result.m_generatedFilename))
            return false;
        if (!decoder.decode(result.m_shouldGenerateFile))
            return false;
        if (!decoder.decode(result.m_fileStart))
            return false;
        if (!decoder.decode(result.m_fileLength))
            return false;

        if (result.m_fileLength != BlobDataItem::toEndOfFile && result.m_fileLength < result.m_fileStart)
            return false;

        if (!decoder.decode(result.m_expectedFileModificationTime))
            return false;

        return true;

    case Type::EncodedBlob: {
        String blobURLString;
        if (!decoder.decode(blobURLString))
            return false;

        result.m_url = URL(URL(), blobURLString);

        return true;
    }
    }

    return false;
}

class FormData : public RefCounted<FormData> {
public:
    enum EncodingType {
        FormURLEncoded, // for application/x-www-form-urlencoded
        TextPlain, // for text/plain
        MultipartFormData // for multipart/form-data
    };

    static PassRefPtr<FormData> create();
    static PassRefPtr<FormData> create(const void*, size_t);
    static PassRefPtr<FormData> create(const CString&);
    static PassRefPtr<FormData> create(const Vector<char>&);
    static PassRefPtr<FormData> create(const FormDataList&, const TextEncoding&, EncodingType = FormURLEncoded);
    static PassRefPtr<FormData> createMultiPart(const FormDataList&, const TextEncoding&, Document*);
    ~FormData();

    // FIXME: Both these functions perform a deep copy of m_elements, but differ in handling of other data members.
    // How much of that is intentional? We need better names that explain the difference.
    PassRefPtr<FormData> copy() const;
    PassRefPtr<FormData> deepCopy() const;

    template<typename Encoder>
    void encode(Encoder&) const;
    template<typename Decoder>
    static PassRefPtr<FormData> decode(Decoder&);

    void appendData(const void* data, size_t);
    void appendFile(const String& filePath, bool shouldGenerateFile = false);
    void appendFileRange(const String& filename, long long start, long long length, double expectedModificationTime, bool shouldGenerateFile = false);
    void appendBlob(const URL& blobURL);
    char* expandDataStore(size_t);

    void flatten(Vector<char>&) const; // omits files
    String flattenToString() const; // omits files

    // Resolve all blob references so we only have file and data.
    // If the FormData has no blob references to resolve, this is returned.
    PassRefPtr<FormData> resolveBlobReferences();

    bool isEmpty() const { return m_elements.isEmpty(); }
    const Vector<FormDataElement>& elements() const { return m_elements; }
    const Vector<char>& boundary() const { return m_boundary; }

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
        if (equalIgnoringCase(type, "text/plain"))
            return TextPlain;
        if (equalIgnoringCase(type, "multipart/form-data"))
            return MultipartFormData;
        return FormURLEncoded;
    }

private:
    FormData();
    FormData(const FormData&);

    void appendKeyValuePairItems(const FormDataList&, const TextEncoding&, bool isMultiPartForm, Document*, EncodingType = FormURLEncoded);

    bool hasGeneratedFiles() const;
    bool hasOwnedGeneratedFiles() const;

    Vector<FormDataElement> m_elements;

    int64_t m_identifier;
    bool m_alwaysStream;
    Vector<char> m_boundary;
    bool m_containsPasswordData;
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
PassRefPtr<FormData> FormData::decode(Decoder& decoder)
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

    return data.release();
}

} // namespace WebCore

#endif
