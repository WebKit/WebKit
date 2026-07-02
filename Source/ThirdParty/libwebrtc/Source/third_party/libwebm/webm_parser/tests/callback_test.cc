// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "webm/callback.h"

#include <cstdint>

#include "gtest/gtest.h"

#include "webm/buffer_reader.h"
#include "webm/element.h"
#include "webm/status.h"

using webm::Action;
using webm::BufferReader;
using webm::Callback;
using webm::ElementMetadata;
using webm::Reader;
using webm::Status;

namespace {

void TestCompletedOk(Status (Callback::*function)(const ElementMetadata&)) {
  Callback callback;
  ElementMetadata metadata{};

  Status status = (callback.*function)(metadata);
  EXPECT_EQ(Status::kOkCompleted, status.code);
}

template <typename T>
void TestCompletedOk(Status (Callback::*function)(const ElementMetadata&,
                                                  const T&)) {
  Callback callback;
  ElementMetadata metadata{};
  T object{};

  Status status = (callback.*function)(metadata, object);
  EXPECT_EQ(Status::kOkCompleted, status.code);
}

void TestAction(Status (Callback::*function)(const ElementMetadata&, Action*),
                Action expected) {
  Callback callback;
  ElementMetadata metadata{};
  Action action;

  Status status = (callback.*function)(metadata, &action);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(expected, action);
}

template <typename T>
void TestAction(Status (Callback::*function)(const ElementMetadata&, const T&,
                                             Action*),
                Action expected) {
  Callback callback;
  ElementMetadata metadata{};
  T t{};
  Action action;

  Status status = (callback.*function)(metadata, t, &action);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(expected, action);
}

template <typename T>
void TestRead(Status (Callback::*function)(const T&, Reader*, std::uint64_t*)) {
  Callback callback;
  Status status;
  T metadata{};
  BufferReader reader = {0x00, 0x01, 0x02, 0x03};
  std::uint64_t bytes_remaining = 4;

  status = (callback.*function)(metadata, &reader, &bytes_remaining);
  EXPECT_EQ(Status::kOkCompleted, status.code);
  EXPECT_EQ(static_cast<std::uint64_t>(0), bytes_remaining);
}

class CallbackTest : public testing::Test {};

TEST_F(CallbackTest, OnElementBegin) {
  TestAction(&Callback::OnElementBegin, Action::kRead);
}

TEST_F(CallbackTest, OnUnknownElement) {
  TestRead(&Callback::OnUnknownElement);
}

TEST_F(CallbackTest, OnEbml) { TestCompletedOk(&Callback::OnEbml); }

TEST_F(CallbackTest, OnVoid) { TestRead(&Callback::OnVoid); }

TEST_F(CallbackTest, OnSegmentBegin) {
  TestAction(&Callback::OnSegmentBegin, Action::kRead);
}

TEST_F(CallbackTest, OnSeek) { TestCompletedOk(&Callback::OnSeek); }

TEST_F(CallbackTest, OnInfo) { TestCompletedOk(&Callback::OnInfo); }

TEST_F(CallbackTest, OnClusterBegin) {
  TestAction(&Callback::OnClusterBegin, Action::kRead);
}

TEST_F(CallbackTest, OnSimpleBlockBegin) {
  TestAction(&Callback::OnSimpleBlockBegin, Action::kRead);
}

TEST_F(CallbackTest, OnSimpleBlockEnd) {
  TestCompletedOk(&Callback::OnSimpleBlockEnd);
}

TEST_F(CallbackTest, OnBlockGroupBegin) {
  TestAction(&Callback::OnBlockGroupBegin, Action::kRead);
}

TEST_F(CallbackTest, OnBlockBegin) {
  TestAction(&Callback::OnBlockBegin, Action::kRead);
}

TEST_F(CallbackTest, OnBlockEnd) { TestCompletedOk(&Callback::OnBlockEnd); }

TEST_F(CallbackTest, OnBlockGroupEnd) {
  TestCompletedOk(&Callback::OnBlockGroupEnd);
}

TEST_F(CallbackTest, OnFrame) { TestRead(&Callback::OnFrame); }

TEST_F(CallbackTest, OnClusterEnd) { TestCompletedOk(&Callback::OnClusterEnd); }

TEST_F(CallbackTest, OnTrackEntry) { TestCompletedOk(&Callback::OnTrackEntry); }

TEST_F(CallbackTest, OnCuePoint) { TestCompletedOk(&Callback::OnCuePoint); }

TEST_F(CallbackTest, OnEditionEntry) {
  TestCompletedOk(&Callback::OnEditionEntry);
}

TEST_F(CallbackTest, OnTag) { TestCompletedOk(&Callback::OnTag); }

TEST_F(CallbackTest, OnSegmentEnd) { TestCompletedOk(&Callback::OnSegmentEnd); }

}  // namespace
