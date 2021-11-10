# RTC event log

<?% config.freshness.owner = 'terelius' %?>
<?% config.freshness.reviewed = '2021-06-02' %?>

## Overview

RTC event logs can be enabled to capture in-depth inpformation about sent and
received packets and the internal state of some WebRTC components. The logs are
useful to understand network behavior and to debug issues around connectivity,
bandwidth estimation and audio jitter buffers.

The contents include:

*   Sent and received RTP headers
*   Full RTCP feedback
*   ICE candidates, pings and responses
*   Bandwidth estimator events, including loss-based estimate, delay-based
    estimate, probe results and ALR state
*   Audio network adaptation settings
*   Audio playout events

## Binary wire format

No guarantees are made on the wire format, and the format may change without
prior notice. To maintain compatibility with past and future formats, analysis
tools should be built on top of the provided
[rtc_event_log_parser.h](https://source.chromium.org/chromium/chromium/src/+/main:third_party/webrtc/logging/rtc_event_log/rtc_event_log_parser.h)

In particular, an analysis tool should *not* read the log as a protobuf.

## Visualization

Since the logs contain a substantial amount of data, it is usually convenient to
get an overview by visualizing them as a set of plots. Use the command:

```
out/Default/event_log_visualizer /path/to/log_file | python
```

This visualization requires matplotlib to be installed. The tool is capable of
producing a substantial number of plots, of which only a handful are generated
by default. You can select which plots are generated though the `--plot=`
command line argument. For example, the command

```
out/Default/event_log_visualizer \
  --plot=incoming_packet_sizes,incoming_stream_bitrate \
  /path/to/log_file | python
```

plots the sizes of incoming packets and the bitrate per incoming stream.

You can get a full list of options for the `--plot` argument through

```
out/Default/event_log_visualizer --list_plots /path/to/log_file
```

You can also synchronize the x-axis between all plots (so zooming or
panning in one plot affects all of them), by adding the command line
argument `--shared_xaxis`.


## Viewing the raw log contents as text

If you know which format version the log file uses, you can view the raw
contents as text. For version 1, you can use the command

```
out/Default/protoc --decode webrtc.rtclog.EventStream \
  ./logging/rtc_event_log/rtc_event_log.proto < /path/to/log_file
```

Similarly, you can use

```
out/Default/protoc --decode webrtc.rtclog2.EventStream \
  ./logging/rtc_event_log/rtc_event_log2.proto < /path/to/log_file
```

for logs that use version 2. However, note that not all of the contents will be
human readable. Some fields are based on the raw RTP format or may be encoded as
deltas relative to previous fields. Such fields will be printed as a list of
bytes.
