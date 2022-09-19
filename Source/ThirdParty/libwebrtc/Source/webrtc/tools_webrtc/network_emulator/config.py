#!/usr/bin/env vpython3

#  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
"""Configuration class for network emulation."""


class ConnectionConfig:
  """Configuration containing the characteristics of a network connection."""

  def __init__(self, num, name, receive_bw_kbps, send_bw_kbps, delay_ms,
               packet_loss_percent, queue_slots):
    self.num = num
    self.name = name
    self.receive_bw_kbps = receive_bw_kbps
    self.send_bw_kbps = send_bw_kbps
    self.delay_ms = delay_ms
    self.packet_loss_percent = packet_loss_percent
    self.queue_slots = queue_slots

  def __str__(self):
    """String representing the configuration.

    Returns:
        A string formatted and padded like this example:
    12 Name                       375 kbps   375 kbps   10   145 ms  0.1 %
    """
    left_aligned_name = self.name.ljust(24, ' ')
    return '%2s %24s %5s kbps %5s kbps %4s %5s ms  %3s %%' % (
        self.num, left_aligned_name, self.receive_bw_kbps, self.send_bw_kbps,
        self.queue_slots, self.delay_ms, self.packet_loss_percent)
