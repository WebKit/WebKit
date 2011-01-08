/*
 * Copyright (C) 2008 Holger Hans Peter Freyther
 *               2008 Kenneth Rohde Christiansen
 *               2009-2010 ProFUSION embedded systems
 *               2009-2010 Samsung Electronics
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

#include <wtf/text/CString.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace WebCore {

PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String& filePath)
{
    FILE* file;
    struct stat fileStat;
    RefPtr<SharedBuffer> result;

    if (filePath.isEmpty())
        return 0;

    if (!(file = fopen(filePath.utf8().data(), "rb")))
        return 0;

    if (fstat(fileno(file), &fileStat)) {
        fclose(file);
        return 0;
    }

    result = SharedBuffer::create();
    result->m_buffer.resize(fileStat.st_size);
    if (result->m_buffer.size() != static_cast<unsigned>(fileStat.st_size)) {
        fclose(file);
        return 0;
    }

    fread(result->m_buffer.data(), 1, fileStat.st_size, file);
    fclose(file);

    return result.release();
}

} // namespace WebCore
