# `video/timing/simulator`

This directory contains classes related to RtcEventLog-based simulation of video
timing and video jitter buffering.

The simulator uses recorded RtcEventLog files, which contain time-stamped events
from a WebRTC session, to reconstruct and simulate the timing aspects of
received video streams. This allows for:

* Replaying Scenarios: Analyzing how different timing algorithms or network
  conditions would affect video performance by replaying logs from real-world
  sessions.
* Algorithm Evaluation: Testing and comparing different timing algorithms
  offline using realistic event sequences.
* Debugging: Understanding timing-related issues observed in production by
  stepping through the events as they occurred.

The simulation focuses on aspects like frame arrival times, decoding times,
rendering times, and the overall flow of video frames through a simulated
pipeline, driven by the events logged in the RtcEventLog.
