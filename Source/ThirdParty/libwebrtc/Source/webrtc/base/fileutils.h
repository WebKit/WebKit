/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_BASE_FILEUTILS_H_
#define WEBRTC_BASE_FILEUTILS_H_

#include <string>

#if !defined(WEBRTC_WIN)
#include <dirent.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "webrtc/base/checks.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/base/platform_file.h"

namespace rtc {

class FileStream;
class Pathname;

//////////////////////////
// Directory Iterator   //
//////////////////////////

// A DirectoryIterator is created with a given directory. It originally points
// to the first file in the directory, and can be advanecd with Next(). This
// allows you to get information about each file.

class DirectoryIterator {
  friend class Filesystem;
 public:
  // Constructor
  DirectoryIterator();
  // Destructor
  virtual ~DirectoryIterator();

  // Starts traversing a directory
  // dir is the directory to traverse
  // returns true if the directory exists and is valid
  // The iterator will point to the first entry in the directory
  virtual bool Iterate(const Pathname &path);

  // Advances to the next file
  // returns true if there were more files in the directory.
  virtual bool Next();

  // returns true if the file currently pointed to is a directory
  virtual bool IsDirectory() const;

  // returns the name of the file currently pointed to
  virtual std::string Name() const;

 private:
  std::string directory_;
#if defined(WEBRTC_WIN)
  WIN32_FIND_DATA data_;
  HANDLE handle_;
#else
  DIR *dir_;
  struct dirent *dirent_;
  struct stat stat_;
#endif
};

enum FileTimeType { FTT_CREATED, FTT_MODIFIED, FTT_ACCESSED };

class FilesystemInterface {
 public:
  virtual ~FilesystemInterface() {}

  // Returns a DirectoryIterator for a given pathname.
  // TODO: Do fancy abstracted stuff
  virtual DirectoryIterator* IterateDirectory();

  // Opens a file. Returns an open StreamInterface if function succeeds.
  // Otherwise, returns null.
  // TODO: Add an error param to indicate failure reason, similar to
  // FileStream::Open
  virtual FileStream *OpenFile(const Pathname &filename,
                               const std::string &mode) = 0;

  // This will attempt to delete the path located at filename.
  // It DCHECKs and returns false if the path points to a folder or a
  // non-existent file.
  virtual bool DeleteFile(const Pathname &filename) = 0;

  // This will attempt to delete the empty folder located at 'folder'
  // It DCHECKs and returns false if the path points to a file or a non-existent
  // folder. It fails normally if the folder is not empty or can otherwise
  // not be deleted.
  virtual bool DeleteEmptyFolder(const Pathname &folder) = 0;

  // This will call IterateDirectory, to get a directory iterator, and then
  // call DeleteFolderAndContents and DeleteFile on every path contained in this
  // folder. If the folder is empty, this returns true.
  virtual bool DeleteFolderContents(const Pathname &folder);

  // This deletes the contents of a folder, recursively, and then deletes
  // the folder itself.
  virtual bool DeleteFolderAndContents(const Pathname& folder);

  // Creates a directory. This will call itself recursively to create /foo/bar
  // even if /foo does not exist. Returns true if the function succeeds.
  virtual bool CreateFolder(const Pathname &pathname) = 0;

  // This moves a file from old_path to new_path, where "old_path" is a
  // plain file. This DCHECKs and returns false if old_path points to a
  // directory, and returns true if the function succeeds.
  // If the new path is on a different volume than the old path, this function
  // will attempt to copy and, if that succeeds, delete the old path.
  virtual bool MoveFile(const Pathname &old_path, const Pathname &new_path) = 0;

  // This copies a file from old_path to new_path. This method DCHECKs and
  // returns false if old_path is a folder, and returns true if the copy
  // succeeds.
  virtual bool CopyFile(const Pathname &old_path, const Pathname &new_path) = 0;

  // Returns true if pathname refers to a directory
  virtual bool IsFolder(const Pathname& pathname) = 0;

  // Returns true if pathname refers to a file
  virtual bool IsFile(const Pathname& pathname) = 0;

  // Returns true if pathname refers to no filesystem object, every parent
  // directory either exists, or is also absent.
  virtual bool IsAbsent(const Pathname& pathname) = 0;

  // Returns true if pathname represents a temporary location on the system.
  virtual bool IsTemporaryPath(const Pathname& pathname) = 0;

  // A folder appropriate for storing temporary files (Contents are
  // automatically deleted when the program exits)
  virtual bool GetTemporaryFolder(Pathname &path, bool create,
                                  const std::string *append) = 0;

  virtual std::string TempFilename(const Pathname &dir,
                                   const std::string &prefix) = 0;

  // Determines the size of the file indicated by path.
  virtual bool GetFileSize(const Pathname& path, size_t* size) = 0;

