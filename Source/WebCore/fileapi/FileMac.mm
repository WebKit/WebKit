/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "File.h"

#if ENABLE(FILE_REPLACEMENT)

#include "FileSystem.h"

namespace WebCore {

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
bool File::shouldReplaceFile(const String& path)
{
    if (path.isEmpty())
        return false;

    FSRef pathRef;
    Boolean targetIsFolder;
    Boolean wasAliased;
    NSString *aliasedPath = path;

    // Determine if the file is an alias, and if so, get the target path.
    if (FSPathMakeRef((UInt8 *)[path fileSystemRepresentation], &pathRef, NULL) == noErr) {
        if (FSResolveAliasFileWithMountFlags(&pathRef, TRUE, &targetIsFolder, &wasAliased, kResolveAliasFileNoUI) == noErr && wasAliased) {
            char pathFromPathRef[PATH_MAX + 1]; // +1 is for \0 
            if (FSRefMakePath(&pathRef, (unsigned char *)pathFromPathRef, PATH_MAX) == noErr)
                aliasedPath = [[NSFileManager defaultManager] stringWithFileSystemRepresentation:pathFromPathRef length:strlen(pathFromPathRef)];
        }
    }
    
    if (!aliasedPath)
        return false;

    return [[NSWorkspace sharedWorkspace] isFilePackageAtPath:aliasedPath];
}
#pragma clang diagnostic pop

void File::computeNameAndContentTypeForReplacedFile(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType)
{
    ASSERT(!pathGetFileName(path).endsWith('/')); // Expecting to get a path without trailing slash, even for directories.
    effectiveContentType = ASCIILiteral("application/zip");
    effectiveName = (nameOverride.isNull() ? pathGetFileName(path) : nameOverride) + ASCIILiteral(".zip");
}

}

#endif
