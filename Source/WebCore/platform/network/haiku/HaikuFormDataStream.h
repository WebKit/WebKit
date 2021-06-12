/*
    Copyright (C) 2018 Haiku, inc.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef HAIKUFORMDATASTREAM_H
#define HAIKUFORMDATASTREAM_H

#include "wtf/FileSystem.h"
#include "FormData.h"
#include <pal/SessionID.h>

#include <DataIO.h>
#include <File.h>

namespace WebCore {

class BFormDataIO : public BDataIO
{
public:
	BFormDataIO(const FormData* form, PAL::SessionID sessionID);
	~BFormDataIO();
	
	ssize_t Size();
	ssize_t Read(void* buffer, size_t size) override;
	ssize_t Write(const void* buffer, size_t size) override;
	
private:
    void computeContentLength();

    std::optional<size_t> readFromFile(const FormDataElement::EncodedFileData&, char*, size_t);
    std::optional<size_t> readFromData(const Vector<uint8_t>&, char*, size_t);
	std::optional<size_t> readFromBlob(const FormDataElement::EncodedBlobData&, char*, size_t);

    RefPtr<FormData> m_formData;
	PAL::SessionID m_sessionID;

    bool m_isContentLengthUpdated { false };
    unsigned long long m_totalSize { 0 };
    unsigned long long m_totalReadSize { 0 };

    size_t m_elementPosition { 0 };
    size_t m_blobItemIndex { 0 };

    FileSystem::PlatformFileHandle m_fileHandle { FileSystem::invalidPlatformFileHandle };
    size_t m_dataOffset { 0 };
};

};


#endif /* !HAIKUFORMDATASTREAM_H */
