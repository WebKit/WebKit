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
#include "WebURL.h"

namespace WebKit {

// FIXME: Move these classes into platform.
class WebFileSystemCallbacks;
class WebFileWriter;
class WebFileWriterClient;

class WebFileSystem {
public:
    enum Type {
        TypeTemporary,
        TypePersistent,

        // Indicates an isolated filesystem which only exposes a set of files.
        TypeIsolated,

        // Indicates a non-sandboxed filesystem.
        TypeExternal,
    };

    // Moves a file or directory at |srcPath| to |destPath|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void move(const WebURL& srcPath, const WebURL& destPath, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Copies a file or directory at |srcPath| to |destPath|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void copy(const WebURL& srcPath, const WebURL& destPath, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Deletes a file or directory at a given |path|.
    // It is an error to try to remove a directory that is not empty.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void remove(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Deletes a file or directory recursively at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void removeRecursively(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Retrieves the metadata information of the file or directory at the given |path|.
    // This may not always return the local platform path in remote filesystem cases.
    // WebFileSystemCallbacks::didReadMetadata() must be called with a valid metadata when the retrieval is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void readMetadata(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a file at given |path|.
    // If the |path| doesn't exist, it creates a new file at |path|.
    // If |exclusive| is true, it fails if the |path| already exists.
    // If |exclusive| is false, it succeeds if the |path| already exists or
    // it has successfully created a new file at |path|.
    //
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void createFile(const WebURL& path, bool exclusive, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a directory at a given |path|.
    // If the |path| doesn't exist, it creates a new directory at |path|.
    // If |exclusive| is true, it fails if the |path| already exists.
    // If |exclusive| is false, it succeeds if the |path| already exists or it has successfully created a new directory at |path|.
    //
    // WebFileSystemCallbacks::didSucceed() must be called when
    // the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void createDirectory(const WebURL& path, bool exclusive, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Checks if a file exists at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void fileExists(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Checks if a directory exists at a given |path|.
    // WebFileSystemCallbacks::didSucceed() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void directoryExists(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Reads directory entries of a given directory at |path|.
    // WebFileSystemCallbacks::didReadDirectory() must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void readDirectory(const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

    // Creates a WebFileWriter that can be used to write to the given file.
    // This is a fast, synchronous call, and should not stat the filesystem.
    virtual WebFileWriter* createFileWriter(const WebURL& path, WebFileWriterClient*) { WEBKIT_ASSERT_NOT_REACHED(); return 0; }

    // Creates a snapshot file for a given file specified by |path| and registers the file with the |blobURL|. It returns the metadata of the created snapshot file.
    // The returned metadata should include a local platform path to the snapshot image.
    // In local filesystem cases the backend may simply return the metadata of the file itself (as well as readMetadata does), while in remote filesystem case the backend may download the file into a temporary snapshot file and return the metadata of the temporary file.
    // The returned metadata is used to create a File object for the |path|.
    // The snapshot file is supposed to be deleted when the last reference to the |blobURL| is dropped.
    // WebFileSystemCallbacks::didReadMetadata() with the metadata of the snapshot file must be called when the operation is completed successfully.
    // WebFileSystemCallbacks::didFail() must be called otherwise.
    virtual void createSnapshotFileAndReadMetadata(const WebURL& blobURL, const WebURL& path, WebFileSystemCallbacks*) { WEBKIT_ASSERT_NOT_REACHED(); }

protected:
    virtual ~WebFileSystem() { }
};

} // namespace WebKit

#endif
