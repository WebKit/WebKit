/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/channel_manager.h"

#include "webrtc/base/timeutils.h"
#include "webrtc/voice_engine/channel.h"

namespace webrtc {
namespace voe {

ChannelOwner::ChannelOwner(class Channel* channel)
    : channel_ref_(new ChannelRef(channel)) {}

ChannelOwner::ChannelOwner(const ChannelOwner& channel_owner)
    : channel_ref_(channel_owner.channel_ref_) {
  ++channel_ref_->ref_count;
}

ChannelOwner::~ChannelOwner() {
  if (--channel_ref_->ref_count == 0)
    delete channel_ref_;
}

ChannelOwner& ChannelOwner::operator=(const ChannelOwner& other) {
  if (other.channel_ref_ == channel_ref_)
    return *this;

  if (--channel_ref_->ref_count == 0)
    delete channel_ref_;

  channel_ref_ = other.channel_ref_;
  ++channel_ref_->ref_count;

  return *this;
}

ChannelOwner::ChannelRef::ChannelRef(class Channel* channel)
    : channel(channel), ref_count(1) {}

ChannelManager::ChannelManager(uint32_t instance_id)
    : instance_id_(instance_id),
      last_channel_id_(-1),
      random_(rtc::TimeNanos()) {}

ChannelOwner ChannelManager::CreateChannel(
    const VoEBase::ChannelConfig& config) {
  Channel* channel;
  Channel::CreateChannel(channel, ++last_channel_id_, instance_id_, config);
  // TODO(solenberg): Delete this, users should configure ssrc
  // explicitly.
  channel->SetLocalSSRC(random_.Rand<uint32_t>());

  ChannelOwner channel_owner(channel);

  rtc::CritScope crit(&lock_);

  channels_.push_back(channel_owner);

  return channel_owner;
}

ChannelOwner ChannelManager::GetChannel(int32_t channel_id) {
  rtc::CritScope crit(&lock_);

  for (size_t i = 0; i < channels_.size(); ++i) {
    if (channels_[i].channel()->ChannelId() == channel_id)
      return channels_[i];
  }
  return ChannelOwner(NULL);
}

void ChannelManager::GetAllChannels(std::vector<ChannelOwner>* channels) {
  rtc::CritScope crit(&lock_);

  *channels = channels_;
}

void ChannelManager::DestroyChannel(int32_t channel_id) {
  assert(channel_id >= 0);
  // Holds a reference to a channel, this is used so that we never delete
  // Channels while holding a lock, but rather when the method returns.
  ChannelOwner reference(NULL);
  {
    rtc::CritScope crit(&lock_);
    std::vector<ChannelOwner>::iterator to_delete = channels_.end();
    for (auto it = channels_.begin(); it != channels_.end(); ++it) {
      Channel* channel = it->channel();
      // For channels associated with the channel to be deleted, disassociate
      // with that channel.
      channel->DisassociateSendChannel(channel_id);

      if (channel->ChannelId() == channel_id) {
        to_delete = it;
      }
    }
    if (to_delete != channels_.end()) {
      reference = *to_delete;
      channels_.erase(to_delete);
    }
  }
  if (reference.channel()) {
    // Ensure the channel is torn down now, on this thread, since a reference
    // may still be held on a different thread (e.g. in the audio capture
    // thread).
    reference.channel()->Terminate();
  }
}

void ChannelManager::DestroyAllChannels() {
  // Holds references so that Channels are not destroyed while holding this
  // lock, but rather when the method returns.
  std::vector<ChannelOwner> references;
  {
    rtc::CritScope crit(&lock_);
    references = channels_;
    channels_.clear();
  }
  for (auto& owner : references) {
    if (owner.channel())
      owner.channel()->Terminate();
  }
}

size_t ChannelManager::NumOfChannels() const {
  rtc::CritScope crit(&lock_);
  return channels_.size();
}

ChannelManager::Iterator::Iterator(ChannelManager* channel_manager)
    : iterator_pos_(0) {
  channel_manager->GetAllChannels(&channels_);
}

Channel* ChannelManager::Iterator::GetChannel() {
  if (iterator_pos_ < channels_.size())
    return channels_[iterator_pos_].channel();
  return NULL;
}

bool ChannelManager::Iterator::IsValid() {
  return iterator_pos_ < channels_.size();
}

void ChannelManager::Iterator::Increment() {
  ++iterator_pos_;
}

}  // namespace voe
}  // namespace webrtc
