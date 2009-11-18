/*
 * Copyright (C) 2007 Kevin Ollivier
 * Copyright (C) 2008 Collabora, Ltd.
 * Copyright (C) 2009 Peter Laufenberg @ Inhance Digital Corp
 *
 * All rights reserved.
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
#include "FileSystem.h"

#include "CString.h"
#include "NotImplemented.h"
#include "PlatformString.h"

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/dir.h>
#include <wx/file.h>
#include <wx/datetime.h>
#include <wx/filefn.h>

namespace WebCore {

bool fileExists(const String& path)
{
    return wxFileName::FileExists(path);
}

bool deleteFile(const String& path)
{
    return wxRemoveFile(path);
}

bool deleteEmptyDirectory(const String& path)
{
    return wxFileName::Rmdir(path);
}

bool getFileSize(const String& path, long long& resultSize)
{
    wxULongLong size = wxFileName::GetSize(path);
    if (wxInvalidSize != size) {
        // TODO: why is FileSystem::getFileSize signed?
        resultSize = (long long)size.GetValue();
        return true;
    }

    return false;
}

bool getFileModificationTime(const String& path, time_t& t)
{
    t = wxFileName(path).GetModificationTime().GetTicks();
    return true;
}

bool makeAllDirectories(const String& path)
{
    return wxFileName::Mkdir(path, 0777, wxPATH_MKDIR_FULL);
}

String pathByAppendingComponent(const String& path, const String& component)
{
    return wxFileName(path, component).GetFullPath();
}

String homeDirectoryPath()
{
    return wxFileName::GetHomeDir();
}

String pathGetFileName(const String& path)
{
    return wxFileName(path).GetFullName();
}

String directoryName(const String& path)
{
    return wxFileName(path).GetPath();
}

CString openTemporaryFile(const char* prefix, PlatformFileHandle& handle)
{
    notImplemented();
    handle = invalidPlatformFileHandle;
    return CString();
}

void closeFile(PlatformFileHandle&)
{
    notImplemented();
}

int writeToFile(PlatformFileHandle, const char* data, int length)
{
    notImplemented();
    return 0;
}

bool unloadModule(PlatformModule mod)
{
#if PLATFORM(WIN_OS)
    return ::FreeLibrary(mod);
#else
    notImplemented();
    return false;
#endif
}

Vector<String> listDirectory(const String& path, const String& filter)
{
    wxArrayString   file_paths;
    
    int n_files = wxDir::GetAllFiles(path, &file_paths, _T(""), wxDIR_FILES);

    Vector<String> entries;
    
    for (int i = 0; i < n_files; i++)
    {
        entries.append(file_paths[i]);
    }   
    
    return entries;
}

}
