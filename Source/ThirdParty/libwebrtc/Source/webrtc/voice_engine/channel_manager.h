/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef VOICE_ENGINE_CHANNEL_MANAGER_H_
#define VOICE_ENGINE_CHANNEL_MANAGER_H_

#include <memory>
#include <vector>

#include "api/refcountedbase.h"
#include "rtc_base/constructormagic.h"
#include "rtc_base/criticalsection.h"
#include "rtc_base/random.h"
#include "rtc_base/scoped_ref_ptr.h"
#include "system_wrappers/include/atomic32.h"
#include "typedefs.h"  // NOLINT(build/include)
#include "voice_engine/include/voe_base.h"

namespace webrtc {

class AudioDecoderFactory;

namespace voe {

class Channel;

// Shared-pointer implementation for keeping track of Channels. The underlying
// shared instance will be dropped when no more ChannelOwners point to it.
//
// One common source of ChannelOwner instances are
// ChannelManager::CreateChannel() and ChannelManager::GetChannel(...).
// It has a similar use case to shared_ptr in C++11. Should this move to C++11
// in the future, this class should be replaced by exactly that.
//
// To access the underlying Channel, use .channel().
// IsValid() implements a convenience method as an alternative for checking
// whether the underlying pointer is NULL or not.
//
// Channel channel_owner = channel_manager.GetChannel(channel_id);
// if (channel_owner.IsValid())
//   channel_owner.channel()->...;
//
class ChannelOwner {
 public:
  explicit ChannelOwner(Channel* channel);
  ChannelOwner(const ChannelOwner& channel_owner) = default;

  ~ChannelOwner() = default;

  ChannelOwner& operator=(const ChannelOwner& other) = default;

  Channel* channel() const { return channel_ref_->channel.get(); }
  bool IsValid() { return channel_ref_->channel.get() != NULL; }
 private:
  // Shared instance of a Channel. Copying ChannelOwners increase the reference
  // count and destroying ChannelOwners decrease references. Channels are
  // deleted when no references to them are held.
  struct ChannelRef : public rtc::RefCountedBase {
    ChannelRef(Channel* channel);
    const std::unique_ptr<Channel> channel;
  };

  rtc::scoped_refptr<ChannelRef> channel_ref_;
};

class ChannelManager {
 public:
  ChannelManager(uint32_t instance_id);

  // Upon construction of an Iterator it will grab a copy of the channel list of
  // the ChannelManager. The iteration will then occur over this state, not the
  // current one of the ChannelManager. As the Iterator holds its own references
  // to the Channels, they will remain valid even if they are removed from the
  // ChannelManager.
  class Iterator {
   public:
    explicit Iterator(ChannelManager* channel_manager);

    Channel* GetChannel();
    bool IsValid();

    void Increment();

   private:
    size_t iterator_pos_;
    std::vector<ChannelOwner> channels_;

    RTC_DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  // CreateChannel will always return a valid ChannelOwner instance.
  ChannelOwner CreateChannel(const VoEBase::ChannelConfig& config);

  // ChannelOwner.channel() will be NULL if channel_id is invalid or no longer
  // exists. This should be checked with ChannelOwner::IsValid().
  ChannelOwner GetChannel(int32_t channel_id);
  void GetAllChannels(std::vector<ChannelOwner>* channels);

  void DestroyChannel(int32_t channel_id);
  void DestroyAllChannels();

  size_t NumOfChannels() const;

 private:
  uint32_t instance_id_;

  Atomic32 last_channel_id_;

  rtc::CriticalSection lock_;
  std::vector<ChannelOwner> channels_;

  // For generation of random ssrc:s.
  webrtc::Random random_;

  RTC_DISALLOW_COPY_AND_ASSIGN(ChannelManager);
};
}  // namespace voe
}  // namespace webrtc

#endif  // VOICE_ENGINE_CHANNEL_MANAGER_H_
