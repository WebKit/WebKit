/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#ifndef WEBRTC_TEST_TESTSUPPORT_FILEUTILS_H_
#define WEBRTC_TEST_TESTSUPPORT_FILEUTILS_H_

#include <string>
#include <vector>

#include "webrtc/base/optional.h"

namespace webrtc {
namespace test {

// This is the "directory" returned if the ProjectPath() function fails
// to find the project root.
extern const char* kCannotFindProjectRootDir;

// Creates and returns the absolute path to the output directory where log files
// and other test artifacts should be put. The output directory is generally a
// directory named "out" at the top-level of the project, i.e. a subfolder to
// the path returned by ProjectRootPath(). The exception is Android where we use
// /sdcard/ instead.
//
// If symbolic links occur in the path they will be resolved and the actual
// directory will be returned.
//
// Returns the path WITH a trailing path delimiter. If the project root is not
// found, the current working directory ("./") is returned as a fallback.
std::string OutputPath();

// Generates an empty file with a unique name in the specified directory and
// returns the file name and path.
std::string TempFilename(const std::string &dir, const std::string &prefix);

// Returns a path to a resource file for the currently executing platform.
// Adapts to what filenames are currently present in the
// [project-root]/resources/ dir.
// Returns an absolute path according to this priority list (the directory
// part of the path is left out for readability):
// 1. [name]_[platform]_[architecture].[extension]
// 2. [name]_[platform].[extension]
// 3. [name]_[architecture].[extension]
// 4. [name].[extension]
// Where
// * platform is either of "win", "mac" or "linux".
// * architecture is either of "32" or "64".
//
// Arguments:
//    name - Name of the resource file. If a plain filename (no directory path)
//           is supplied, the file is assumed to be located in resources/
//           If a directory path is prepended to the filename, a subdirectory
//           hierarchy reflecting that path is assumed to be present.
//    extension - File extension, without the dot, i.e. "bmp" or "yuv".
std::string ResourcePath(const std::string& name,
                         const std::string& extension);

// Gets the current working directory for the executing program.
// Returns "./" if for some reason it is not possible to find the working
// directory.
std::string WorkingDir();

// Reads the content of a directory and, in case of success, returns a vector
// of strings with one element for each found file or directory. Each element is
// a path created by prepending |dir| to the file/directory name. "." and ".."
// are never added in the returned vector.
rtc::Optional<std::vector<std::string>> ReadDirectory(std::string path);

// Creates a directory if it not already exists.
// Returns true if successful. Will print an error message to stderr and return
// false if a file with the same name already exists.
bool CreateDir(const std::string& directory_name);

// Removes a directory, which must already be empty.
bool RemoveDir(const std::string& directory_name);

// Removes a file.
bool RemoveFile(const std::string& file_name);

// Checks if a file exists.
bool FileExists(const std::string& file_name);

// Checks if a directory exists.
bool DirExists(const std::string& directory_name);

// File size of the supplied file in bytes. Will return 0 if the file is
// empty or if the file does not exist/is readable.
size_t GetFileSize(const std::string& filename);

// Sets the executable path, i.e. the path to the executable that is being used
// when launching it. This is usually the path relative to the working directory
// but can also be an absolute path. The intention with this function is to pass
// the argv[0] being sent into the main function to make it possible for
// fileutils.h to find the correct project paths even when the working directory
// is outside the project tree (which happens in some cases).
void SetExecutablePath(const std::string& path_to_executable);

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_TEST_TESTSUPPORT_FILEUTILS_H_
