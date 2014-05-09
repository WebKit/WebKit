/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef File_h
#define File_h

#include "Blob.h"
#include <wtf/PassRefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class URL;

class File final : public Blob {
public:
    static PassRefPtr<File> create(const String& path)
    {
        return adoptRef(new File(path));
    }

    static PassRefPtr<File> deserialize(const String& path, const URL& srcURL, const String& type, const String& name)
    {
        return adoptRef(new File(deserializationContructor, path, srcURL, type, name));
    }

    // Create a file with a name exposed to the author (via File.name and associated DOM properties) that differs from the one provided in the path.
    static PassRefPtr<File> createWithName(const String& path, const String& nameOverride)
    {
        if (nameOverride.isEmpty())
            return adoptRef(new File(path));
        return adoptRef(new File(path, nameOverride));
    }

    virtual bool isFile() const override { return true; }

    const String& path() const { return m_path; }
    const String& name() const { return m_name; }

    // This returns the current date and time if the file's last modification date is not known (per spec: http://www.w3.org/TR/FileAPI/#dfn-lastModifiedDate).
    double lastModifiedDate() const;

    static String contentTypeForFile(const String& path);

#if ENABLE(FILE_REPLACEMENT)
    static bool shouldReplaceFile(const String& path);
#endif

private:
    explicit File(const String& path);
    File(const String& path, const String& nameOverride);

    File(DeserializationContructor, const String& path, const URL& srcURL, const String& type, const String& name);

    static void computeNameAndContentType(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType);
#if ENABLE(FILE_REPLACEMENT)
    static void computeNameAndContentTypeForReplacedFile(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType);
#endif

    String m_path;
    String m_name;
};

inline File* toFile(Blob* blob)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!blob || blob->isFile());
    return static_cast<File*>(blob);
}

inline const File* toFile(const Blob* blob)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!blob || blob->isFile());
    return static_cast<const File*>(blob);
}

} // namespace WebCore

#endif // File_h
