/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "SharedBuffer.h"

#include "FileSystem.h"
#include "ShellBrew.h"

#include <AEEAppGen.h>
#include <AEEFile.h>
#include <AEEStdLib.h>

#include <wtf/OwnPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    if (filePath.isEmpty())
        return 0;

    long long fileSize;
    if (!fileExists(filePath) || !getFileSize(filePath, fileSize))
        return 0;

    RefPtr<SharedBuffer> result = create();
    result->m_buffer.grow(fileSize);

    OwnPtr<IFileMgr> fileMgr = createInstance<IFileMgr>(AEECLSID_FILEMGR);

    CString filename = fileSystemRepresentation(filePath);
    OwnPtr<IFile> file(IFILEMGR_OpenFile(fileMgr.get(), filename.data(), _OFM_READ));

    if (!file) {
        LOG_ERROR("Failed to open file %s to create shared buffer, errno(%i)", filePath.ascii().data(), IFILEMGR_GetLastError(fileMgr.get()));
        return 0;
    }

    size_t totalBytesRead = 0;
    int32 bytesRead;
    while ((bytesRead = IFILE_Read(file.get(), result->m_buffer.data() + totalBytesRead, fileSize - totalBytesRead)) > 0)
        totalBytesRead += bytesRead;
    result->m_size = totalBytesRead;

    if (totalBytesRead != fileSize) {
        LOG_ERROR("Failed to fully read contents of file %s - errno(%i)", filePath.ascii().data(), IFILEMGR_GetLastError(fileMgr.get()));
        return 0;
    }

    return result.release();
}

} // namespace WebCore
