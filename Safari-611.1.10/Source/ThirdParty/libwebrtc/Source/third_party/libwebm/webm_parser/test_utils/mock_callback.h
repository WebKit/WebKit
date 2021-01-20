// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#ifndef TEST_UTILS_MOCK_CALLBACK_H_
#define TEST_UTILS_MOCK_CALLBACK_H_

#include <cstdint>

#include "gmock/gmock.h"

#include "webm/callback.h"
#include "webm/dom_types.h"
#include "webm/reader.h"
#include "webm/status.h"

namespace webm {

// A simple version of Callback that can be used with Google Mock. By default,
// the mocked methods will call through to the corresponding Callback methods.
class MockCallback : public Callback {
 public:
  MockCallback() {
    using testing::_;
    using testing::Invoke;

    ON_CALL(*this, OnElementBegin(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnElementBeginConcrete));
    ON_CALL(*this, OnUnknownElement(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnUnknownElementConcrete));
    ON_CALL(*this, OnEbml(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnEbmlConcrete));
    ON_CALL(*this, OnVoid(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnVoidConcrete));
    ON_CALL(*this, OnSegmentBegin(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnSegmentBeginConcrete));
    ON_CALL(*this, OnSeek(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnSeekConcrete));
    ON_CALL(*this, OnInfo(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnInfoConcrete));
    ON_CALL(*this, OnClusterBegin(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnClusterBeginConcrete));
    ON_CALL(*this, OnSimpleBlockBegin(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnSimpleBlockBeginConcrete));
    ON_CALL(*this, OnSimpleBlockEnd(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnSimpleBlockEndConcrete));
    ON_CALL(*this, OnBlockGroupBegin(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnBlockGroupBeginConcrete));
    ON_CALL(*this, OnBlockBegin(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnBlockBeginConcrete));
    ON_CALL(*this, OnBlockEnd(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnBlockEndConcrete));
    ON_CALL(*this, OnBlockGroupEnd(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnBlockGroupEndConcrete));
    ON_CALL(*this, OnFrame(_, _, _))
        .WillByDefault(Invoke(this, &MockCallback::OnFrameConcrete));
    ON_CALL(*this, OnClusterEnd(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnClusterEndConcrete));
    ON_CALL(*this, OnTrackEntry(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnTrackEntryConcrete));
    ON_CALL(*this, OnCuePoint(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnCuePointConcrete));
    ON_CALL(*this, OnEditionEntry(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnEditionEntryConcrete));
    ON_CALL(*this, OnTag(_, _))
        .WillByDefault(Invoke(this, &MockCallback::OnTagConcrete));
    ON_CALL(*this, OnSegmentEnd(_))
        .WillByDefault(Invoke(this, &MockCallback::OnSegmentEndConcrete));
  }

  // Mocks for methods from Callback.
  MOCK_METHOD2(OnElementBegin,
               Status(const ElementMetadata& metadata, Action* action));

  MOCK_METHOD3(OnUnknownElement,
               Status(const ElementMetadata& metadata, Reader* reader,
                      std::uint64_t* bytes_remaining));

  MOCK_METHOD2(OnEbml,
               Status(const ElementMetadata& metadata, const Ebml& ebml));

  MOCK_METHOD3(OnVoid, Status(const ElementMetadata& metadata, Reader* reader,
                              std::uint64_t* bytes_remaining));

  MOCK_METHOD2(OnSegmentBegin,
               Status(const ElementMetadata& metadata, Action* action));

  MOCK_METHOD2(OnSeek,
               Status(const ElementMetadata& metadata, const Seek& seek));

  MOCK_METHOD2(OnInfo,
               Status(const ElementMetadata& metadata, const Info& info));

  MOCK_METHOD3(OnClusterBegin, Status(const ElementMetadata& metadata,
                                      const Cluster& cluster, Action* action));

  MOCK_METHOD3(OnSimpleBlockBegin,
               Status(const ElementMetadata& metadata,
                      const SimpleBlock& simple_block, Action* action));

  MOCK_METHOD2(OnSimpleBlockEnd, Status(const ElementMetadata& metadata,
                                        const SimpleBlock& simple_block));

  MOCK_METHOD2(OnBlockGroupBegin,
               Status(const ElementMetadata& metadata, Action* action));

  MOCK_METHOD3(OnBlockBegin, Status(const ElementMetadata& metadata,
                                    const Block& block, Action* action));

  MOCK_METHOD2(OnBlockEnd,
               Status(const ElementMetadata& metadata, const Block& block));

  MOCK_METHOD2(OnBlockGroupEnd, Status(const ElementMetadata& metadata,
                                       const BlockGroup& block_group));

  MOCK_METHOD3(OnFrame, Status(const FrameMetadata& metadata, Reader* reader,
                               std::uint64_t* bytes_remaining));

