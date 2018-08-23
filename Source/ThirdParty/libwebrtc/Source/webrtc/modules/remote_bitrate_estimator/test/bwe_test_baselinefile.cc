/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/remote_bitrate_estimator/test/bwe_test_baselinefile.h"

#include <stdio.h>

#include <algorithm>
#include <memory>
#include <vector>

#include "modules/remote_bitrate_estimator/test/bwe_test_fileutils.h"
#include "modules/remote_bitrate_estimator/test/bwe_test_logging.h"
#include "rtc_base/constructormagic.h"
#include "test/testsupport/fileutils.h"

namespace webrtc {
namespace testing {
namespace bwe {

// The format of BWE test baseline files is extremely simple:
//   1. All read/written entities are 32-bit unsigned integers in network byte
//      order (Big Endian).
//   2. Files beging with a 2 word header containing a magic marker and file
//      format version indicator. The Magic marker reads "BWE!" in a hex dump.
//   3. Each estimate is logged as a pair of words: time in milliseconds and
//      estimated bit rate, in bits per second.
const uint32_t kMagicMarker = 0x42574521;
const uint32_t kFileVersion1 = 0x00000001;
const char kResourceSubDir[] = "remote_bitrate_estimator";

class BaseLineFileVerify : public BaseLineFileInterface {
 public:
  // If |allow_missing_file| is set, VerifyOrWrite() will return true even if
  // the baseline file is missing. This is the default when verifying files, but
  // not when updating (i.e. we always write it out if missing).
  BaseLineFileVerify(const std::string& filepath, bool allow_missing_file)
      : reader_(), fail_to_read_response_(false) {
    std::unique_ptr<ResourceFileReader> reader;
    reader.reset(ResourceFileReader::Create(filepath, "bin"));
    if (!reader.get()) {
      printf("WARNING: Missing baseline file for BWE test: %s.bin\n",
             filepath.c_str());
      fail_to_read_response_ = allow_missing_file;
    } else {
      uint32_t magic_marker = 0;
      uint32_t file_version = 0;
      if (reader->Read(&magic_marker) && magic_marker == kMagicMarker &&
          reader->Read(&file_version) && file_version == kFileVersion1) {
        reader_.swap(reader);
      } else {
        printf("WARNING: Bad baseline file header for BWE test: %s.bin\n",
               filepath.c_str());
      }
    }
  }
  ~BaseLineFileVerify() override {}

  void Estimate(int64_t time_ms, uint32_t estimate_bps) override {
    if (reader_.get()) {
      uint32_t read_ms = 0;
      uint32_t read_bps = 0;
      if (reader_->Read(&read_ms) && read_ms == time_ms &&
          reader_->Read(&read_bps) && read_bps == estimate_bps) {
      } else {
        printf("ERROR: Baseline differs starting at: %d ms (%d vs %d)!\n",
               static_cast<uint32_t>(time_ms), estimate_bps, read_bps);
        reader_.reset(NULL);
      }
    }
  }

  bool VerifyOrWrite() override {
    if (reader_.get()) {
      if (reader_->IsAtEnd()) {
        return true;
      } else {
        printf("ERROR: Baseline file contains more data!\n");
        return false;
      }
    }
    return fail_to_read_response_;
  }

 private:
  std::unique_ptr<ResourceFileReader> reader_;
  bool fail_to_read_response_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BaseLineFileVerify);
};

class BaseLineFileUpdate : public BaseLineFileInterface {
 public:
  BaseLineFileUpdate(const std::string& filepath,
                     BaseLineFileInterface* verifier)
      : verifier_(verifier), output_content_(), filepath_(filepath) {
    output_content_.push_back(kMagicMarker);
    output_content_.push_back(kFileVersion1);
  }
  ~BaseLineFileUpdate() override {}

  void Estimate(int64_t time_ms, uint32_t estimate_bps) override {
    verifier_->Estimate(time_ms, estimate_bps);
    output_content_.push_back(static_cast<uint32_t>(time_ms));
    output_content_.push_back(estimate_bps);
  }

  bool VerifyOrWrite() override {
    if (!verifier_->VerifyOrWrite()) {
      std::string dir_path = webrtc::test::OutputPath() + kResourceSubDir;
      if (!webrtc::test::CreateDir(dir_path)) {
        printf("WARNING: Cannot create output dir: %s\n", dir_path.c_str());
        return false;
      }
      std::unique_ptr<OutputFileWriter> writer;
      writer.reset(OutputFileWriter::Create(filepath_, "bin"));
      if (!writer.get()) {
        printf("WARNING: Cannot create output file: %s.bin\n",
               filepath_.c_str());
        return false;
      }
      printf("NOTE: Writing baseline file for BWE test: %s.bin\n",
             filepath_.c_str());
      for (std::vector<uint32_t>::iterator it = output_content_.begin();
           it != output_content_.end(); ++it) {
        writer->Write(*it);
      }
      return true;
    }
    printf("NOTE: No change, not writing: %s\n", filepath_.c_str());
    return true;
  }

 private:
  std::unique_ptr<BaseLineFileInterface> verifier_;
  std::vector<uint32_t> output_content_;
  std::string filepath_;

  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(BaseLineFileUpdate);
};

BaseLineFileInterface* BaseLineFileInterface::Create(
    const std::string& filename,
    bool write_output_file) {
  std::string filepath = filename;
  std::replace(filepath.begin(), filepath.end(), '/', '_');
  filepath = std::string(kResourceSubDir) + "/" + filepath;

  std::unique_ptr<BaseLineFileInterface> result;
  result.reset(new BaseLineFileVerify(filepath, !write_output_file));
  if (write_output_file) {
    // Takes ownership of the |verifier| instance.
    result.reset(new BaseLineFileUpdate(filepath, result.release()));
  }
  return result.release();
}
}  // namespace bwe
}  // namespace testing
}  // namespace webrtc
