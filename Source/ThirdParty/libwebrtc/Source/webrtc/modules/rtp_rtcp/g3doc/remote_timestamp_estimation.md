# NTP Time Estimation based on RRTR/DLRR

## Overview
The estimation of NTP time offset between a local system and a remote system using RTCP XR (Extended Reports) RRTR and DLRR blocks involves two main steps: Round-Trip Time (RTT) calculation and NTP Offset estimation.

## 1. RTT Calculation using RRTR/DLRR
The flow is as follows (RFC 3611):
1.  **Local Receiver** sends an **RRTR** (Receiver Reference Time Report) block containing its current NTP timestamp ($T_1$).
2.  **Remote Sender** receives the RRTR at $T_2$.
3.  **Remote Sender** prepares a **DLRR** (Delay since Last Receiver Report) block. It includes:
    *   `last_rr`: The middle 32 bits of the $T_1$ timestamp from the received RRTR.
    *   `delay_since_last_rr`: The time elapsed between receiving the RRTR and sending the DLRR ($D = T_3 - T_2$).
4.  **Local Receiver** receives the DLRR at $T_4$.

The **RTT** is calculated in `RTCPReceiver::HandleXrDlrrReportBlock` (in `modules/rtp_rtcp/source/rtcp_receiver.cc`) as:
$$RTT = (T_4 - T_1) - 	ext{delay\_since\_last\_rr}$$
In code, this uses compact NTP representation (1/2^16 seconds resolution).

## 2. NTP Offset Estimation
Once the RTT is known, it is used along with Sender Reports (SR) to estimate the NTP offset between the remote and local clocks. This logic is implemented in `RemoteNtpTimeEstimator::UpdateRtcpTimestamp` (in `modules/rtp_rtcp/source/remote_ntp_time_estimator.cc`).

1.  **SR Data**: The Sender Report provides the remote NTP time ($T_{	ext{remote\_send}}$) and the corresponding RTP timestamp.
2.  **Delivery Time Estimation**: Assuming network symmetry, the one-way delivery time is estimated as:
    $$DeliverTime = RTT / 2$$
3.  **Receiver Arrival Time**: The local system records the NTP time when the SR was received ($T_{	ext{local\_arrival}}$).
4.  **Clock Offset Calculation**:
    $$	ext{RemoteToLocalOffset} = (T_{	ext{local\_arrival}} - T_{	ext{remote\_send}}) - DeliverTime$$

This offset is inserted into a smoothing filter (`ntp_clocks_offset_estimator_`).

## 3. Usage
The estimated offset is used by:
*   `RemoteNtpTimeEstimator::EstimateNtp`: To convert RTP timestamps to receiver-local NTP time.
*   `CaptureClockOffsetUpdater`: To adjust `Absolute Capture Time` extensions in RTP packets, allowing the receiver to know the exact capture time in its own NTP clock.

## Relevant Code Locations
*   `modules/rtp_rtcp/source/rtcp_receiver.cc`: `HandleXrDlrrReportBlock` - RTT calculation logic.
*   `modules/rtp_rtcp/source/remote_ntp_time_estimator.cc`: `UpdateRtcpTimestamp` - NTP offset estimation.
*   `modules/rtp_rtcp/source/ntp_time_util.h`: NTP conversion helpers like `CompactNtp`, `ToNtpUnits`, and `CompactNtpRttToTimeDelta`.
