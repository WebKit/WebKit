#  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
#
#  Use of this source code is governed by a BSD-style license
#  that can be found in the LICENSE file in the root of the source
#  tree. An additional intellectual property rights grant can be found
#  in the file PATENTS.  All contributing project authors may
#  be found in the AUTHORS file in the root of the source tree.
"""Displays statistics and plots graphs from RTC protobuf dump."""

from __future__ import division
from __future__ import print_function

import collections
import optparse
import os
import sys

import matplotlib.pyplot as plt
import numpy

import misc
import pb_parse


class RTPStatistics(object):
    """Has methods for calculating and plotting RTP stream statistics."""

    BANDWIDTH_SMOOTHING_WINDOW_SIZE = 10
    PLOT_RESOLUTION_MS = 50

    def __init__(self, data_points):
        """Initializes object with data_points and computes simple statistics.

    Computes percentages of number of packets and packet sizes by
    SSRC.

    Args:
        data_points: list of pb_parse.DataPoints on which statistics are
            calculated.

    """

        self.data_points = data_points
        self.ssrc_frequencies = misc.NormalizeCounter(
            collections.Counter([pt.ssrc for pt in self.data_points]))
        self.ssrc_size_table = misc.SsrcNormalizedSizeTable(self.data_points)
        self.bandwidth_kbps = None
        self.smooth_bw_kbps = None

    def PrintHeaderStatistics(self):
        print("{:>6}{:>14}{:>14}{:>6}{:>6}{:>3}{:>11}".format(
            "SeqNo", "TimeStamp", "SendTime", "Size", "PT", "M", "SSRC"))
        for point in self.data_points:
            print("{:>6}{:>14}{:>14}{:>6}{:>6}{:>3}{:>11}".format(
                point.sequence_number, point.timestamp,
                int(point.arrival_timestamp_ms), point.size,
                point.payload_type, point.marker_bit,
                "0x{:x}".format(point.ssrc)))

    def PrintSsrcInfo(self, ssrc_id, ssrc):
        """Prints packet and size statistics for a given SSRC.

    Args:
        ssrc_id: textual identifier of SSRC printed beside statistics for it.
        ssrc: SSRC by which to filter data and display statistics
    """
        filtered_ssrc = [
            point for point in self.data_points if point.ssrc == ssrc
        ]
        payloads = misc.NormalizeCounter(
            collections.Counter(
                [point.payload_type for point in filtered_ssrc]))

        payload_info = "payload type(s): {}".format(", ".join(
            str(payload) for payload in payloads))
        print("{} 0x{:x} {}, {:.2f}% packets, {:.2f}% data".format(
            ssrc_id, ssrc, payload_info, self.ssrc_frequencies[ssrc] * 100,
            self.ssrc_size_table[ssrc] * 100))
        print("  packet sizes:")
        (bin_counts,
         bin_bounds) = numpy.histogram([point.size for point in filtered_ssrc],
                                       bins=5,
                                       density=False)
        bin_proportions = bin_counts / sum(bin_counts)
        print("\n".join([
            " {:.1f} - {:.1f}: {:.2f}%".format(bin_bounds[i],
                                               bin_bounds[i + 1],
                                               bin_proportions[i] * 100)
            for i in range(len(bin_proportions))
        ]))

    def ChooseSsrc(self):
        """Queries user for SSRC."""

        if len(self.ssrc_frequencies) == 1:
            chosen_ssrc = self.ssrc_frequencies.keys()[0]
            self.PrintSsrcInfo("", chosen_ssrc)
            return chosen_ssrc

        ssrc_is_incoming = misc.SsrcDirections(self.data_points)
        incoming = [
            ssrc for ssrc in ssrc_is_incoming if ssrc_is_incoming[ssrc]
        ]
        outgoing = [
            ssrc for ssrc in ssrc_is_incoming if not ssrc_is_incoming[ssrc]
        ]

        print("\nIncoming:\n")
        for (i, ssrc) in enumerate(incoming):
            self.PrintSsrcInfo(i, ssrc)

        print("\nOutgoing:\n")
        for (i, ssrc) in enumerate(outgoing):
            self.PrintSsrcInfo(i + len(incoming), ssrc)

        while True:
            chosen_index = int(misc.get_input("choose one> "))
            if 0 <= chosen_index < len(self.ssrc_frequencies):
                return (incoming + outgoing)[chosen_index]
            else:
                print("Invalid index!")

    def FilterSsrc(self, chosen_ssrc):
        """Filters and wraps data points.

    Removes data points with `ssrc != chosen_ssrc`. Unwraps sequence
    numbers and timestamps for the chosen selection.
    """
        self.data_points = [
            point for point in self.data_points if point.ssrc == chosen_ssrc
        ]
        unwrapped_sequence_numbers = misc.Unwrap(
            [point.sequence_number for point in self.data_points], 2**16 - 1)
        for (data_point, sequence_number) in zip(self.data_points,
                                                 unwrapped_sequence_numbers):
            data_point.sequence_number = sequence_number

        unwrapped_timestamps = misc.Unwrap(
            [point.timestamp for point in self.data_points], 2**32 - 1)

        for (data_point, timestamp) in zip(self.data_points,
                                           unwrapped_timestamps):
            data_point.timestamp = timestamp

    def PrintSequenceNumberStatistics(self):
        seq_no_set = set(point.sequence_number for point in self.data_points)
        missing_sequence_numbers = max(seq_no_set) - min(seq_no_set) + (
            1 - len(seq_no_set))
        print("Missing sequence numbers: {} out of {}  ({:.2f}%)".format(
            missing_sequence_numbers, len(seq_no_set),
            100 * missing_sequence_numbers / len(seq_no_set)))
        print("Duplicated packets: {}".format(
            len(self.data_points) - len(seq_no_set)))
        print("Reordered packets: {}".format(
            misc.CountReordered(
                [point.sequence_number for point in self.data_points])))

    def EstimateFrequency(self, always_query_sample_rate):
        """Estimates frequency and updates data.

    Guesses the most probable frequency by looking at changes in
    timestamps (RFC 3550 section 5.1), calculates clock drifts and
    sending time of packets. Updates `self.data_points` with changes
    in delay and send time.
    """
        delta_timestamp = (self.data_points[-1].timestamp -
                           self.data_points[0].timestamp)
        delta_arr_timestamp = float(
            (self.data_points[-1].arrival_timestamp_ms -
             self.data_points[0].arrival_timestamp_ms))
        freq_est = delta_timestamp / delta_arr_timestamp

        freq_vec = [8, 16, 32, 48, 90]
        freq = None
        for f in freq_vec:
            if abs((freq_est - f) / f) < 0.05:
                freq = f

        print("Estimated frequency: {:.3f}kHz".format(freq_est))
        if freq is None or always_query_sample_rate:
            if not always_query_sample_rate:
                print("Frequency could not be guessed.", end=" ")
            freq = int(misc.get_input("Input frequency (in kHz)> "))
        else:
            print("Guessed frequency: {}kHz".format(freq))

        for point in self.data_points:
            point.real_send_time_ms = (point.timestamp -
                                       self.data_points[0].timestamp) / freq
            point.delay = point.arrival_timestamp_ms - point.real_send_time_ms

    def PrintDurationStatistics(self):
        """Prints delay, clock drift and bitrate statistics."""

        min_delay = min(point.delay for point in self.data_points)

        for point in self.data_points:
            point.absdelay = point.delay - min_delay

        stream_duration_sender = self.data_points[-1].real_send_time_ms / 1000
        print("Stream duration at sender: {:.1f} seconds".format(
            stream_duration_sender))

        arrival_timestamps_ms = [
            point.arrival_timestamp_ms for point in self.data_points
        ]
        stream_duration_receiver = (max(arrival_timestamps_ms) -
                                    min(arrival_timestamps_ms)) / 1000
        print("Stream duration at receiver: {:.1f} seconds".format(
            stream_duration_receiver))

        print("Clock drift: {:.2f}%".format(
            100 * (stream_duration_receiver / stream_duration_sender - 1)))

        total_size = sum(point.size for point in self.data_points) * 8 / 1000
        print("Send average bitrate: {:.2f} kbps".format(
            total_size / stream_duration_sender))

        print("Receive average bitrate: {:.2f} kbps".format(
            total_size / stream_duration_receiver))

    def RemoveReordered(self):
        last = self.data_points[0]
        data_points_ordered = [last]
        for point in self.data_points[1:]:
            if point.sequence_number > last.sequence_number and (
                    point.real_send_time_ms > last.real_send_time_ms):
                data_points_ordered.append(point)
                last = point
        self.data_points = data_points_ordered

    def ComputeBandwidth(self):
        """Computes bandwidth averaged over several consecutive packets.

    The number of consecutive packets used in the average is
    BANDWIDTH_SMOOTHING_WINDOW_SIZE. Averaging is done with
    numpy.correlate.
    """
        start_ms = self.data_points[0].real_send_time_ms
        stop_ms = self.data_points[-1].real_send_time_ms
        (self.bandwidth_kbps, _) = numpy.histogram(
            [point.real_send_time_ms for point in self.data_points],
            bins=numpy.arange(start_ms, stop_ms,
                              RTPStatistics.PLOT_RESOLUTION_MS),
            weights=[
                point.size * 8 / RTPStatistics.PLOT_RESOLUTION_MS
                for point in self.data_points
            ])
        correlate_filter = (
            numpy.ones(RTPStatistics.BANDWIDTH_SMOOTHING_WINDOW_SIZE) /
            RTPStatistics.BANDWIDTH_SMOOTHING_WINDOW_SIZE)
        self.smooth_bw_kbps = numpy.correlate(self.bandwidth_kbps,
                                              correlate_filter)

    def PlotStatistics(self):
        """Plots changes in delay and average bandwidth."""

        start_ms = self.data_points[0].real_send_time_ms
        stop_ms = self.data_points[-1].real_send_time_ms
        time_axis = numpy.arange(start_ms / 1000, stop_ms / 1000,
                                 RTPStatistics.PLOT_RESOLUTION_MS / 1000)

        delay = CalculateDelay(start_ms, stop_ms,
                               RTPStatistics.PLOT_RESOLUTION_MS,
                               self.data_points)

        plt.figure(1)
        plt.plot(time_axis, delay[:len(time_axis)])
        plt.xlabel("Send time [s]")
        plt.ylabel("Relative transport delay [ms]")

        plt.figure(2)
        plt.plot(time_axis[:len(self.smooth_bw_kbps)], self.smooth_bw_kbps)
        plt.xlabel("Send time [s]")
        plt.ylabel("Bandwidth [kbps]")

        plt.show()


