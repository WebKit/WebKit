/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "WebDatabaseProvider.h"

#include <shlobj.h>
#include <shlwapi.h>

#if ENABLE(INDEXED_DATABASE)
String WebDatabaseProvider::indexedDatabaseDirectoryPath()
{
    char executablePath[MAX_PATH];
    char appDataDirectory[MAX_PATH];
    char databaseDirectory[MAX_PATH];

    if (FAILED(::SHGetFolderPathA(0, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, 0, 0, appDataDirectory))
        || FAILED(::GetModuleFileNameA(0, executablePath, MAX_PATH)))
        return emptyString();

    ::PathRemoveExtensionA(executablePath);
    LPSTR executableName = ::PathFindFileNameA(executablePath);
    sprintf_s(databaseDirectory, MAX_PATH, "%s\\%s\\WebKit\\Databases\\___IndexedDB", appDataDirectory, executableName);

    if (::SHCreateDirectoryExA(0, databaseDirectory, 0) != ERROR_SUCCESS
        && ::GetLastError() != ERROR_FILE_EXISTS
        && ::GetLastError() != ERROR_ALREADY_EXISTS)
        return emptyString();

    return String(databaseDirectory);
}
#endif
