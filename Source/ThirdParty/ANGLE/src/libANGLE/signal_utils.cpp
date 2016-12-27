//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils:
//   Helper classes for tracking dependent state changes between objects.
//   These changes are signaled to the dependent class via channels.

#include "libANGLE/signal_utils.h"

#include <algorithm>

#include "common/debug.h"

namespace angle
{

BroadcastChannel::BroadcastChannel()
{
}

BroadcastChannel::~BroadcastChannel()
{
    reset();
}

void BroadcastChannel::addReceiver(ChannelBinding *receiver)
{
    ASSERT(std::find(mReceivers.begin(), mReceivers.end(), receiver) == mReceivers.end());
    mReceivers.push_back(receiver);
}

void BroadcastChannel::removeReceiver(ChannelBinding *receiver)
{
    auto iter = std::find(mReceivers.begin(), mReceivers.end(), receiver);
    ASSERT(iter != mReceivers.end());
    mReceivers.erase(iter);
}

void BroadcastChannel::signal() const
{
    if (mReceivers.empty())
        return;

    for (const auto *receiver : mReceivers)
    {
        receiver->signal();
    }
}

void BroadcastChannel::reset()
{
    for (auto receiver : mReceivers)
    {
        receiver->onChannelClosed();
    }
    mReceivers.clear();
}

ChannelBinding::ChannelBinding(SignalReceiver *receiver, SignalToken token)
    : mChannel(nullptr), mReceiver(receiver), mToken(token)
{
    ASSERT(receiver);
}

ChannelBinding::~ChannelBinding()
{
    reset();
}

void ChannelBinding::bind(BroadcastChannel *channel)
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

void ChannelBinding::reset()
{
    bind(nullptr);
}

void ChannelBinding::signal() const
{
    mReceiver->signal(mToken);
}

void ChannelBinding::onChannelClosed()
{
    mChannel = nullptr;
}

}  // namespace angle
