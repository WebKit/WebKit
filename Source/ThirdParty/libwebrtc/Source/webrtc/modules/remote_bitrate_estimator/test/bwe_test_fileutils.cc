/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/bwe_test_fileutils.h"

#ifdef WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif
#include <assert.h>

#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {

ResourceFileReader::~ResourceFileReader() {
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
  }
}

bool ResourceFileReader::IsAtEnd() {
  int32_t current_pos = ftell(file_);
  fseek(file_, 0, SEEK_END);
  int32_t end_pos = ftell(file_);
  fseek(file_, current_pos, SEEK_SET);
  return current_pos == end_pos;
}

bool ResourceFileReader::Read(uint32_t* out) {
  assert(out);
  uint32_t tmp = 0;
  if (fread(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
    printf("Error reading!\n");
    return false;
  }
  *out = ntohl(tmp);
  return true;
}

ResourceFileReader* ResourceFileReader::Create(const std::string& filename,
                                               const std::string& extension) {
  std::string filepath = webrtc::test::ResourcePath(filename, extension);
  FILE* file = fopen(filepath.c_str(), "rb");
  if (file == NULL) {
    BWE_TEST_LOGGING_CONTEXT("ResourceFileReader");
    BWE_TEST_LOGGING_LOG1("Create", "Can't read file: %s", filepath.c_str());
    return 0;
  } else {
    return new ResourceFileReader(file);
  }
}

OutputFileWriter::~OutputFileWriter() {
  if (file_ != NULL) {
    fclose(file_);
    file_ = NULL;
  }
}

bool OutputFileWriter::Write(uint32_t value) {
  uint32_t tmp = htonl(value);
  if (fwrite(&tmp, 1, sizeof(uint32_t), file_) != sizeof(uint32_t)) {
    return false;
  }
  return true;
}

OutputFileWriter* OutputFileWriter::Create(const std::string& filename,
                                           const std::string& extension) {
  std::string filepath = webrtc::test::OutputPath() + filename + "." +
      extension;
  FILE* file = fopen(filepath.c_str(), "wb");
  if (file == NULL) {
    BWE_TEST_LOGGING_CONTEXT("OutputFileWriter");
    BWE_TEST_LOGGING_LOG1("Create", "Can't write file: %s", filepath.c_str());
    return NULL;
  } else {
    return new OutputFileWriter(file);
  }
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
