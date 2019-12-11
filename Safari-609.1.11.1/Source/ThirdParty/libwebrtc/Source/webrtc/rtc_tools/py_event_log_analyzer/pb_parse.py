#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

"""Parses protobuf RTC dumps."""

from __future__ import division
import struct
import pyproto.logging.rtc_event_log.rtc_event_log_pb2 as rtc_pb


class DataPoint(object):
  """Simple container class for RTP events."""

  def __init__(self, rtp_header_str, packet_size,
               arrival_timestamp_us, incoming):
    """Builds a data point by parsing an RTP header, size and arrival time.

    RTP header structure is defined in RFC 3550 section 5.1.
    """
    self.size = packet_size
    self.arrival_timestamp_ms = arrival_timestamp_us / 1000
    self.incoming = incoming
    header = struct.unpack_from("!HHII", rtp_header_str, 0)
    (first2header_bytes, self.sequence_number, self.timestamp,
     self.ssrc) = header
    self.payload_type = first2header_bytes & 0b01111111
    self.marker_bit = (first2header_bytes & 0b10000000) >> 7


def ParseProtobuf(file_path):
  """Parses RTC event log from protobuf file.

  Args:
       file_path: path to protobuf file of RTC event stream

  Returns:
    all RTP packet events from the event stream as a list of DataPoints
  """
  event_stream = rtc_pb.EventStream()
  with open(file_path, "rb") as f:
    event_stream.ParseFromString(f.read())

  return [DataPoint(event.rtp_packet.header,
                    event.rtp_packet.packet_length,
                    event.timestamp_us, event.rtp_packet.incoming)
          for event in event_stream.stream
          if event.HasField("rtp_packet")]
