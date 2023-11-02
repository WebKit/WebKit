/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Archive.h"

#include <wtf/RunLoop.h>
#include <wtf/Scope.h>

namespace WebCore {

Archive::~Archive() = default;

void Archive::clearAllSubframeArchives()
{
    HashSet<Archive*> clearedArchives;
    clearedArchives.add(this);
    clearAllSubframeArchives(clearedArchives);
}

void Archive::clearAllSubframeArchives(HashSet<Archive*>& clearedArchives)
{
    ASSERT(clearedArchives.contains(this));
    for (auto& archive : m_subframeArchives) {
        if (clearedArchives.add(archive.ptr()))
            archive->clearAllSubframeArchives(clearedArchives);
    }
    m_subframeArchives.clear();
}

Expected<Vector<String>, ArchiveError> Archive::saveResourcesToDisk(const String& directory)
{
    ASSERT(!RunLoop::isMain());

    Vector<String> filePaths;
    if (!m_mainResource)
        return makeUnexpected(ArchiveError::EmptyResource);

    bool hasError = false;
    auto cleanup = makeScopeExit([&] {
        if (hasError) {
            for (auto filePath : filePaths)
                FileSystem::deleteFile(filePath);
        }
    });

    auto mainResourceResult = m_mainResource->saveToDisk(directory);
    if (!mainResourceResult) {
        hasError = true;
        return makeUnexpected(mainResourceResult.error());
    }
    filePaths.append(mainResourceResult.value());

    for (auto subresource : m_subresources) {
        auto result = subresource->saveToDisk(directory);
        if (!result) {
            hasError = true;
            return makeUnexpected(result.error());
        }
        filePaths.append(result.value());
    }

    for (auto subframeArchive : m_subframeArchives) {
        auto result = subframeArchive->saveResourcesToDisk(directory);
        if (!result) {
            hasError = true;
            return makeUnexpected(result.error());
        }
        filePaths.appendVector(result.value());
    }

    return filePaths;
}

}
