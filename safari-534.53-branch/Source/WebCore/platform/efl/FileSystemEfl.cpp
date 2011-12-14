/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Collabora, Ltd. All rights reserved.
 * Copyright (C) 2008 Kenneth Rohde Christiansen.
 * Copyright (C) 2009-2010 ProFUSION embedded systems
 * Copyright (C) 2009-2010 Samsung Electronics
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "FileSystem.h"

#include "NotImplemented.h"

#include <Ecore.h>
#include <Ecore_File.h>
#include <Eina.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fnmatch.h>
#if ENABLE(GLIB_SUPPORT)
#include <glib.h> // TODO: remove me after following TODO is solved.
#endif
#include <limits.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wtf/text/CString.h>

namespace WebCore {

CString fileSystemRepresentation(const String& path)
{
// WARNING: this is just used by platform/network/soup, thus must be GLIB!!!
// TODO: move this to CString and use it instead in both, being more standard
#if !PLATFORM(WIN_OS) && defined(WTF_USE_SOUP)
    char* filename = g_uri_unescape_string(path.utf8().data(), 0);
    CString cfilename(filename);
    g_free(filename);
    return cfilename;
#else
    return path.utf8();
#endif
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    char buffer[PATH_MAX];
    const char* tmpDir = getenv("TMPDIR");

    if (!tmpDir)
        tmpDir = "/tmp";

    if (snprintf(buffer, PATH_MAX, "%s/%sXXXXXX", tmpDir, prefix.utf8().data()) >= PATH_MAX)
        goto end;

    handle = mkstemp(buffer);
    if (handle < 0)
        goto end;

    return String::fromUTF8(buffer);

end:
    handle = invalidPlatformFileHandle;
    return String();
}

bool unloadModule(PlatformModule module)
{
    // caution, closing handle will make memory vanish and any remaining
    // timer, idler, threads or any other left-over will crash,
    // maybe just ignore this is a safer solution?
    return !dlclose(module);
}

String homeDirectoryPath()
{
    const char *home = getenv("HOME");
    if (!home) {
        home = getenv("TMPDIR");
        if (!home)
            home = "/tmp";
    }
    return String::fromUTF8(home);
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    Vector<String> entries;
    CString cpath = path.utf8();
    CString cfilter = filter.utf8();
    char filePath[PATH_MAX];
    char* fileName;
    size_t fileNameSpace;
    DIR* dir;

    if (cpath.length() + NAME_MAX >= sizeof(filePath))
        return entries;
    // loop invariant: directory part + '/'
    memcpy(filePath, cpath.data(), cpath.length());
    fileName = filePath + cpath.length();
    if (cpath.length() > 0 && filePath[cpath.length() - 1] != '/') {
        fileName[0] = '/';
        fileName++;
    }
    fileNameSpace = sizeof(filePath) - (fileName - filePath) - 1;

    dir = opendir(cpath.data());
    if (!dir)
        return entries;

    struct dirent* de;
    while (de = readdir(dir)) {
        size_t nameLen;
        if (de->d_name[0] == '.') {
            if (de->d_name[1] == '\0')
                continue;
            if (de->d_name[1] == '.' && de->d_name[2] == '\0')
                continue;
        }
        if (fnmatch(cfilter.data(), de->d_name, 0))
            continue;

        nameLen = strlen(de->d_name);
        if (nameLen >= fileNameSpace)
            continue; // maybe assert? it should never happen anyway...

        memcpy(fileName, de->d_name, nameLen + 1);
        entries.append(filePath);
    }
    closedir(dir);
    return entries;
}

}