  MOCK_METHOD2(OnClusterEnd,
               Status(const ElementMetadata& metadata, const Cluster& cluster));

  MOCK_METHOD2(OnTrackEntry, Status(const ElementMetadata& metadata,
                                    const TrackEntry& track_entry));

  MOCK_METHOD2(OnCuePoint, Status(const ElementMetadata& metadata,
                                  const CuePoint& cue_point));

  MOCK_METHOD2(OnEditionEntry, Status(const ElementMetadata& metadata,
                                      const EditionEntry& edition_entry));

  MOCK_METHOD2(OnTag, Status(const ElementMetadata& metadata, const Tag& tag));

  MOCK_METHOD1(OnSegmentEnd, Status(const ElementMetadata& metadata));

  // Concrete implementations that the corresponding mocked method may call,
  // provided for convenience. These methods just call through to the
  // corrensponding methods in Callback, and provide an convenient way for the
  // MockCallback to exhibit the same behavior as Callback.
  Status OnElementBeginConcrete(const ElementMetadata& metadata,
                                Action* action) {
    return Callback::OnElementBegin(metadata, action);
  }

  Status OnUnknownElementConcrete(const ElementMetadata& metadata,
                                  Reader* reader,
                                  std::uint64_t* bytes_remaining) {
    return Callback::OnUnknownElement(metadata, reader, bytes_remaining);
  }

  Status OnEbmlConcrete(const ElementMetadata& metadata, const Ebml& ebml) {
    return Callback::OnEbml(metadata, ebml);
  }

  Status OnVoidConcrete(const ElementMetadata& metadata, Reader* reader,
                        std::uint64_t* bytes_remaining) {
    return Callback::OnVoid(metadata, reader, bytes_remaining);
  }

  Status OnSegmentBeginConcrete(const ElementMetadata& metadata,
                                Action* action) {
    return Callback::OnSegmentBegin(metadata, action);
  }

  Status OnSeekConcrete(const ElementMetadata& metadata, const Seek& seek) {
    return Callback::OnSeek(metadata, seek);
  }

  Status OnInfoConcrete(const ElementMetadata& metadata, const Info& info) {
    return Callback::OnInfo(metadata, info);
  }

  Status OnClusterBeginConcrete(const ElementMetadata& metadata,
                                const Cluster& cluster, Action* action) {
    return Callback::OnClusterBegin(metadata, cluster, action);
  }

  Status OnSimpleBlockBeginConcrete(const ElementMetadata& metadata,
                                    const SimpleBlock& simple_block,
                                    Action* action) {
    return Callback::OnSimpleBlockBegin(metadata, simple_block, action);
  }

  Status OnSimpleBlockEndConcrete(const ElementMetadata& metadata,
                                  const SimpleBlock& simple_block) {
    return Callback::OnSimpleBlockEnd(metadata, simple_block);
  }

  Status OnBlockGroupBeginConcrete(const ElementMetadata& metadata,
                                   Action* action) {
    return Callback::OnBlockGroupBegin(metadata, action);
  }

  Status OnBlockBeginConcrete(const ElementMetadata& metadata,
                              const Block& block, Action* action) {
    return Callback::OnBlockBegin(metadata, block, action);
  }

  Status OnBlockEndConcrete(const ElementMetadata& metadata,
                            const Block& block) {
    return Callback::OnBlockEnd(metadata, block);
  }

  Status OnBlockGroupEndConcrete(const ElementMetadata& metadata,
                                 const BlockGroup& block_group) {
    return Callback::OnBlockGroupEnd(metadata, block_group);
  }

  Status OnFrameConcrete(const FrameMetadata& metadata, Reader* reader,
                         std::uint64_t* bytes_remaining) {
    return Callback::OnFrame(metadata, reader, bytes_remaining);
  }

  Status OnClusterEndConcrete(const ElementMetadata& metadata,
                              const Cluster& cluster) {
    return Callback::OnClusterEnd(metadata, cluster);
  }

  Status OnTrackEntryConcrete(const ElementMetadata& metadata,
                              const TrackEntry& track_entry) {
    return Callback::OnTrackEntry(metadata, track_entry);
  }

  Status OnCuePointConcrete(const ElementMetadata& metadata,
                            const CuePoint& cue_point) {
    return Callback::OnCuePoint(metadata, cue_point);
  }

  Status OnEditionEntryConcrete(const ElementMetadata& metadata,
                                const EditionEntry& edition_entry) {
    return Callback::OnEditionEntry(metadata, edition_entry);
  }

  Status OnTagConcrete(const ElementMetadata& metadata, const Tag& tag) {
    return Callback::OnTag(metadata, tag);
  }

  Status OnSegmentEndConcrete(const ElementMetadata& metadata) {
    return Callback::OnSegmentEnd(metadata);
  }
};

}  // namespace webm

#endif  // TEST_UTILS_MOCK_CALLBACK_H_
