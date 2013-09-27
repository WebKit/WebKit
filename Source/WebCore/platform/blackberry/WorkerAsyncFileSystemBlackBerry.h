/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA
 */

#ifndef WorkerAsyncFileSystemBlackBerry_h
#define WorkerAsyncFileSystemBlackBerry_h

#if ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)
#include "AsyncFileSystemBlackBerry.h"

#include "FileSystemType.h"

#include <wtf/PassOwnPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ScriptExecutionContext;
class WorkerPlatformAsyncFileSystemCallbacks;
class WorkerGlobalScope;

class WorkerAsyncFileSystemBlackBerry : public AsyncFileSystemBlackBerry {
public:
    static PassOwnPtr<WorkerAsyncFileSystemBlackBerry> create(WorkerGlobalScope* context, const String& mode, PassOwnPtr<BlackBerry::Platform::WebFileSystem> platformFileSystem)
    {
        return adoptPtr(new WorkerAsyncFileSystemBlackBerry(context, mode, platformFileSystem));
    }

    static void openFileSystem(WorkerGlobalScope*, const URL& rootURL, const String& mode, const String& basePath, const String& storageIdentifier, FileSystemType, long long size, bool create, PassOwnPtr<AsyncFileSystemCallbacks>);
    static void deleteFileSystem(WorkerGlobalScope*, const String& mode, const String& basePath, const String& storageIdentifier, FileSystemType, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual ~WorkerAsyncFileSystemBlackBerry();
    virtual bool waitForOperationToComplete();
    virtual void move(const URL& sourcePath, const URL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void copy(const URL& sourcePath, const URL& destinationPath, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void remove(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void removeRecursively(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void readMetadata(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createFile(const URL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createDirectory(const URL& path, bool exclusive, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void fileExists(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void directoryExists(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void readDirectory(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createWriter(AsyncFileWriterClient*, const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);
    virtual void createSnapshotFileAndReadMetadata(const URL& path, PassOwnPtr<AsyncFileSystemCallbacks>);

private:
    WorkerAsyncFileSystemBlackBerry(WorkerGlobalScope*, const String& mode, PassOwnPtr<BlackBerry::Platform::WebFileSystem> platformFileSystem);
    static void openFileSystemOnMainThread(ScriptExecutionContext*, const String& basePath, const String& storageIdentifier, FileSystemType, long long size, bool create, WorkerPlatformAsyncFileSystemCallbacks*);
    static void deleteFileSystemOnMainThread(ScriptExecutionContext*, const String& basePath, const String& storageIdentifier, FileSystemType, WorkerPlatformAsyncFileSystemCallbacks*);
    static void moveOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& sourcePath, const URL& destinationPath, WorkerPlatformAsyncFileSystemCallbacks*);
    static void copyOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& sourcePath, const URL& destinationPath, WorkerPlatformAsyncFileSystemCallbacks*);
    static void removeOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void removeRecursivelyOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void readMetadataOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void createFileOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, bool exclusive, WorkerPlatformAsyncFileSystemCallbacks*);
    static void createDirectoryOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, bool exclusive, WorkerPlatformAsyncFileSystemCallbacks*);
    static void fileExistsOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void directoryExistsOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void readDirectoryOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void createWriterOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, AsyncFileWriterClient*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);
    static void createSnapshotFileAndReadMetadataOnMainThread(ScriptExecutionContext*, BlackBerry::Platform::WebFileSystem*, const URL& path, WorkerPlatformAsyncFileSystemCallbacks*);

    WorkerGlobalScope* m_context;
    String m_mode;
};

} // namespace WebCore

#endif // ENABLE(FILE_SYSTEM) && ENABLE(WORKERS)

#endif // WorkerAsyncFileSystemBlackBerry_h
