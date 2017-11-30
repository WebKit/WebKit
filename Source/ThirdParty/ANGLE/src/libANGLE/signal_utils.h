//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils:
//   Helper classes for tracking dependent state changes between objects.
//   These changes are signaled to the dependent class via channels.
//   See design document:
//   https://docs.google.com/document/d/15Edfotqg6_l1skTEL8ADQudF_oIdNa7i8Po43k6jMd4/

#ifndef LIBANGLE_SIGNAL_UTILS_H_
#define LIBANGLE_SIGNAL_UTILS_H_

#include <set>

#include "common/angleutils.h"
#include "common/debug.h"

namespace angle
{

// Interface that the depending class inherits from.
template <typename ChannelID = uint32_t, typename... MessageT>
class SignalReceiver
{
  public:
    virtual ~SignalReceiver() = default;
    virtual void signal(ChannelID channelID, MessageT... message) = 0;
};

template <typename ChannelID, typename... MessageT>
class ChannelBinding;

// The host class owns the channel. It uses the channel to fire signals to the receiver.
template <typename ChannelID = uint32_t, typename... MessageT>
class BroadcastChannel final : NonCopyable
{
  public:
    BroadcastChannel();
    ~BroadcastChannel();

    void signal(MessageT... message) const;

    void reset();

    bool empty() const;

  private:
    // Only the ChannelBinding class should add or remove receivers.
    friend class ChannelBinding<ChannelID, MessageT...>;
    void addReceiver(ChannelBinding<ChannelID, MessageT...> *receiver);
    void removeReceiver(ChannelBinding<ChannelID, MessageT...> *receiver);

    std::vector<ChannelBinding<ChannelID, MessageT...> *> mReceivers;
};

template <typename ChannelID, typename... MessageT>
BroadcastChannel<ChannelID, MessageT...>::BroadcastChannel()
{
}

template <typename ChannelID, typename... MessageT>
BroadcastChannel<ChannelID, MessageT...>::~BroadcastChannel()
{
    reset();
}

template <typename ChannelID, typename... MessageT>
void BroadcastChannel<ChannelID, MessageT...>::addReceiver(
    ChannelBinding<ChannelID, MessageT...> *receiver)
{
    ASSERT(std::find(mReceivers.begin(), mReceivers.end(), receiver) == mReceivers.end());
    mReceivers.push_back(receiver);
}

template <typename ChannelID, typename... MessageT>
void BroadcastChannel<ChannelID, MessageT...>::removeReceiver(
    ChannelBinding<ChannelID, MessageT...> *receiver)
{
    auto iter = std::find(mReceivers.begin(), mReceivers.end(), receiver);
    ASSERT(iter != mReceivers.end());
    mReceivers.erase(iter);
}

template <typename ChannelID, typename... MessageT>
void BroadcastChannel<ChannelID, MessageT...>::signal(MessageT... message) const
{
    if (mReceivers.empty())
        return;

    for (const auto *receiver : mReceivers)
    {
        receiver->signal(message...);
    }
}

template <typename ChannelID, typename... MessageT>
void BroadcastChannel<ChannelID, MessageT...>::reset()
{
    for (auto receiver : mReceivers)
    {
        receiver->onChannelClosed();
    }
    mReceivers.clear();
}

template <typename ChannelID, typename... MessageT>
bool BroadcastChannel<ChannelID, MessageT...>::empty() const
{
    return mReceivers.empty();
}

// The dependent class keeps bindings to the host's BroadcastChannel.
template <typename ChannelID = uint32_t, typename... MessageT>
class ChannelBinding final
{
  public:
    ChannelBinding(SignalReceiver<ChannelID, MessageT...> *receiver, ChannelID channelID);
    ~ChannelBinding();
    ChannelBinding(const ChannelBinding &other) = default;
    ChannelBinding &operator=(const ChannelBinding &other) = default;

    void bind(BroadcastChannel<ChannelID, MessageT...> *channel);
    void reset();
    void signal(MessageT... message) const;
    void onChannelClosed();

  private:
    BroadcastChannel<ChannelID, MessageT...> *mChannel;
    SignalReceiver<ChannelID, MessageT...> *mReceiver;
    ChannelID mChannelID;
};

template <typename ChannelID, typename... MessageT>
ChannelBinding<ChannelID, MessageT...>::ChannelBinding(
    SignalReceiver<ChannelID, MessageT...> *receiver,
    ChannelID channelID)
    : mChannel(nullptr), mReceiver(receiver), mChannelID(channelID)
{
    ASSERT(receiver);
}

template <typename ChannelID, typename... MessageT>
ChannelBinding<ChannelID, MessageT...>::~ChannelBinding()
{
    reset();
}

template <typename ChannelID, typename... MessageT>
void ChannelBinding<ChannelID, MessageT...>::bind(BroadcastChannel<ChannelID, MessageT...> *channel)
{
    ASSERT(mReceiver);
    if (mChannel)
    {
        mChannel->removeReceiver(this);
    }

    mChannel = channel;

    if (mChannel)
    {
        mChannel->addReceiver(this);
    }
}

template <typename ChannelID, typename... MessageT>
void ChannelBinding<ChannelID, MessageT...>::reset()
{
    bind(nullptr);
}

template <typename ChannelID, typename... MessageT>
void ChannelBinding<ChannelID, MessageT...>::signal(MessageT... message) const
{
    mReceiver->signal(mChannelID, message...);
}

template <typename ChannelID, typename... MessageT>
void ChannelBinding<ChannelID, MessageT...>::onChannelClosed()
{
    mChannel = nullptr;
}

}  // namespace angle

#endif  // LIBANGLE_SIGNAL_UTILS_H_
