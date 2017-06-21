/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_
#define WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/thread_annotations.h"
#include "webrtc/modules/include/module_common_types.h"
#include "webrtc/modules/utility/include/process_thread.h"
#include "webrtc/modules/video_coding/include/video_coding.h"
#include "webrtc/modules/video_coding/include/video_coding_defines.h"
#include "webrtc/modules/video_coding/decoding_state.h"
#include "webrtc/modules/video_coding/inter_frame_delay.h"
#include "webrtc/modules/video_coding/jitter_buffer_common.h"
#include "webrtc/modules/video_coding/jitter_estimator.h"
#include "webrtc/modules/video_coding/nack_module.h"
#include "webrtc/typedefs.h"

namespace webrtc {

enum VCMNackMode { kNack, kNoNack };

// forward declarations
class Clock;
class EventFactory;
class EventWrapper;
class VCMFrameBuffer;
class VCMPacket;
class VCMEncodedFrame;

typedef std::list<VCMFrameBuffer*> UnorderedFrameList;

struct VCMJitterSample {
  VCMJitterSample() : timestamp(0), frame_size(0), latest_packet_time(-1) {}
  uint32_t timestamp;
  uint32_t frame_size;
  int64_t latest_packet_time;
};

class TimestampLessThan {
 public:
  bool operator()(uint32_t timestamp1, uint32_t timestamp2) const {
    return IsNewerTimestamp(timestamp2, timestamp1);
  }
};

class FrameList
    : public std::map<uint32_t, VCMFrameBuffer*, TimestampLessThan> {
 public:
  void InsertFrame(VCMFrameBuffer* frame);
  VCMFrameBuffer* PopFrame(uint32_t timestamp);
  VCMFrameBuffer* Front() const;
  VCMFrameBuffer* Back() const;
  int RecycleFramesUntilKeyFrame(FrameList::iterator* key_frame_it,
                                 UnorderedFrameList* free_frames);
  void CleanUpOldOrEmptyFrames(VCMDecodingState* decoding_state,
                               UnorderedFrameList* free_frames);
  void Reset(UnorderedFrameList* free_frames);
};

class Vp9SsMap {
 public:
  typedef std::map<uint32_t, GofInfoVP9, TimestampLessThan> SsMap;
  bool Insert(const VCMPacket& packet);
  void Reset();

  // Removes SS data that are older than |timestamp|.
  // The |timestamp| should be an old timestamp, i.e. packets with older
  // timestamps should no longer be inserted.
  void RemoveOld(uint32_t timestamp);

  bool UpdatePacket(VCMPacket* packet);
  void UpdateFrames(FrameList* frames);

  // Public for testing.
  // Returns an iterator to the corresponding SS data for the input |timestamp|.
  bool Find(uint32_t timestamp, SsMap::iterator* it);

 private:
  // These two functions are called by RemoveOld.
  // Checks if it is time to do a clean up (done each kSsCleanupIntervalSec).
  bool TimeForCleanup(uint32_t timestamp) const;

  // Advances the oldest SS data to handle timestamp wrap in cases where SS data
  // are received very seldom (e.g. only once in beginning, second when
  // IsNewerTimestamp is not true).
  void AdvanceFront(uint32_t timestamp);

  SsMap ss_map_;
};

class VCMJitterBuffer {
 public:
  VCMJitterBuffer(Clock* clock,
                  std::unique_ptr<EventWrapper> event,
                  NackSender* nack_sender = nullptr,
                  KeyFrameRequestSender* keyframe_request_sender = nullptr);

  ~VCMJitterBuffer();

  // Initializes and starts jitter buffer.
  void Start();

  // Signals all internal events and stops the jitter buffer.
  void Stop();

  // Returns true if the jitter buffer is running.
  bool Running() const;

  // Empty the jitter buffer of all its data.
  void Flush();

  // Get the number of received frames, by type, since the jitter buffer
  // was started.
  FrameCounts FrameStatistics() const;

  // Gets number of packets received.
  int num_packets() const;

  // Gets number of duplicated packets received.
  int num_duplicated_packets() const;

  // Gets number of packets discarded by the jitter buffer.
  int num_discarded_packets() const;

  // Statistics, Calculate frame and bit rates.
  void IncomingRateStatistics(unsigned int* framerate, unsigned int* bitrate);

  // Wait |max_wait_time_ms| for a complete frame to arrive.
  // If found, a pointer to the frame is returned. Returns nullptr otherwise.
  VCMEncodedFrame* NextCompleteFrame(uint32_t max_wait_time_ms);

  // Locates a frame for decoding (even an incomplete) without delay.
  // The function returns true once such a frame is found, its corresponding
  // timestamp is returned. Otherwise, returns false.
  bool NextMaybeIncompleteTimestamp(uint32_t* timestamp);

