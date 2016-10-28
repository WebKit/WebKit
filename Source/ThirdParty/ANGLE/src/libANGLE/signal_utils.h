//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// signal_utils:
//   Helper classes for tracking dependent state changes between objects.
//   These changes are signaled to the dependent class via channels.

#ifndef LIBANGLE_SIGNAL_UTILS_H_
#define LIBANGLE_SIGNAL_UTILS_H_

#include <set>

#include "common/angleutils.h"

namespace angle
{

// Message token passed to the receiver;
using SignalToken = uint32_t;

// Interface that the depending class inherits from.
class SignalReceiver
{
  public:
    virtual ~SignalReceiver() = default;
    virtual void signal(SignalToken token) = 0;
};

class ChannelBinding;

// The host class owns the channel. It uses the channel to fire signals to the receiver.
class BroadcastChannel final : NonCopyable
{
  public:
    BroadcastChannel();
    ~BroadcastChannel();

    void signal() const;
    void reset();

  private:
    // Only the ChannelBinding class should add or remove receivers.
    friend class ChannelBinding;
    void addReceiver(ChannelBinding *receiver);
    void removeReceiver(ChannelBinding *receiver);

    std::vector<ChannelBinding *> mReceivers;
};

// The dependent class keeps bindings to the host's BroadcastChannel.
class ChannelBinding final
{
  public:
    ChannelBinding(SignalReceiver *receiver, SignalToken token);
    ~ChannelBinding();
    ChannelBinding(const ChannelBinding &other) = default;
    ChannelBinding &operator=(const ChannelBinding &other) = default;

    void bind(BroadcastChannel *channel);
    void reset();
    void signal() const;
    void onChannelClosed();

  private:
    BroadcastChannel *mChannel;
    SignalReceiver *mReceiver;
    SignalToken mToken;
};

}  // namespace angle

#endif  // LIBANGLE_SIGNAL_UTILS_H_
