/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "PathsBlockedForSandboxExtensions.h"

#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Vector.h>

namespace WebKit {

static Vector<String>& pathsBlockedForSandboxExtension()
{
    static NeverDestroyed<Vector<String>> pathsBlocked;
    return pathsBlocked;
}

static Vector<String>& subPathsBlockedForSandboxExtension()
{
    static NeverDestroyed<Vector<String>> subPathsBlocked;
    return subPathsBlocked;
}

void addPathsBlockedForSandboxExtension(Vector<String>&& paths)
{
    for (auto& path : paths) {
        String normalizedPath = FileSystem::realPath(path);
        path = FileSystem::lexicallyNormal(normalizedPath);
    }
    pathsBlockedForSandboxExtension().appendVector(WTF::move(paths));
}

void addSubPathsBlockedForSandboxExtension(Vector<String>&& paths)
{
    for (auto& path : paths) {
        String normalizedPath = FileSystem::realPath(path);
        path = FileSystem::lexicallyNormal(normalizedPath);
    }
    subPathsBlockedForSandboxExtension().appendVector(WTF::move(paths));
}

bool pathIsBlockedForSandboxExtensions(const String& path)
{
    String normalizedPath = FileSystem::realPath(path);
    normalizedPath = FileSystem::lexicallyNormal(normalizedPath);

    for (auto& pathBlocked : pathsBlockedForSandboxExtension()) {
        if (pathBlocked == normalizedPath)
            return true;
    }

    for (auto& pathBlocked : subPathsBlockedForSandboxExtension()) {
        if (FileSystem::isAncestor(pathBlocked, normalizedPath))
            return true;
    }

    return false;
}

}
