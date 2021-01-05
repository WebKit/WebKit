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

#include "config.h"
#include "HaikuFormDataStream.h"

#include "BlobRegistry.h"
#include "BlobRegistryImpl.h"
#include "Logging.h"
#include "SharedBuffer.h"

#include <String.h>

namespace WebCore {

BFormDataIO::BFormDataIO(const FormData* formData, PAL::SessionID sessionID)
	: m_sessionID(sessionID)
{
    ASSERT(isMainThread());

    if (!formData || formData->isEmpty())
        return;

    m_formData = formData->isolatedCopy();

    // Resolve the blob elements so the formData can correctly report it's size.
    m_formData = m_formData->resolveBlobReferences(blobRegistry().blobRegistryImpl());
}

BFormDataIO::~BFormDataIO()
{
}

ssize_t BFormDataIO::Size()
{
    computeContentLength();

    return m_totalSize;
}


ssize_t
BFormDataIO::Read(void* buffer, size_t size)
{
    if (!m_formData)
        return -1;

    const auto totalElementSize = m_formData->elements().size();
    if (m_elementPosition >= totalElementSize)
        return 0;

    size_t totalReadBytes = 0;

    while ((m_elementPosition < totalElementSize) && (totalReadBytes < size)) {
        const auto& element = m_formData->elements().at(m_elementPosition);

        size_t bufferSize = size - totalReadBytes;
        char* bufferPosition = (char*)buffer + totalReadBytes;

		WTF::Optional<size_t> readBytes = switchOn(element.data,
            [&] (const Vector<char>& bytes) {
                return readFromData(bytes, bufferPosition, bufferSize);
            }, [&] (const FormDataElement::EncodedFileData& fileData) {
                return readFromFile(fileData, bufferPosition, bufferSize);
            }, [&] (const FormDataElement::EncodedBlobData& blobData) {
				return readFromBlob(blobData, bufferPosition, bufferSize);
            }
        );

        if (!readBytes)
            return -1;

        totalReadBytes += *readBytes;
    }

    m_totalReadSize += totalReadBytes;

    return totalReadBytes;
}

ssize_t
BFormDataIO::Write(const void* /*buffer*/, size_t /*size*/)
{
    // Write isn't implemented since we don't use it
    return B_NOT_SUPPORTED;
}


void BFormDataIO::computeContentLength()
{
    if (!m_formData || m_isContentLengthUpdated)
        return;

    m_isContentLengthUpdated = true;

    for (const auto& element : m_formData->elements())
        m_totalSize += element.lengthInBytes();
}


WTF::Optional<size_t> BFormDataIO::readFromFile(const FormDataElement::EncodedFileData& fileData, char* buffer, size_t size)
{
    if (m_fileHandle == FileSystem::invalidPlatformFileHandle)
        m_fileHandle = FileSystem::openFile(fileData.filename, FileSystem::FileOpenMode::Read);

    if (!FileSystem::isHandleValid(m_fileHandle)) {
        LOG(Network, "Haiku - Failed while trying to open %s for upload\n", fileData.filename.utf8().data());
        m_fileHandle = FileSystem::invalidPlatformFileHandle;
        return WTF::nullopt;
    }

	// Note: there is no management of a file offset, we just keep the file
	// handle open and read from the current position.
    auto readBytes = FileSystem::readFromFile(m_fileHandle, buffer, size);
    if (readBytes < 0) {
        LOG(Network, "Haiku - Failed while trying to read %s for upload\n", fileData.filename.utf8().data());
        FileSystem::closeFile(m_fileHandle);
        m_fileHandle = FileSystem::invalidPlatformFileHandle;
        return WTF::nullopt;
    }

    if (!readBytes) {
        FileSystem::closeFile(m_fileHandle);
        m_fileHandle = FileSystem::invalidPlatformFileHandle;
        m_elementPosition++;
    }

    return readBytes;
}

WTF::Optional<size_t> BFormDataIO::readFromData(const Vector<char>& data, char* buffer, size_t size)
{
    size_t elementSize = data.size() - m_dataOffset;
    const char* elementBuffer = data.data() + m_dataOffset;

    size_t readBytes = elementSize > size ? size : elementSize;
    memcpy(buffer, elementBuffer, readBytes);

    if (elementSize > readBytes)
        m_dataOffset += readBytes;
    else {
        m_dataOffset = 0;
        m_elementPosition++;
    }

    return readBytes;
}


WTF::Optional<size_t> BFormDataIO::readFromBlob(const FormDataElement::EncodedBlobData& blob, char* buffer, size_t size)
{
    BlobData* blobData = blobRegistry().blobRegistryImpl()->getBlobDataFromURL(blob.url);

	if (!blobData)
		return WTF::nullopt;

	auto& blobItem = blobData->items().at(m_blobItemIndex);
	off_t readBytes;

    switch (blobItem.type()) {
		case BlobDataItem::Type::Data:
		{
			size_t elementSize = blobItem.data().data()->size() - m_dataOffset;
			const uint8_t* elementBuffer = blobItem.data().data()->data() + m_dataOffset;

			readBytes = elementSize > size ? size : elementSize;
			memcpy(buffer, elementBuffer, readBytes);

			if (elementSize > readBytes)
				m_dataOffset += readBytes;
			else {
				m_dataOffset = 0;
				m_blobItemIndex++;
			}
		}
		break;
    case BlobDataItem::Type::File: {
		// Open the file if not done yet
		if (m_fileHandle == FileSystem::invalidPlatformFileHandle)
		{
			WTF::Optional<WallTime> fileModificationTime = FileSystem::getFileModificationTime(blobItem.file()->path());
			if (fileModificationTime
					&& fileModificationTime == blobItem.file()->expectedModificationTime())
				m_fileHandle = FileSystem::openFile(blobItem.file()->path(), FileSystem::FileOpenMode::Read);

			// FIXME the blob can specify an offset and chunk size inside the file
			// So we should seek there and make sure we stop at the right time.
		}


		if (!FileSystem::isHandleValid(m_fileHandle)) {
			LOG(Network, "Haiku - Failed while trying to open %s for upload\n", blobItem.file()->path().utf8().data());
			m_fileHandle = FileSystem::invalidPlatformFileHandle;
			readBytes = -1;
		} else {
			// Note: there is no management of a file offset, we just keep the file
			// handle open and read from the current position.
			readBytes = FileSystem::readFromFile(m_fileHandle, buffer, size);
			if (readBytes < 0) {
				LOG(Network, "Haiku - Failed while trying to read %s for upload\n", blobItem.file()->path().utf8().data());
			}
		}

		if (readBytes <= 0) {
			FileSystem::closeFile(m_fileHandle);
			m_fileHandle = FileSystem::invalidPlatformFileHandle;
			m_blobItemIndex++;
		}
    }
		break;
    }

	// Should we advance to the next form element yet?
	if (m_blobItemIndex > blobData->items().size())
	{
		m_elementPosition++;
		m_blobItemIndex = 0;
	}

	if (readBytes < 0)
		return WTF::nullopt;
    return readBytes;
}

};
