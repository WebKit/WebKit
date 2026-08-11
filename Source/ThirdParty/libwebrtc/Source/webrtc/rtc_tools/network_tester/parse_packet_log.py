#  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.

#  To run this script please copy "out/<build_name>/pyproto/webrtc/rtc_tools/
#  network_tester/network_tester_packet_pb2.py" next to this script.
#  The you can run this script with:
#  "python parse_packet_log.py -f packet_log.dat"
#  for more information call:
#  "python parse_packet_log.py --help"

from optparse import OptionParser
import struct

import matplotlib.pyplot as plt

import network_tester_packet_pb2


def GetSize(file_to_parse):
    data = file_to_parse.read(1)
    if data == '':
        return 0
    return struct.unpack('<b', data)[0]


def ParsePacketLog(packet_log_file_to_parse):
    packets = []
    with open(packet_log_file_to_parse, 'rb') as file_to_parse:
        while True:
            size = GetSize(file_to_parse)
            if size == 0:
                break
            try:
                packet = network_tester_packet_pb2.NetworkTesterPacket()
                packet.ParseFromString(file_to_parse.read(size))
                packets.append(packet)
            except IOError:
                break
    return packets


def GetTimeAxis(packets):
    first_arrival_time = packets[0].arrival_timestamp
    return [(packet.arrival_timestamp - first_arrival_time) / 1000000.0
            for packet in packets]


def CreateSendTimeDiffPlot(packets, plot):
    first_send_time_diff = (packets[0].arrival_timestamp -
                            packets[0].send_timestamp)
    y = [(packet.arrival_timestamp - packet.send_timestamp) -
         first_send_time_diff for packet in packets]
    plot.grid(True)
    plot.set_title("SendTime difference [us]")
    plot.plot(GetTimeAxis(packets), y)


class MovingAverageBitrate(object):
    def __init__(self):
        self.packet_window = []
        self.window_time = 1000000
        self.bytes = 0
        self.latest_packet_time = 0
        self.send_interval = 0

    def RemoveOldPackets(self):
        for packet in self.packet_window:
            if (self.latest_packet_time - packet.arrival_timestamp >
                    self.window_time):
                self.bytes = self.bytes - packet.packet_size
                self.packet_window.remove(packet)

    def AddPacket(self, packet):
        """This functions returns bits / second"""
        self.send_interval = packet.arrival_timestamp - self.latest_packet_time
        self.latest_packet_time = packet.arrival_timestamp
        self.RemoveOldPackets()
        self.packet_window.append(packet)
        self.bytes = self.bytes + packet.packet_size
        return self.bytes * 8


def CreateReceiveBiratePlot(packets, plot):
    bitrate = MovingAverageBitrate()
    y = [bitrate.AddPacket(packet) for packet in packets]
    plot.grid(True)
    plot.set_title("Receive birate [bps]")
    plot.plot(GetTimeAxis(packets), y)


def CreatePacketlossPlot(packets, plot):
    packets_look_up = {}
    first_sequence_number = packets[0].sequence_number
    last_sequence_number = packets[-1].sequence_number
    for packet in packets:
        packets_look_up[packet.sequence_number] = packet
    y = []
    x = []
    first_arrival_time = 0
    last_arrival_time = 0
    last_arrival_time_diff = 0
    for sequence_number in range(first_sequence_number,
                                 last_sequence_number + 1):
        if sequence_number in packets_look_up:
            y.append(0)
            if first_arrival_time == 0:
                first_arrival_time = packets_look_up[
                    sequence_number].arrival_timestamp
            x_time = (packets_look_up[sequence_number].arrival_timestamp -
                      first_arrival_time)
            if last_arrival_time != 0:
                last_arrival_time_diff = x_time - last_arrival_time
            last_arrival_time = x_time
            x.append(x_time / 1000000.0)
        else:
            if last_arrival_time != 0 and last_arrival_time_diff != 0:
                x.append(
                    (last_arrival_time + last_arrival_time_diff) / 1000000.0)
                y.append(1)
    plot.grid(True)
    plot.set_title("Lost packets [0/1]")
    plot.plot(x, y)


def main():
    parser = OptionParser()
    parser.add_option("-f",
                      "--packet_log_file",
                      dest="packet_log_file",
                      help="packet_log file to parse")

    options = parser.parse_args()[0]

    packets = ParsePacketLog(options.packet_log_file)
    f, plots = plt.subplots(3, sharex=True)
    plt.xlabel('time [sec]')
    CreateSendTimeDiffPlot(packets, plots[0])
    CreateReceiveBiratePlot(packets, plots[1])
    CreatePacketlossPlot(packets, plots[2])
    f.subplots_adjust(hspace=0.3)
    plt.show()


if __name__ == "__main__":
    main()
