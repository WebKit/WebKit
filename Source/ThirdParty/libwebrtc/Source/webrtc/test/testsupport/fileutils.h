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

#ifndef TEST_TESTSUPPORT_FILEUTILS_H_
#define TEST_TESTSUPPORT_FILEUTILS_H_

#include <string>
#include <vector>

#include "absl/types/optional.h"

namespace webrtc {
namespace test {

// This is the "directory" returned if the ProjectPath() function fails
// to find the project root.
extern const char* kCannotFindProjectRootDir;

// Slash or backslash, depending on platform. NUL-terminated string.
extern const char* kPathDelimiter;

// Returns the absolute path to the output directory where log files and other
// test artifacts should be put. The output directory is generally a directory
// named "out" at the project root. This root is assumed to be two levels above
// where the test binary is located; this is because tests execute in a dir
// out/Whatever relative to the project root. This convention is also followed
// in Chromium.
//
// The exception is Android where we use /sdcard/ instead.
//
// If symbolic links occur in the path they will be resolved and the actual
// directory will be returned.
//
// Returns the path WITH a trailing path delimiter. If the project root is not
// found, the current working directory ("./") is returned as a fallback.
std::string OutputPath();

// Generates an empty file with a unique name in the specified directory and
// returns the file name and path.
// TODO(titovartem) rename to TempFile and next method to TempFilename
std::string TempFilename(const std::string& dir, const std::string& prefix);

// Generates a unique file name that can be used for file creation. Doesn't
// create any files.
std::string GenerateTempFilename(const std::string& dir,
                                 const std::string& prefix);

// Returns a path to a resource file in [project-root]/resources/ dir.
// Returns an absolute path
//
// Arguments:
//    name - Name of the resource file. If a plain filename (no directory path)
//           is supplied, the file is assumed to be located in resources/
//           If a directory path is prepended to the filename, a subdirectory
//           hierarchy reflecting that path is assumed to be present.
//    extension - File extension, without the dot, i.e. "bmp" or "yuv".
std::string ResourcePath(const std::string& name, const std::string& extension);

// Joins directory name and file name, separated by the path delimiter.
std::string JoinFilename(const std::string& dir, const std::string& name);

// Gets the current working directory for the executing program.
// Returns "./" if for some reason it is not possible to find the working
// directory.
std::string WorkingDir();

// Reads the content of a directory and, in case of success, returns a vector
// of strings with one element for each found file or directory. Each element is
// a path created by prepending |dir| to the file/directory name. "." and ".."
// are never added in the returned vector.
absl::optional<std::vector<std::string>> ReadDirectory(std::string path);

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

// Strips the rightmost path segment from a path.
std::string DirName(const std::string& path);

// File size of the supplied file in bytes. Will return 0 if the file is
// empty or if the file does not exist/is readable.
size_t GetFileSize(const std::string& filename);

}  // namespace test
}  // namespace webrtc

#endif  // TEST_TESTSUPPORT_FILEUTILS_H_
