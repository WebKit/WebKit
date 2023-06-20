/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "modules/audio_coding/test/Channel.h"

#include <iostream>

#include "rtc_base/strings/string_builder.h"
#include "rtc_base/time_utils.h"

namespace webrtc {

int32_t Channel::SendData(AudioFrameType frameType,
                          uint8_t payloadType,
                          uint32_t timeStamp,
                          const uint8_t* payloadData,
                          size_t payloadSize,
                          int64_t absolute_capture_timestamp_ms) {
  RTPHeader rtp_header;
  int32_t status;
  size_t payloadDataSize = payloadSize;

  rtp_header.markerBit = false;
  rtp_header.ssrc = 0;
  rtp_header.sequenceNumber =
      (external_sequence_number_ < 0)
          ? _seqNo++
          : static_cast<uint16_t>(external_sequence_number_);
  rtp_header.payloadType = payloadType;
  rtp_header.timestamp = (external_send_timestamp_ < 0)
                             ? timeStamp
                             : static_cast<uint32_t>(external_send_timestamp_);

  if (frameType == AudioFrameType::kEmptyFrame) {
    // When frame is empty, we should not transmit it. The frame size of the
    // next non-empty frame will be based on the previous frame size.
    _useLastFrameSize = _lastFrameSizeSample > 0;
    return 0;
  }

  memcpy(_payloadData, payloadData, payloadDataSize);
  if (_isStereo) {
    if (_leftChannel) {
      _rtp_header = rtp_header;
      _leftChannel = false;
    } else {
      rtp_header = _rtp_header;
      _leftChannel = true;
    }
  }

  _channelCritSect.Lock();
  if (_saveBitStream) {
    // fwrite(payloadData, sizeof(uint8_t), payloadSize, _bitStreamFile);
  }

  if (!_isStereo) {
    CalcStatistics(rtp_header, payloadSize);
  }
  _useLastFrameSize = false;
  _lastInTimestamp = timeStamp;
  _totalBytes += payloadDataSize;
  _channelCritSect.Unlock();

  if (_useFECTestWithPacketLoss) {
    _packetLoss += 1;
    if (_packetLoss == 3) {
      _packetLoss = 0;
      return 0;
    }
  }

  if (num_packets_to_drop_ > 0) {
    num_packets_to_drop_--;
    return 0;
  }

  status = _receiverACM->InsertPacket(
      rtp_header, rtc::ArrayView<const uint8_t>(_payloadData, payloadDataSize));

  return status;
}

// TODO(turajs): rewite this method.
void Channel::CalcStatistics(const RTPHeader& rtp_header, size_t payloadSize) {
  int n;
  if ((rtp_header.payloadType != _lastPayloadType) &&
      (_lastPayloadType != -1)) {
    // payload-type is changed.
    // we have to terminate the calculations on the previous payload type
    // we ignore the last packet in that payload type just to make things
    // easier.
    for (n = 0; n < MAX_NUM_PAYLOADS; n++) {
      if (_lastPayloadType == _payloadStats[n].payloadType) {
        _payloadStats[n].newPacket = true;
        break;
      }
    }
  }
  _lastPayloadType = rtp_header.payloadType;

  bool newPayload = true;
  ACMTestPayloadStats* currentPayloadStr = NULL;
  for (n = 0; n < MAX_NUM_PAYLOADS; n++) {
    if (rtp_header.payloadType == _payloadStats[n].payloadType) {
      newPayload = false;
      currentPayloadStr = &_payloadStats[n];
      break;
    }
  }

  if (!newPayload) {
    if (!currentPayloadStr->newPacket) {
      if (!_useLastFrameSize) {
        _lastFrameSizeSample =
            (uint32_t)((uint32_t)rtp_header.timestamp -
                       (uint32_t)currentPayloadStr->lastTimestamp);
      }
      RTC_DCHECK_GT(_lastFrameSizeSample, 0);
      int k = 0;
      for (; k < MAX_NUM_FRAMESIZES; ++k) {
        if ((currentPayloadStr->frameSizeStats[k].frameSizeSample ==
             _lastFrameSizeSample) ||
            (currentPayloadStr->frameSizeStats[k].frameSizeSample == 0)) {
          break;
        }
      }
      if (k == MAX_NUM_FRAMESIZES) {
        // New frame size found but no space to count statistics on it. Skip it.
        printf("No memory to store statistics for payload %d : frame size %d\n",
               _lastPayloadType, _lastFrameSizeSample);
        return;
      }
      ACMTestFrameSizeStats* currentFrameSizeStats =
          &(currentPayloadStr->frameSizeStats[k]);
      currentFrameSizeStats->frameSizeSample = (int16_t)_lastFrameSizeSample;

      // increment the number of encoded samples.
      currentFrameSizeStats->totalEncodedSamples += _lastFrameSizeSample;
      // increment the number of recveived packets
      currentFrameSizeStats->numPackets++;
      // increment the total number of bytes (this is based on
      // the previous payload we don't know the frame-size of
      // the current payload.
      currentFrameSizeStats->totalPayloadLenByte +=
          currentPayloadStr->lastPayloadLenByte;
      // store the maximum payload-size (this is based on
      // the previous payload we don't know the frame-size of
      // the current payload.
      if (currentFrameSizeStats->maxPayloadLen <
          currentPayloadStr->lastPayloadLenByte) {
        currentFrameSizeStats->maxPayloadLen =
            currentPayloadStr->lastPayloadLenByte;
      }
      // store the current values for the next time
      currentPayloadStr->lastTimestamp = rtp_header.timestamp;
      currentPayloadStr->lastPayloadLenByte = payloadSize;
    } else {
      currentPayloadStr->newPacket = false;
      currentPayloadStr->lastPayloadLenByte = payloadSize;
      currentPayloadStr->lastTimestamp = rtp_header.timestamp;
      currentPayloadStr->payloadType = rtp_header.payloadType;
      memset(currentPayloadStr->frameSizeStats, 0,
             MAX_NUM_FRAMESIZES * sizeof(ACMTestFrameSizeStats));
    }
  } else {
    n = 0;
    while (_payloadStats[n].payloadType != -1) {
      n++;
    }
    // first packet
    _payloadStats[n].newPacket = false;
    _payloadStats[n].lastPayloadLenByte = payloadSize;
    _payloadStats[n].lastTimestamp = rtp_header.timestamp;
    _payloadStats[n].payloadType = rtp_header.payloadType;
    memset(_payloadStats[n].frameSizeStats, 0,
           MAX_NUM_FRAMESIZES * sizeof(ACMTestFrameSizeStats));
  }
}

Channel::Channel(int16_t chID)
    : _receiverACM(NULL),
      _seqNo(0),
      _bitStreamFile(NULL),
      _saveBitStream(false),
      _lastPayloadType(-1),
      _isStereo(false),
      _leftChannel(true),
      _lastInTimestamp(0),
      _useLastFrameSize(false),
      _lastFrameSizeSample(0),
      _packetLoss(0),
      _useFECTestWithPacketLoss(false),
      _beginTime(rtc::TimeMillis()),
      _totalBytes(0),
      external_send_timestamp_(-1),
      external_sequence_number_(-1),
      num_packets_to_drop_(0) {
  int n;
  int k;
  for (n = 0; n < MAX_NUM_PAYLOADS; n++) {
    _payloadStats[n].payloadType = -1;
    _payloadStats[n].newPacket = true;
    for (k = 0; k < MAX_NUM_FRAMESIZES; k++) {
      _payloadStats[n].frameSizeStats[k].frameSizeSample = 0;
      _payloadStats[n].frameSizeStats[k].maxPayloadLen = 0;
      _payloadStats[n].frameSizeStats[k].numPackets = 0;
      _payloadStats[n].frameSizeStats[k].totalPayloadLenByte = 0;
      _payloadStats[n].frameSizeStats[k].totalEncodedSamples = 0;
    }
  }
  if (chID >= 0) {
    _saveBitStream = true;
    rtc::StringBuilder ss;
    ss.AppendFormat("bitStream_%d.dat", chID);
    _bitStreamFile = fopen(ss.str().c_str(), "wb");
  } else {
    _saveBitStream = false;
  }
}

Channel::~Channel() {}

void Channel::RegisterReceiverACM(acm2::AcmReceiver* acm_receiver) {
  _receiverACM = acm_receiver;
  return;
}

void Channel::ResetStats() {
  int n;
  int k;
  _channelCritSect.Lock();
  _lastPayloadType = -1;
  for (n = 0; n < MAX_NUM_PAYLOADS; n++) {
    _payloadStats[n].payloadType = -1;
    _payloadStats[n].newPacket = true;
    for (k = 0; k < MAX_NUM_FRAMESIZES; k++) {
      _payloadStats[n].frameSizeStats[k].frameSizeSample = 0;
      _payloadStats[n].frameSizeStats[k].maxPayloadLen = 0;
      _payloadStats[n].frameSizeStats[k].numPackets = 0;
      _payloadStats[n].frameSizeStats[k].totalPayloadLenByte = 0;
      _payloadStats[n].frameSizeStats[k].totalEncodedSamples = 0;
    }
  }
  _beginTime = rtc::TimeMillis();
  _totalBytes = 0;
  _channelCritSect.Unlock();
}

uint32_t Channel::LastInTimestamp() {
  uint32_t timestamp;
  _channelCritSect.Lock();
  timestamp = _lastInTimestamp;
  _channelCritSect.Unlock();
  return timestamp;
}

double Channel::BitRate() {
  double rate;
  uint64_t currTime = rtc::TimeMillis();
  _channelCritSect.Lock();
  rate = ((double)_totalBytes * 8.0) / (double)(currTime - _beginTime);
  _channelCritSect.Unlock();
  return rate;
}

}  // namespace webrtc
