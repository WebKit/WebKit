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
// A very simple commandline tool for decompressing woff2 format files to true
// type font files.

#include <string>


#include "file.h"
#include "./woff2_dec.h"

int main(int argc, char **argv) {
  using std::string;

  if (argc != 2) {
    fprintf(stderr, "One argument, the input filename, must be provided.\n");
    return 1;
  }

  string filename(argv[1]);
  string outfilename = filename.substr(0, filename.find_last_of(".")) + ".ttf";
  fprintf(stdout, "Processing %s => %s\n",
    filename.c_str(), outfilename.c_str());
  string input = woff2::GetFileContent(filename);

  size_t decompressed_size = woff2::ComputeWOFF2FinalSize(
      reinterpret_cast<const uint8_t*>(input.data()), input.size());
  string output(decompressed_size, 0);
  const bool ok = woff2::ConvertWOFF2ToTTF(
      reinterpret_cast<uint8_t*>(&output[0]), decompressed_size,
      reinterpret_cast<const uint8_t*>(input.data()), input.size());

  if (!ok) {
    fprintf(stderr, "Decompression failed\n");
    return 1;
  }

  woff2::SetFileContents(outfilename, output);

  return 0;
}

