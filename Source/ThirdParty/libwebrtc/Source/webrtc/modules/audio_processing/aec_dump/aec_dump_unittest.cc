/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/modules/audio_processing/aec_dump/aec_dump_factory.h"

#include "webrtc/base/task_queue.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/test/gtest.h"
#include "webrtc/test/testsupport/fileutils.h"

TEST(AecDumper, APICallsDoNotCrash) {
  // Note order of initialization: Task queue has to be initialized
  // before AecDump.
  rtc::TaskQueue file_writer_queue("file_writer_queue");

  const std::string filename =
      webrtc::test::TempFilename(webrtc::test::OutputPath(), "aec_dump");

  {
    std::unique_ptr<webrtc::AecDump> aec_dump =
        webrtc::AecDumpFactory::Create(filename, -1, &file_writer_queue);

    const webrtc::AudioFrame frame;
    aec_dump->WriteRenderStreamMessage(frame);

    aec_dump->AddCaptureStreamInput(frame);
    aec_dump->AddCaptureStreamOutput(frame);

    aec_dump->WriteCaptureStreamMessage();

    webrtc::InternalAPMConfig apm_config;
    aec_dump->WriteConfig(apm_config);

    webrtc::InternalAPMStreamsConfig streams_config;
    aec_dump->WriteInitMessage(streams_config);
  }
  // Remove file after the AecDump d-tor has finished.
  ASSERT_EQ(0, remove(filename.c_str()));
}

TEST(AecDumper, WriteToFile) {
  rtc::TaskQueue file_writer_queue("file_writer_queue");

  const std::string filename =
      webrtc::test::TempFilename(webrtc::test::OutputPath(), "aec_dump");

  {
    std::unique_ptr<webrtc::AecDump> aec_dump =
        webrtc::AecDumpFactory::Create(filename, -1, &file_writer_queue);
    const webrtc::AudioFrame frame;
    aec_dump->WriteRenderStreamMessage(frame);
  }

  // Verify the file has been written after the AecDump d-tor has
  // finished.
  FILE* fid = fopen(filename.c_str(), "r");
  ASSERT_TRUE(fid != NULL);

  // Clean it up.
  ASSERT_EQ(0, fclose(fid));
  ASSERT_EQ(0, remove(filename.c_str()));
}