def CalculateDelay(start, stop, step, points):
    """Quantizes the time coordinates for the delay.

  Quantizes points by rounding the timestamps downwards to the nearest
  point in the time sequence start, start+step, start+2*step... Takes
  the average of the delays of points rounded to the same. Returns
  masked array, in which time points with no value are masked.

  """
    grouped_delays = [[] for _ in numpy.arange(start, stop + step, step)]
    rounded_value_index = lambda x: int((x - start) / step)
    for point in points:
        grouped_delays[rounded_value_index(point.real_send_time_ms)].append(
            point.absdelay)
    regularized_delays = [
        numpy.average(arr) if arr else -1 for arr in grouped_delays
    ]
    return numpy.ma.masked_values(regularized_delays, -1)


def main():
    usage = "Usage: %prog [options] <filename of rtc event log>"
    parser = optparse.OptionParser(usage=usage)
    parser.add_option(
        "--dump_header_to_stdout",
        default=False,
        action="store_true",
        help="print header info to stdout; similar to rtp_analyze")
    parser.add_option("--query_sample_rate",
                      default=False,
                      action="store_true",
                      help="always query user for real sample rate")

    parser.add_option("--working_directory",
                      default=None,
                      action="store",
                      help="directory in which to search for relative paths")

    (options, args) = parser.parse_args()

    if len(args) < 1:
        parser.print_help()
        sys.exit(0)

    input_file = args[0]

    if options.working_directory and not os.path.isabs(input_file):
        input_file = os.path.join(options.working_directory, input_file)

    data_points = pb_parse.ParseProtobuf(input_file)
    rtp_stats = RTPStatistics(data_points)

    if options.dump_header_to_stdout:
        print("Printing header info to stdout.", file=sys.stderr)
        rtp_stats.PrintHeaderStatistics()
        sys.exit(0)

    chosen_ssrc = rtp_stats.ChooseSsrc()
    print("Chosen SSRC: 0X{:X}".format(chosen_ssrc))

    rtp_stats.FilterSsrc(chosen_ssrc)

    print("Statistics:")
    rtp_stats.PrintSequenceNumberStatistics()
    rtp_stats.EstimateFrequency(options.query_sample_rate)
    rtp_stats.PrintDurationStatistics()
    rtp_stats.RemoveReordered()
    rtp_stats.ComputeBandwidth()
    rtp_stats.PlotStatistics()


if __name__ == "__main__":
    main()
