/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "DownloadBundle.h"

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

namespace DownloadBundle {

static uint32_t magicNumber()
{
    return 0xDECAF4EA;
}

const String& fileExtension()
{
    static const NeverDestroyed<String> extension { ".download"_s };
    return extension;
}

bool appendResumeData(const uint8_t* resumeBytes, uint32_t resumeLength, const String& bundlePath)
{
    if (!resumeBytes || !resumeLength) {
        LOG_ERROR("Invalid resume data to write to bundle path");
        return false;
    }
    if (bundlePath.isEmpty()) {
        LOG_ERROR("Cannot write resume data to empty download bundle path");
        return false;
    }

    auto nullifiedPath = bundlePath.wideCharacters();
    FILE* bundlePtr = 0;
    if (_wfopen_s(&bundlePtr, nullifiedPath.data(), TEXT("ab")) || !bundlePtr) {
        LOG_ERROR("Failed to open file %s to append resume data", bundlePath.ascii().data());
        return false;
    }
    std::unique_ptr<FILE, decltype(&fclose)> bundle(bundlePtr, &fclose);

    if (fwrite(resumeBytes, 1, resumeLength, bundle.get()) != resumeLength) {
        LOG_ERROR("Failed to write resume data to the bundle - errno(%i)", errno);
        return false;
    }

    if (fwrite(&resumeLength, 4, 1, bundle.get()) != 1) {
        LOG_ERROR("Failed to write footer length to the bundle - errno(%i)", errno);
        return false;
    }

    const uint32_t magic = magicNumber();
    if (fwrite(&magic, 4, 1, bundle.get()) != 1) {
        LOG_ERROR("Failed to write footer magic number to the bundle - errno(%i)", errno);
        return false;
    }

    return true;
}

bool extractResumeData(const String& bundlePath, Vector<uint8_t>& resumeData)
{
    if (bundlePath.isEmpty()) {
        LOG_ERROR("Cannot create resume data from empty download bundle path");
        return false;
    }

    // Open a handle to the bundle file
    auto nullifiedPath = bundlePath.wideCharacters();
    FILE* bundlePtr = 0;
    if (_wfopen_s(&bundlePtr, nullifiedPath.data(), TEXT("r+b")) || !bundlePtr) {
        LOG_ERROR("Failed to open file %s to get resume data", bundlePath.ascii().data());
        return false;
    }
    std::unique_ptr<FILE, decltype(&fclose)> bundle(bundlePtr, &fclose);

    // Stat the file to get its size
    struct _stat64 fileStat;
    if (_fstat64(_fileno(bundle.get()), &fileStat))
        return false;

    // Check for the bundle magic number at the end of the file
    fpos_t footerMagicNumberPosition = fileStat.st_size - 4;
    ASSERT(footerMagicNumberPosition >= 0);
    if (footerMagicNumberPosition < 0)
        return false;
    if (fsetpos(bundle.get(), &footerMagicNumberPosition))
        return false;

    uint32_t footerMagicNumber = 0;
    if (fread(&footerMagicNumber, 4, 1, bundle.get()) != 1) {
        LOG_ERROR("Failed to read footer magic number from the bundle - errno(%i)", errno);
        return false;
    }

    if (footerMagicNumber != magicNumber()) {
        LOG_ERROR("Footer's magic number does not match 0x%X - errno(%i)", magicNumber(), errno);
        return false;
    }

    // Now we're *reasonably* sure this is a .download bundle we actually wrote.
    // Get the length of the resume data
    fpos_t footerLengthPosition = fileStat.st_size - 8;
    ASSERT(footerLengthPosition >= 0);
    if (footerLengthPosition < 0)
        return false;

    if (fsetpos(bundle.get(), &footerLengthPosition))
        return false;

    uint32_t footerLength = 0;
    if (fread(&footerLength, 4, 1, bundle.get()) != 1) {
        LOG_ERROR("Failed to read ResumeData length from the bundle - errno(%i)", errno);
        return false;
    }

    // Make sure theres enough bytes to read in for the resume data, and perform the read
    fpos_t footerStartPosition = fileStat.st_size - 8 - footerLength;
    ASSERT(footerStartPosition >= 0);
    if (footerStartPosition < 0)
        return false;
    if (fsetpos(bundle.get(), &footerStartPosition))
        return false;

    resumeData.resize(footerLength);
    if (fread(resumeData.data(), 1, footerLength, bundle.get()) != footerLength) {
        LOG_ERROR("Failed to read ResumeData from the bundle - errno(%i)", errno);
        return false;
    }

    // CFURLDownload will seek to the appropriate place in the file (before our footer) and start overwriting from there
    // However, say we were within a few hundred bytes of the end of a download when it was paused -
    // The additional footer extended the length of the file beyond its final length, and there will be junk data leftover
    // at the end.  Therefore, now that we've retrieved the footer data, we need to truncate it.
    if (errno_t resizeError = _chsize_s(_fileno(bundle.get()), footerStartPosition)) {
        LOG_ERROR("Failed to truncate the resume footer off the end of the file - errno(%i)", resizeError);
        return false;
    }

    return true;
}

} // namespace DownloadBundle

} // namespace WebCore