  // Extract frame corresponding to input timestamp.
  // Frame will be set to a decoding state.
  VCMEncodedFrame* ExtractAndSetDecode(uint32_t timestamp);

  // Releases a frame returned from the jitter buffer, should be called when
  // done with decoding.
  void ReleaseFrame(VCMEncodedFrame* frame);

  // Returns the time in ms when the latest packet was inserted into the frame.
  // Retransmitted is set to true if any of the packets belonging to the frame
  // has been retransmitted.
  int64_t LastPacketTime(const VCMEncodedFrame* frame,
                         bool* retransmitted) const;

  // Inserts a packet into a frame returned from GetFrame().
  // If the return value is <= 0, |frame| is invalidated and the pointer must
  // be dropped after this function returns.
  VCMFrameBufferEnum InsertPacket(const VCMPacket& packet, bool* retransmitted);

  // Returns the estimated jitter in milliseconds.
  uint32_t EstimatedJitterMs();

  // Updates the round-trip time estimate.
  void UpdateRtt(int64_t rtt_ms);

  // Set the NACK mode. |high_rtt_nack_threshold_ms| is an RTT threshold in ms
  // above which NACK will be disabled if the NACK mode is |kNack|, -1 meaning
  // that NACK is always enabled in the |kNack| mode.
  // |low_rtt_nack_threshold_ms| is an RTT threshold in ms below which we expect
  // to rely on NACK only, and therefore are using larger buffers to have time
  // to wait for retransmissions.
  void SetNackMode(VCMNackMode mode,
                   int64_t low_rtt_nack_threshold_ms,
                   int64_t high_rtt_nack_threshold_ms);

  void SetNackSettings(size_t max_nack_list_size,
                       int max_packet_age_to_nack,
                       int max_incomplete_time_ms);

  // Returns the current NACK mode.
  VCMNackMode nack_mode() const;

  // Returns a list of the sequence numbers currently missing.
  std::vector<uint16_t> GetNackList(bool* request_key_frame);

  // Set decode error mode - Should not be changed in the middle of the
  // session. Changes will not influence frames already in the buffer.
  void SetDecodeErrorMode(VCMDecodeErrorMode error_mode);
  VCMDecodeErrorMode decode_error_mode() const { return decode_error_mode_; }

  void RegisterStatsCallback(VCMReceiveStatisticsCallback* callback);

 private:
  class SequenceNumberLessThan {
   public:
    bool operator()(const uint16_t& sequence_number1,
                    const uint16_t& sequence_number2) const {
      return IsNewerSequenceNumber(sequence_number2, sequence_number1);
    }
  };
  typedef std::set<uint16_t, SequenceNumberLessThan> SequenceNumberSet;

  // Gets the frame assigned to the timestamp of the packet. May recycle
  // existing frames if no free frames are available. Returns an error code if
  // failing, or kNoError on success. |frame_list| contains which list the
  // packet was in, or NULL if it was not in a FrameList (a new frame).
  VCMFrameBufferEnum GetFrame(const VCMPacket& packet,
                              VCMFrameBuffer** frame,
                              FrameList** frame_list)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Returns true if |frame| is continuous in |decoding_state|, not taking
  // decodable frames into account.
  bool IsContinuousInState(const VCMFrameBuffer& frame,
                           const VCMDecodingState& decoding_state) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Returns true if |frame| is continuous in the |last_decoded_state_|, taking
  // all decodable frames into account.
  bool IsContinuous(const VCMFrameBuffer& frame) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Looks for frames in |incomplete_frames_| which are continuous in the
  // provided |decoded_state|. Starts the search from the timestamp of
  // |decoded_state|.
  void FindAndInsertContinuousFramesWithState(
      const VCMDecodingState& decoded_state)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Looks for frames in |incomplete_frames_| which are continuous in
  // |last_decoded_state_| taking all decodable frames into account. Starts
  // the search from |new_frame|.
  void FindAndInsertContinuousFrames(const VCMFrameBuffer& new_frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  VCMFrameBuffer* NextFrame() const EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Returns true if the NACK list was updated to cover sequence numbers up to
  // |sequence_number|. If false a key frame is needed to get into a state where
  // we can continue decoding.
  bool UpdateNackList(uint16_t sequence_number)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  bool TooLargeNackList() const;
  // Returns true if the NACK list was reduced without problem. If false a key
  // frame is needed to get into a state where we can continue decoding.
  bool HandleTooLargeNackList() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  bool MissingTooOldPacket(uint16_t latest_sequence_number) const
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Returns true if the too old packets was successfully removed from the NACK
  // list. If false, a key frame is needed to get into a state where we can
  // continue decoding.
  bool HandleTooOldPackets(uint16_t latest_sequence_number)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);
  // Drops all packets in the NACK list up until |last_decoded_sequence_number|.
  void DropPacketsFromNackList(uint16_t last_decoded_sequence_number);

