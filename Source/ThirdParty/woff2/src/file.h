// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// File IO helpers

#ifndef WOFF2_FILE_H_
#define WOFF2_FILE_H_

#include <fstream>
#include <iterator>

namespace woff2 {

inline std::string GetFileContent(std::string filename) {
  std::ifstream ifs(filename.c_str(), std::ios::binary);
  return std::string(
    std::istreambuf_iterator<char>(ifs.rdbuf()),
    std::istreambuf_iterator<char>());
}

inline void SetFileContents(std::string filename, std::string content) {
  std::ofstream ofs(filename.c_str(), std::ios::binary);
  std::copy(content.begin(),
            content.end(),
            std::ostream_iterator<char>(ofs));
}

} // namespace woff2
#endif  // WOFF2_FILE_H_
