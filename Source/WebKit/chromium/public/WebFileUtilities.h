/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFileUtilities_h
#define WebFileUtilities_h

#include "platform/WebCommon.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"

#ifdef WIN32
typedef void *HANDLE;
#endif

namespace WebKit {

class WebFileUtilities {
public:
#ifdef WIN32
    typedef HANDLE FileHandle;
#else
    typedef int FileHandle;
#endif
    virtual void revealFolderInOS(const WebString& path) { }
    virtual bool fileExists(const WebString& path) { return false; }
    virtual bool deleteFile(const WebString& path) { return false; }
    virtual bool deleteEmptyDirectory(const WebString& path) { return false; }
    virtual bool getFileSize(const WebString& path, long long& result) { return false; }
    virtual bool getFileModificationTime(const WebString& path, double& result) { return false; }
    virtual WebString directoryName(const WebString& path)  { return WebString(); }
    virtual WebString pathByAppendingComponent(const WebString& path, const WebString& component)  { return WebString(); }
    virtual bool makeAllDirectories(const WebString& path) { return false; }
    virtual WebString getAbsolutePath(const WebString& path)  { return WebString(); }
    virtual bool isDirectory(const WebString& path) { return false; }
    virtual WebURL filePathToURL(const WebString& path)  { return WebURL(); }
    virtual FileHandle openFile(const WebString& path, int mode)  { return FileHandle(); }
    // Should set the FileHandle to a invalid value if the file is closed successfully.
    virtual void closeFile(FileHandle&) { }
    virtual long long seekFile(FileHandle, long long offset, int origin) { return 0; }
    virtual bool truncateFile(FileHandle, long long offset) { return false; }
    virtual int readFromFile(FileHandle, char* data, int length) { return 0; }
    virtual int writeToFile(FileHandle, const char* data, int length) { return 0; }

protected:
    ~WebFileUtilities() {}
};

} // namespace WebKit

#endif