  // Gets an empty frame, creating a new frame if necessary (i.e. increases
  // jitter buffer size).
  VCMFrameBuffer* GetEmptyFrame() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Attempts to increase the size of the jitter buffer. Returns true on
  // success, false otherwise.
  bool TryToIncreaseJitterBufferSize() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Recycles oldest frames until a key frame is found. Used if jitter buffer is
  // completely full. Returns true if a key frame was found.
  bool RecycleFramesUntilKeyFrame() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Updates the frame statistics.
  // Counts only complete frames, so decodable incomplete frames will not be
  // counted.
  void CountFrame(const VCMFrameBuffer& frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Update rolling average of packets per frame.
  void UpdateAveragePacketsPerFrame(int current_number_packets_);

  // Cleans the frame list in the JB from old/empty frames.
  // Should only be called prior to actual use.
  void CleanUpOldOrEmptyFrames() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Returns true if |packet| is likely to have been retransmitted.
  bool IsPacketRetransmitted(const VCMPacket& packet) const;

  // The following three functions update the jitter estimate with the
  // payload size, receive time and RTP timestamp of a frame.
  void UpdateJitterEstimate(const VCMJitterSample& sample,
                            bool incomplete_frame);
  void UpdateJitterEstimate(const VCMFrameBuffer& frame, bool incomplete_frame);
  void UpdateJitterEstimate(int64_t latest_packet_time_ms,
                            uint32_t timestamp,
                            unsigned int frame_size,
                            bool incomplete_frame);

  // Returns true if we should wait for retransmissions, false otherwise.
  bool WaitForRetransmissions();

  int NonContinuousOrIncompleteDuration() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  uint16_t EstimatedLowSequenceNumber(const VCMFrameBuffer& frame) const;

  void UpdateHistograms() EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  // Reset frame buffer and return it to free_frames_.
  void RecycleFrameBuffer(VCMFrameBuffer* frame)
      EXCLUSIVE_LOCKS_REQUIRED(crit_sect_);

  Clock* clock_;
  // If we are running (have started) or not.
  bool running_;
  rtc::CriticalSection crit_sect_;
  // Event to signal when we have a frame ready for decoder.
  std::unique_ptr<EventWrapper> frame_event_;
  // Number of allocated frames.
  int max_number_of_frames_;
  UnorderedFrameList free_frames_ GUARDED_BY(crit_sect_);
  FrameList decodable_frames_ GUARDED_BY(crit_sect_);
  FrameList incomplete_frames_ GUARDED_BY(crit_sect_);
  VCMDecodingState last_decoded_state_ GUARDED_BY(crit_sect_);
  bool first_packet_since_reset_;

  // Statistics.
  VCMReceiveStatisticsCallback* stats_callback_ GUARDED_BY(crit_sect_);
  // Frame counts for each type (key, delta, ...)
  FrameCounts receive_statistics_;
  // Latest calculated frame rates of incoming stream.
  unsigned int incoming_frame_rate_;
  unsigned int incoming_frame_count_;
  int64_t time_last_incoming_frame_count_;
  unsigned int incoming_bit_count_;
  unsigned int incoming_bit_rate_;
  // Number of packets in a row that have been too old.
  int num_consecutive_old_packets_;
  // Number of packets received.
  int num_packets_ GUARDED_BY(crit_sect_);
  // Number of duplicated packets received.
  int num_duplicated_packets_ GUARDED_BY(crit_sect_);
  // Number of packets discarded by the jitter buffer.
  int num_discarded_packets_ GUARDED_BY(crit_sect_);
  // Time when first packet is received.
  int64_t time_first_packet_ms_ GUARDED_BY(crit_sect_);

  // Jitter estimation.
  // Filter for estimating jitter.
  VCMJitterEstimator jitter_estimate_;
  // Calculates network delays used for jitter calculations.
  VCMInterFrameDelay inter_frame_delay_;
  VCMJitterSample waiting_for_completion_;
  int64_t rtt_ms_;

  // NACK and retransmissions.
  VCMNackMode nack_mode_;
  int64_t low_rtt_nack_threshold_ms_;
  int64_t high_rtt_nack_threshold_ms_;
  // Holds the internal NACK list (the missing sequence numbers).
  SequenceNumberSet missing_sequence_numbers_;
  uint16_t latest_received_sequence_number_;
  size_t max_nack_list_size_;
  int max_packet_age_to_nack_;  // Measured in sequence numbers.
  int max_incomplete_time_ms_;

  VCMDecodeErrorMode decode_error_mode_;
  // Estimated rolling average of packets per frame
  float average_packets_per_frame_;
  // average_packets_per_frame converges fast if we have fewer than this many
  // frames.
  int frame_counter_;

  RTC_DISALLOW_COPY_AND_ASSIGN(VCMJitterBuffer);
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_JITTER_BUFFER_H_
