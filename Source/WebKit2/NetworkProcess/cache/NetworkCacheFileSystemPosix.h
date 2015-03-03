/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef NetworkCacheFileSystemPosix_h
#define NetworkCacheFileSystemPosix_h

#if ENABLE(NETWORK_CACHE)

#include "NetworkCacheKey.h"
#include <WebCore/FileSystem.h>
#include <dirent.h>
#include <wtf/text/CString.h>

namespace WebKit {

template <typename Function>
static void traverseDirectory(const String& path, uint8_t type, const Function& function)
{
    DIR* dir = opendir(WebCore::fileSystemRepresentation(path).data());
    if (!dir)
        return;
    struct dirent* dp;
    while ((dp = readdir(dir))) {
        if (dp->d_type != type)
            continue;
        const char* name = dp->d_name;
        if (!strcmp(name, ".") || !strcmp(name, ".."))
            continue;
        function(String(name));
    }
    closedir(dir);
}

template <typename Function>
inline void traverseCacheFiles(const String& cachePath, const Function& function)
{
    traverseDirectory(cachePath, DT_DIR, [&cachePath, &function](const String& subdirName) {
        String partitionPath = WebCore::pathByAppendingComponent(cachePath, subdirName);
        traverseDirectory(partitionPath, DT_REG, [&function, &partitionPath](const String& fileName) {
            if (fileName.length() != NetworkCacheKey::hashStringLength())
                return;
            function(fileName, partitionPath);
        });
    });
}

} // namespace WebKit

#endif // ENABLE(NETWORK_CACHE)

#endif // NetworkCacheFileSystemPosix_h

