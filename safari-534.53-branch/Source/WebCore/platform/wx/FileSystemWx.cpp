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

#include "NotImplemented.h"
#include "PlatformString.h"
#include <wtf/text/CString.h>

#include <wx/wx.h>
#include <wx/datetime.h>
#include <wx/dir.h>
#include <wx/dynlib.h>
#include <wx/file.h>
#include <wx/filefn.h>
#include <wx/filename.h>

#if OS(DARWIN)
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace WebCore {

bool fileExists(const String& path)
{
    // NOTE: This is called for directory paths too so we need to check both.
    return wxFileName::FileExists(path) || wxFileName::DirExists(path);
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
    if (wxFileExists(path)) {
        t = wxFileName(path).GetModificationTime().GetTicks();
        return true;
    }
    return false;
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

String openTemporaryFile(const String& prefix, PlatformFileHandle& handle)
{
    wxString sFilename = wxFileName::CreateTempFileName(prefix);
    wxFile* temp = new wxFile();
    temp->Open(sFilename.c_str(), wxFile::read_write);
    handle = temp;
    return String(sFilename);
}

void closeFile(PlatformFileHandle& handle)
{
    if (handle && handle != invalidPlatformFileHandle)
        delete handle;
}

int writeToFile(PlatformFileHandle handle, const char* data, int length)
{
    if (handle)
        return static_cast<wxFile*>(handle)->Write(data, length);
    return -1;
}

bool unloadModule(PlatformModule mod)
{
#if OS(WINDOWS)
    return ::FreeLibrary(mod);
#elif OS(DARWIN)
    CFRelease(mod);
    return true;
#else
    wxASSERT(mod);
    delete mod;
    return true;
#endif
}


class wxDirTraverserNonRecursive : public wxDirTraverser {
public:
    wxDirTraverserNonRecursive(wxString basePath, wxArrayString& files) : m_basePath(basePath), m_files(files) { }

    virtual wxDirTraverseResult OnFile(const wxString& filename)
    {
        wxFileName afile(filename);
        afile.MakeRelativeTo(m_basePath);
        if (afile.GetFullPath().Find(afile.GetPathSeparator()) == wxNOT_FOUND)
            m_files.push_back(filename);

        return wxDIR_CONTINUE;
    }

    virtual wxDirTraverseResult OnDir(const wxString& dirname)
    {
        wxFileName dirfile(dirname);
        dirfile.MakeRelativeTo(m_basePath);
        if (dirfile.GetFullPath().Find(dirfile.GetPathSeparator()) == wxNOT_FOUND)
            m_files.push_back(dirname);

        return wxDIR_CONTINUE;
    }

private:
    wxString m_basePath;
    wxArrayString& m_files;

    DECLARE_NO_COPY_CLASS(wxDirTraverserNonRecursive)
};

Vector<String> listDirectory(const String& path, const String& filter)
{
    wxArrayString   file_paths;
    // wxDir::GetAllFiles recurses and for platforms like Mac where
    // a .plugin or .bundle can be a dir wx will recurse into the bundle
    // and list the files rather than just returning the plugin name, so
    // we write a special traverser that works around that issue.
    wxDirTraverserNonRecursive traverser(path, file_paths);
    
    wxDir dir(path);
    dir.Traverse(traverser, _T(""), wxDIR_FILES | wxDIR_DIRS);

    Vector<String> entries;
    
    for (int i = 0; i < file_paths.GetCount(); i++)
    {
        entries.append(file_paths[i]);
    }   
    
    return entries;
}

}
