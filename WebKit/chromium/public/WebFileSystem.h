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

#ifndef WebFileSystem_h
#define WebFileSystem_h

#include "WebCommon.h"
#include "WebString.h"

namespace WebKit {

class WebFileSystemCallbacks;
class WebFileWriter;
class WebFileWriterClient;

class WebFileSystem {
public:
    enum Type {
        TypeTemporary,
        TypePersistent,
    };

    // Moves a file or directory at |srcPath| to |destPath|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void move(const WebString& srcPath, const WebString& destPath, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Copies a file or directory at |srcPath| to |destPath|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void copy(const WebString& srcPath, const WebString& destPath, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Deletes a file or directory at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void remove(const WebString& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Retrieves the metadata information of the file or directory at the given |path|.
    // WebFileSystemCallbacks::didReadMetadata() must be called with a valid metadata when the retrieval is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void readMetadata(const WebString& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a file at given |path|.
    // If the |path| doesn't exist, it creates a new file at |path|.
    // If |exclusive| is true, it fails if the |path| already exists.
    // If |exclusive| is false, it succeeds if the |path| already exists or
    // it has successfully created a new file at |path|.
    //
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void createFile(const WebString& path, bool exclusive, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a directory at a given |path|.
    // If the |path| doesn't exist, it creates a new directory at |path|.
    // If |exclusive| is true, it fails if the |path| already exists.
    // If |exclusive| is false, it succeeds if the |path| already exists or it has successfully created a new directory at |path|.
    //
    // WebFileSystemCallbacks::didSucceed() must be called when
    // the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void createDirectory(const WebString& path, bool exclusive, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Checks if a file exists at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void fileExists(const WebString& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Checks if a directory exists at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void directoryExists(const WebString& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Reads directory entries of a given directory at |path|.
    // WebFileSystemCallbacks::didReadDirectory() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void readDirectory(const WebString& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a WebFileWriter that can be used to write to the given file.
    // This is a fast, synchronous call, and should not stat the filesystem.
    virtual WebFileWriter* createFileWriter(const WebString& path, WebFileWriterClient*) { WEBKIT_ASSERT_NOT_REACHED(); return 0; }

protected:
    virtual ~WebFileSystem() { }
};

} // namespace WebKit

#endif