  // Determines a timestamp associated with the file indicated by path.
  virtual bool GetFileTime(const Pathname& path, FileTimeType which,
                           time_t* time) = 0;

  // Get a temporary folder that is unique to the current user and application.
  // TODO: Re-evaluate the goals of this function.  We probably just need any
  // directory that won't collide with another existing directory, and which
  // will be cleaned up when the program exits.
  virtual bool GetAppTempFolder(Pathname* path) = 0;

  // Note: These might go into some shared config section later, but they're
  // used by some methods in this interface, so we're leaving them here for now.
  void SetOrganizationName(const std::string& organization) {
    organization_name_ = organization;
  }
  void GetOrganizationName(std::string* organization) {
    RTC_DCHECK(organization);
    *organization = organization_name_;
  }
  void SetApplicationName(const std::string& application) {
    application_name_ = application;
  }
  void GetApplicationName(std::string* application) {
    RTC_DCHECK(application);
    *application = application_name_;
  }

 protected:
  std::string organization_name_;
  std::string application_name_;
};

class Filesystem {
 public:
  static FilesystemInterface *default_filesystem() {
    RTC_DCHECK(default_filesystem_);
    return default_filesystem_;
  }

  static void set_default_filesystem(FilesystemInterface *filesystem) {
    default_filesystem_ = filesystem;
  }

  static FilesystemInterface *swap_default_filesystem(
      FilesystemInterface *filesystem) {
    FilesystemInterface *cur = default_filesystem_;
    default_filesystem_ = filesystem;
    return cur;
  }

  static DirectoryIterator *IterateDirectory() {
    return EnsureDefaultFilesystem()->IterateDirectory();
  }

  static bool CreateFolder(const Pathname &pathname) {
    return EnsureDefaultFilesystem()->CreateFolder(pathname);
  }

  static FileStream *OpenFile(const Pathname &filename,
                              const std::string &mode) {
    return EnsureDefaultFilesystem()->OpenFile(filename, mode);
  }

  static bool DeleteFile(const Pathname &filename) {
    return EnsureDefaultFilesystem()->DeleteFile(filename);
  }

  static bool DeleteFolderContents(const Pathname &folder) {
    return EnsureDefaultFilesystem()->DeleteFolderContents(folder);
  }

  static bool DeleteFolderAndContents(const Pathname &folder) {
    return EnsureDefaultFilesystem()->DeleteFolderAndContents(folder);
  }

  static bool MoveFile(const Pathname &old_path, const Pathname &new_path) {
    return EnsureDefaultFilesystem()->MoveFile(old_path, new_path);
  }

  static bool CopyFile(const Pathname &old_path, const Pathname &new_path) {
    return EnsureDefaultFilesystem()->CopyFile(old_path, new_path);
  }

  static bool IsFolder(const Pathname& pathname) {
    return EnsureDefaultFilesystem()->IsFolder(pathname);
  }

  static bool IsFile(const Pathname &pathname) {
    return EnsureDefaultFilesystem()->IsFile(pathname);
  }

  static bool IsAbsent(const Pathname &pathname) {
    return EnsureDefaultFilesystem()->IsAbsent(pathname);
  }

  static bool IsTemporaryPath(const Pathname& pathname) {
    return EnsureDefaultFilesystem()->IsTemporaryPath(pathname);
  }

  static bool GetTemporaryFolder(Pathname &path, bool create,
                                 const std::string *append) {
    return EnsureDefaultFilesystem()->GetTemporaryFolder(path, create, append);
  }

  static std::string TempFilename(const Pathname &dir,
                                  const std::string &prefix) {
    return EnsureDefaultFilesystem()->TempFilename(dir, prefix);
  }

  static bool GetFileSize(const Pathname& path, size_t* size) {
    return EnsureDefaultFilesystem()->GetFileSize(path, size);
  }

  static bool GetFileTime(const Pathname& path, FileTimeType which,
                          time_t* time) {
    return EnsureDefaultFilesystem()->GetFileTime(path, which, time);
  }

  static bool GetAppTempFolder(Pathname* path) {
    return EnsureDefaultFilesystem()->GetAppTempFolder(path);
  }

  static void SetOrganizationName(const std::string& organization) {
    EnsureDefaultFilesystem()->SetOrganizationName(organization);
  }

  static void GetOrganizationName(std::string* organization) {
    EnsureDefaultFilesystem()->GetOrganizationName(organization);
  }

  static void SetApplicationName(const std::string& application) {
    EnsureDefaultFilesystem()->SetApplicationName(application);
  }

  static void GetApplicationName(std::string* application) {
    EnsureDefaultFilesystem()->GetApplicationName(application);
  }

 private:
  static FilesystemInterface* default_filesystem_;

  static FilesystemInterface *EnsureDefaultFilesystem();
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(Filesystem);
};

}  // namespace rtc

#endif  // WEBRTC_BASE_FILEUTILS_H_
