# RTP log analyzer
This file describes how to set up and use the RTP log analyzer.

## Build

```shell
$ autoninja -C out/my_build webrtc:rtp_analyzer
```

## Usage

```shell
./out/my_build/rtp_analyzer.sh [options] /path/to/rtc_event.log
```

where `/path/to/rtc_event.log` is a recorded RTC event log, which is stored in
protobuf format. Such logs are generated in multiple ways, e.g. by Chrome
through the chrome://webrtc-internals page.

Use `--help` for the options.

The script requires Python (2.7 or 3+) and it has the following dependencies:
Dependencies (available on pip):
- matplotlib (http://matplotlib.org/)
- numpy (http://www.numpy.org/)
- protobuf (https://pypi.org/project/protobuf/)
