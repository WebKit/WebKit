/*
 * Copyright (C) 2014-2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "File.h"

#if ENABLE(FILE_REPLACEMENT)

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <wtf/FileSystem.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

bool File::shouldReplaceFile(const String& path)
{
    if (path.isEmpty())
        return false;

    NSError *error;
    NSURL *pathURL = [NSURL URLByResolvingAliasFileAtURL:[NSURL fileURLWithPath:path isDirectory:NO] options:NSURLBookmarkResolutionWithoutUI error:&error];
    if (!pathURL) {
        LOG_ERROR("Failed to resolve alias at path %s with error %@.\n", path.utf8().data(), error);
        return false;
    }

    UTType *uti;
    if (![pathURL getResourceValue:&uti forKey:NSURLContentTypeKey error:&error]) {
        LOG_ERROR("Failed to get type identifier of resource at URL %@ with error %@.\n", pathURL, error);
        return false;
    }

    return [uti conformsToType:UTTypePackage];
}

void File::computeNameAndContentTypeForReplacedFile(const String& path, const String& nameOverride, String& effectiveName, String& effectiveContentType)
{
    ASSERT(!FileSystem::pathFileName(path).endsWith('/')); // Expecting to get a path without trailing slash, even for directories.
    effectiveContentType = "application/zip"_s;
    effectiveName = makeString((nameOverride.isNull() ? FileSystem::pathFileName(path) : nameOverride), ".zip"_s);
}

}

#endif // ENABLE(FILE_REPLACEMENT)
