/*
 *  Copyright (c) 2016 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <bitset>

#include "modules/rtp_rtcp/include/rtp_header_extension_map.h"
#include "modules/rtp_rtcp/source/rtp_generic_frame_descriptor_extension.h"
#include "modules/rtp_rtcp/source/rtp_header_extensions.h"
#include "modules/rtp_rtcp/source/rtp_packet_received.h"

namespace webrtc {

// We decide which header extensions to register by reading two bytes
// from the beginning of |data| and interpreting it as a bitmask over
// the RTPExtensionType enum. This assert ensures two bytes are enough.
static_assert(kRtpExtensionNumberOfExtensions <= 16,
              "Insufficient bits read to configure all header extensions. Add "
              "an extra byte and update the switches.");

void FuzzOneInput(const uint8_t* data, size_t size) {
  if (size <= 2)
    return;

  // Don't use the configuration bytes as part of the packet.
  std::bitset<16> extensionMask(*reinterpret_cast<const uint16_t*>(data));
  data += 2;
  size -= 2;

  RtpPacketReceived::ExtensionManager extensions;
  // Skip i = 0 since it maps to ExtensionNone and extension id = 0 is invalid.
  for (int i = 1; i < kRtpExtensionNumberOfExtensions; i++) {
    RTPExtensionType extension_type = static_cast<RTPExtensionType>(i);
    if (extensionMask[i] && extension_type != kRtpExtensionNone) {
      // Extensions are registered with an ID, which you signal to the
      // peer so they know what to expect. This code only cares about
      // parsing so the value of the ID isn't relevant; we use i.
      extensions.RegisterByType(i, extension_type);
    }
  }

  RtpPacketReceived packet(&extensions);
  packet.Parse(data, size);

  // Call packet accessors because they have extra checks.
  packet.Marker();
  packet.PayloadType();
  packet.SequenceNumber();
  packet.Timestamp();
  packet.Ssrc();
  packet.Csrcs();

  // Each extension has its own getter. It is supported behaviour to
  // call GetExtension on an extension which was not registered, so we
  // don't check the bitmask here.
  for (int i = 0; i < kRtpExtensionNumberOfExtensions; i++) {
    switch (static_cast<RTPExtensionType>(i)) {
      case kRtpExtensionNone:
      case kRtpExtensionNumberOfExtensions:
        break;
      case kRtpExtensionTransmissionTimeOffset:
        int32_t offset;
        packet.GetExtension<TransmissionOffset>(&offset);
        break;
      case kRtpExtensionAudioLevel:
        bool voice_activity;
        uint8_t audio_level;
        packet.GetExtension<AudioLevel>(&voice_activity, &audio_level);
        break;
      case kRtpExtensionAbsoluteSendTime:
        uint32_t sendtime;
        packet.GetExtension<AbsoluteSendTime>(&sendtime);
        break;
      case kRtpExtensionVideoRotation:
        uint8_t rotation;
        packet.GetExtension<VideoOrientation>(&rotation);
        break;
      case kRtpExtensionTransportSequenceNumber:
        uint16_t seqnum;
        packet.GetExtension<TransportSequenceNumber>(&seqnum);
        break;
      case kRtpExtensionPlayoutDelay:
        PlayoutDelay playout;
        packet.GetExtension<PlayoutDelayLimits>(&playout);
        break;
      case kRtpExtensionVideoContentType:
        VideoContentType content_type;
        packet.GetExtension<VideoContentTypeExtension>(&content_type);
        break;
      case kRtpExtensionVideoTiming:
        VideoSendTiming timing;
        packet.GetExtension<VideoTimingExtension>(&timing);
        break;
      case kRtpExtensionFrameMarking:
        FrameMarking frame_marking;
        packet.GetExtension<FrameMarkingExtension>(&frame_marking);
        break;
      case kRtpExtensionRtpStreamId: {
        std::string rsid;
        packet.GetExtension<RtpStreamId>(&rsid);
        break;
      }
      case kRtpExtensionRepairedRtpStreamId: {
        std::string rsid;
        packet.GetExtension<RepairedRtpStreamId>(&rsid);
        break;
      }
      case kRtpExtensionMid: {
        std::string mid;
        packet.GetExtension<RtpMid>(&mid);
        break;
      }
      case kRtpExtensionGenericFrameDescriptor: {
        RtpGenericFrameDescriptor descriptor;
        packet.GetExtension<RtpGenericFrameDescriptorExtension>(&descriptor);
        break;
      }
      case kRtpExtensionHdrMetadata: {
        HdrMetadata hdr_metadata;
        packet.GetExtension<HdrMetadataExtension>(&hdr_metadata);
        break;
      }
    }
  }
}
}  // namespace webrtc
