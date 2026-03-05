/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/pc/e2e/sdp/sdp_changer.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "api/array_view.h"
#include "api/jsep.h"
#include "api/media_types.h"
#include "api/rtp_parameters.h"
#include "api/rtp_transceiver_direction.h"
#include "api/test/pclf/media_configuration.h"
#include "media/base/media_constants.h"
#include "media/base/rid_description.h"
#include "media/base/stream_params.h"
#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "p2p/base/transport_info.h"
#include "pc/session_description.h"
#include "pc/simulcast_description.h"
#include "rtc_base/checks.h"
#include "rtc_base/strings/string_builder.h"
#include "rtc_base/unique_id_generator.h"

namespace webrtc {
namespace webrtc_pc_e2e {
namespace {

std::string CodecRequiredParamsToString(
    const std::map<std::string, std::string>& codec_required_params) {
  StringBuilder out;
  for (const auto& entry : codec_required_params) {
    out << entry.first << "=" << entry.second << ";";
  }
  return out.str();
}

std::string SupportedCodecsToString(
    ArrayView<const RtpCodecCapability> supported_codecs) {
  StringBuilder out;
  for (const auto& codec : supported_codecs) {
    out << codec.name;
    if (!codec.parameters.empty()) {
      out << "(";
      for (const auto& param : codec.parameters) {
        out << param.first << "=" << param.second << ";";
      }
      out << ")";
    }
    out << "; ";
  }
  return out.str();
}

}  // namespace

std::vector<RtpCodecCapability> FilterVideoCodecCapabilities(
    ArrayView<const VideoCodecConfig> video_codecs,
    bool use_rtx,
    bool use_ulpfec,
    bool use_flexfec,
    ArrayView<const RtpCodecCapability> supported_codecs) {
  std::vector<RtpCodecCapability> output_codecs;
  // Find requested codecs among supported and add them to output in the order
  // they were requested.
  for (auto& codec_request : video_codecs) {
    size_t size_before = output_codecs.size();
    for (auto& codec : supported_codecs) {
      if (codec.name != codec_request.name) {
        continue;
      }
      bool parameters_matched = true;
      for (const auto& item : codec_request.required_params) {
        auto it = codec.parameters.find(item.first);
        if (it == codec.parameters.end()) {
          parameters_matched = false;
          break;
        }
        if (item.second != it->second) {
          parameters_matched = false;
          break;
        }
      }
      if (parameters_matched) {
        output_codecs.push_back(codec);
      }
    }
    RTC_CHECK_GT(output_codecs.size(), size_before)
        << "Codec with name=" << codec_request.name << " and params {"
        << CodecRequiredParamsToString(codec_request.required_params)
        << "} is unsupported for this peer connection. Supported codecs are: "
        << SupportedCodecsToString(supported_codecs);
  }

  // Add required FEC and RTX codecs to output.
  for (auto& codec : supported_codecs) {
    if (codec.name == kRtxCodecName && use_rtx) {
      output_codecs.push_back(codec);
    } else if (codec.name == kFlexfecCodecName && use_flexfec) {
      output_codecs.push_back(codec);
    } else if ((codec.name == kRedCodecName ||
                codec.name == kUlpfecCodecName) &&
               use_ulpfec) {
      // Red and ulpfec should be enabled or disabled together.
      output_codecs.push_back(codec);
    }
  }
  return output_codecs;
}

// If offer has no simulcast video sections - do nothing.
//
// If offer has simulcast video sections - for each section creates
// SimulcastSectionInfo and put it into `context_`.
void SignalingInterceptor::FillSimulcastContext(
    SessionDescriptionInterface* offer) {
  for (auto& content : offer->description()->contents()) {
    MediaContentDescription* media_desc = content.media_description();
    if (media_desc->type() != MediaType::VIDEO) {
      continue;
    }
    if (media_desc->HasSimulcast()) {
      // We support only single stream simulcast sections with rids.
      RTC_CHECK_EQ(media_desc->mutable_streams().size(), 1);
      RTC_CHECK(media_desc->mutable_streams()[0].has_rids());

      // Create SimulcastSectionInfo for this video section.
      SimulcastSectionInfo info(content.mid(), content.type,
                                media_desc->mutable_streams()[0].rids());

      // Set new rids basing on created SimulcastSectionInfo.
      std::vector<RidDescription> rids;
      SimulcastDescription simulcast_description;
      for (std::string& rid : info.rids) {
        rids.emplace_back(rid, RidDirection::kSend);
        simulcast_description.send_layers().AddLayer(
            SimulcastLayer(rid, false));
      }
      media_desc->mutable_streams()[0].set_rids(rids);
      media_desc->set_simulcast_description(simulcast_description);

      info.simulcast_description = media_desc->simulcast_description();
      for (const auto& extension : media_desc->rtp_header_extensions()) {
        if (extension.uri == RtpExtension::kMidUri) {
          info.mid_extension = extension;
        } else if (extension.uri == RtpExtension::kRidUri) {
          info.rid_extension = extension;
        } else if (extension.uri == RtpExtension::kRepairedRidUri) {
          info.rrid_extension = extension;
        }
      }
      RTC_CHECK_NE(info.rid_extension.id, 0);
      RTC_CHECK_NE(info.mid_extension.id, 0);
      bool transport_description_found = false;
      for (auto& transport_info : offer->description()->transport_infos()) {
        if (transport_info.content_name == info.mid) {
          info.transport_description = transport_info.description;
          transport_description_found = true;
          break;
        }
      }
      RTC_CHECK(transport_description_found);

      context_.AddSimulcastInfo(info);
    }
  }
}

LocalAndRemoteSdp SignalingInterceptor::PatchOffer(
    std::unique_ptr<SessionDescriptionInterface> offer,
    const VideoCodecConfig& first_codec) {
  for (auto& content : offer->description()->contents()) {
    context_.mids_order.push_back(content.mid());
    MediaContentDescription* media_desc = content.media_description();
    if (media_desc->type() != MediaType::VIDEO) {
      continue;
    }
    if (content.media_description()->streams().empty()) {
      // It means that this media section describes receive only media section
      // in SDP.
      RTC_CHECK_EQ(content.media_description()->direction(),
                   RtpTransceiverDirection::kRecvOnly);
      continue;
    }
    media_desc->set_conference_mode(params_.use_conference_mode);
  }

  if (!params_.stream_label_to_simulcast_streams_count.empty()) {
    // Because simulcast enabled `params_.video_codecs` has only 1 element.
    if (first_codec.name == kVp8CodecName) {
      return PatchVp8Offer(std::move(offer));
    }

    if (first_codec.name == kVp9CodecName) {
      return PatchVp9Offer(std::move(offer));
    }
  }

  auto offer_for_remote = offer->Clone();
  return LocalAndRemoteSdp(std::move(offer), std::move(offer_for_remote));
}

LocalAndRemoteSdp SignalingInterceptor::PatchVp8Offer(
    std::unique_ptr<SessionDescriptionInterface> offer) {
  FillSimulcastContext(offer.get());
  if (!context_.HasSimulcast()) {
    auto offer_for_remote = offer->Clone();
    return LocalAndRemoteSdp(std::move(offer), std::move(offer_for_remote));
  }

  // Clone original offer description. We mustn't access original offer after
  // this point.
  std::unique_ptr<SessionDescription> desc = offer->description()->Clone();

  for (auto& info : context_.simulcast_infos) {
    // For each simulcast section we have to perform:
    //   1. Swap MID and RID header extensions
    //   2. Remove RIDs from streams and remove SimulcastDescription
    //   3. For each RID duplicate media section
    ContentInfo* simulcast_content = desc->GetContentByName(info.mid);

    // Now we need to prepare common prototype for "m=video" sections, in which
    // single simulcast section will be converted. Do it before removing content
    // because otherwise description will be deleted.
    std::unique_ptr<MediaContentDescription> prototype_media_desc =
        simulcast_content->media_description()->Clone();

    // Remove simulcast video section from offer.
    RTC_CHECK(desc->RemoveContentByName(simulcast_content->mid()));
    // Clear `simulcast_content`, because now it is pointing to removed object.
    simulcast_content = nullptr;

    // Swap mid and rid extensions, so remote peer will understand rid as mid.
    // Also remove rid extension.
    std::vector<RtpExtension> extensions =
        prototype_media_desc->rtp_header_extensions();
    for (auto ext_it = extensions.begin(); ext_it != extensions.end();) {
      if (ext_it->uri == RtpExtension::kRidUri) {
        // We don't need rid extension for remote peer.
        ext_it = extensions.erase(ext_it);
        continue;
      }
      if (ext_it->uri == RtpExtension::kRepairedRidUri) {
        // We don't support RTX in simulcast.
        ext_it = extensions.erase(ext_it);
        continue;
      }
      if (ext_it->uri == RtpExtension::kMidUri) {
        ext_it->id = info.rid_extension.id;
      }
      ++ext_it;
    }

    prototype_media_desc->set_rtp_header_extensions(extensions);

    // We support only single stream inside video section with simulcast
    RTC_CHECK_EQ(prototype_media_desc->mutable_streams().size(), 1);
    // This stream must have rids.
    RTC_CHECK(prototype_media_desc->mutable_streams()[0].has_rids());

    // Remove rids and simulcast description from media description.
    prototype_media_desc->mutable_streams()[0].set_rids({});
    prototype_media_desc->set_simulcast_description(SimulcastDescription());

    // For each rid add separate video section.
    for (std::string& rid : info.rids) {
      desc->AddContent(rid, info.media_protocol_type,
                       prototype_media_desc->Clone());
    }
  }

  // Now we need to add bundle line to have all media bundled together.
  ContentGroup bundle_group(GROUP_TYPE_BUNDLE);
  for (auto& content : desc->contents()) {
    bundle_group.AddContentName(content.mid());
  }
  if (desc->HasGroup(GROUP_TYPE_BUNDLE)) {
    desc->RemoveGroupByName(GROUP_TYPE_BUNDLE);
  }
  desc->AddGroup(bundle_group);

  // Update transport_infos to add TransportInfo for each new media section.
  std::vector<TransportInfo> transport_infos = desc->transport_infos();
  std::erase_if(transport_infos, [this](const TransportInfo& ti) {
    // Remove transport infos that correspond to simulcast video sections.
    return context_.simulcast_infos_by_mid.contains(ti.content_name);
  });
  for (auto& info : context_.simulcast_infos) {
    for (auto& rid : info.rids) {
      transport_infos.emplace_back(rid, info.transport_description);
    }
  }
  desc->set_transport_infos(transport_infos);

  // Create patched offer.
  std::unique_ptr<SessionDescriptionInterface> patched_offer =
      CreateSessionDescription(SdpType::kOffer, offer->session_id(),
                               offer->session_version(), std::move(desc));

  return LocalAndRemoteSdp(std::move(offer), std::move(patched_offer));
}

LocalAndRemoteSdp SignalingInterceptor::PatchVp9Offer(
    std::unique_ptr<SessionDescriptionInterface> offer) {
  UniqueRandomIdGenerator ssrcs_generator;
  for (auto& content : offer->description()->contents()) {
    for (auto& stream : content.media_description()->streams()) {
      for (auto& ssrc : stream.ssrcs) {
        ssrcs_generator.AddKnownId(ssrc);
      }
    }
  }

  for (auto& content : offer->description()->contents()) {
    if (content.media_description()->type() != MediaType::VIDEO) {
      // We are interested in only video tracks
      continue;
    }
    if (content.media_description()->direction() ==
        RtpTransceiverDirection::kRecvOnly) {
      // If direction is receive only, then there is no media in this track from
      // sender side, so we needn't to do anything with this track.
      continue;
    }
    RTC_CHECK_EQ(content.media_description()->streams().size(), 1);
    StreamParams& stream = content.media_description()->mutable_streams()[0];
    RTC_CHECK_EQ(stream.stream_ids().size(), 2)
        << "Expected 2 stream ids in video stream: 1st - sync_group, 2nd - "
           "unique label";
    std::string stream_label = stream.stream_ids()[1];

    auto it =
        params_.stream_label_to_simulcast_streams_count.find(stream_label);
    if (it == params_.stream_label_to_simulcast_streams_count.end()) {
      continue;
    }
    int svc_layers_count = it->second;

    RTC_CHECK(stream.has_ssrc_groups()) << "Only SVC with RTX is supported";
    RTC_CHECK_EQ(stream.ssrc_groups.size(), 1)
        << "Too many ssrc groups in the track";
    std::vector<uint32_t> primary_ssrcs;
    stream.GetPrimarySsrcs(&primary_ssrcs);
    RTC_CHECK(primary_ssrcs.size() == 1);
    for (int i = 1; i < svc_layers_count; ++i) {
      uint32_t ssrc = ssrcs_generator.GenerateId();
      primary_ssrcs.push_back(ssrc);
      stream.add_ssrc(ssrc);
      stream.AddFidSsrc(ssrc, ssrcs_generator.GenerateId());
    }
    stream.ssrc_groups.push_back(
        SsrcGroup(kSimSsrcGroupSemantics, primary_ssrcs));
  }
  auto offer_for_remote = offer->Clone();
  return LocalAndRemoteSdp(std::move(offer), std::move(offer_for_remote));
}

LocalAndRemoteSdp SignalingInterceptor::PatchAnswer(
    std::unique_ptr<SessionDescriptionInterface> answer,
    const VideoCodecConfig& first_codec) {
  for (auto& content : answer->description()->contents()) {
    MediaContentDescription* media_desc = content.media_description();
    if (media_desc->type() != MediaType::VIDEO) {
      continue;
    }
    if (content.media_description()->direction() !=
        RtpTransceiverDirection::kRecvOnly) {
      continue;
    }
    media_desc->set_conference_mode(params_.use_conference_mode);
  }

  if (!params_.stream_label_to_simulcast_streams_count.empty()) {
    // Because simulcast enabled `params_.video_codecs` has only 1 element.
    if (first_codec.name == kVp8CodecName) {
      return PatchVp8Answer(std::move(answer));
    }

    if (first_codec.name == kVp9CodecName) {
      return PatchVp9Answer(std::move(answer));
    }
  }

  auto answer_for_remote = answer->Clone();
  return LocalAndRemoteSdp(std::move(answer), std::move(answer_for_remote));
}

LocalAndRemoteSdp SignalingInterceptor::PatchVp8Answer(
    std::unique_ptr<SessionDescriptionInterface> answer) {
  if (!context_.HasSimulcast()) {
    auto answer_for_remote = answer->Clone();
    return LocalAndRemoteSdp(std::move(answer), std::move(answer_for_remote));
  }

  std::unique_ptr<SessionDescription> desc = answer->description()->Clone();

  for (auto& info : context_.simulcast_infos) {
    ContentInfo* simulcast_content = desc->GetContentByName(info.rids[0]);
    RTC_CHECK(simulcast_content);

    // Get media description, which will be converted to simulcast answer.
    std::unique_ptr<MediaContentDescription> media_desc =
        simulcast_content->media_description()->Clone();
    // Set `simulcast_content` to nullptr, because then it will be removed, so
    // it will point to deleted object.
    simulcast_content = nullptr;

    // Remove separate media sections for simulcast streams.
    for (auto& rid : info.rids) {
      RTC_CHECK(desc->RemoveContentByName(rid));
    }

    // Patch `media_desc` to make it simulcast answer description.
    // Restore mid/rid rtp header extensions
    std::vector<RtpExtension> extensions = media_desc->rtp_header_extensions();
    // First remove existing rid/mid header extensions.
    std::erase_if(extensions, [](const webrtc::RtpExtension& e) {
      return e.uri == RtpExtension::kMidUri || e.uri == RtpExtension::kRidUri ||
             e.uri == RtpExtension::kRepairedRidUri;
    });

    // Then add right ones.
    extensions.push_back(info.mid_extension);
    extensions.push_back(info.rid_extension);
    // extensions.push_back(info.rrid_extension);
    media_desc->set_rtp_header_extensions(extensions);

    // Add StreamParams with rids for receive.
    RTC_CHECK_EQ(media_desc->mutable_streams().size(), 0);
    std::vector<RidDescription> rids;
    for (auto& rid : info.rids) {
      rids.emplace_back(rid, RidDirection::kReceive);
    }
    StreamParams stream_params;
    stream_params.set_rids(rids);
    media_desc->mutable_streams().push_back(stream_params);

    // Restore SimulcastDescription. It should correspond to one from offer,
    // but it have to have receive layers instead of send. So we need to put
    // send layers from offer to receive layers in answer.
    SimulcastDescription simulcast_description;
    for (const auto& layer : info.simulcast_description.send_layers()) {
      simulcast_description.receive_layers().AddLayerWithAlternatives(layer);
    }
    media_desc->set_simulcast_description(simulcast_description);

    // Add simulcast media section.
    desc->AddContent(info.mid, info.media_protocol_type, std::move(media_desc));
  }

  desc = RestoreMediaSectionsOrder(std::move(desc));

  // Now we need to add bundle line to have all media bundled together.
  ContentGroup bundle_group(GROUP_TYPE_BUNDLE);
  for (auto& content : desc->contents()) {
    bundle_group.AddContentName(content.mid());
  }
  if (desc->HasGroup(GROUP_TYPE_BUNDLE)) {
    desc->RemoveGroupByName(GROUP_TYPE_BUNDLE);
  }
  desc->AddGroup(bundle_group);

  // Fix transport_infos: it have to have single info for simulcast section.
  std::vector<TransportInfo> transport_infos = desc->transport_infos();
  std::map<std::string, TransportDescription> mid_to_transport_description;
  for (auto info_it = transport_infos.begin();
       info_it != transport_infos.end();) {
    auto it = context_.simulcast_infos_by_rid.find(info_it->content_name);
    if (it != context_.simulcast_infos_by_rid.end()) {
      // This transport info correspond to some extra added media section.
      mid_to_transport_description.insert(
          {it->second->mid, info_it->description});
      info_it = transport_infos.erase(info_it);
    } else {
      ++info_it;
    }
  }
  for (auto& info : context_.simulcast_infos) {
    transport_infos.emplace_back(info.mid,
                                 mid_to_transport_description.at(info.mid));
  }
  desc->set_transport_infos(transport_infos);

  std::unique_ptr<SessionDescriptionInterface> patched_answer =
      CreateSessionDescription(SdpType::kAnswer, answer->session_id(),
                               answer->session_version(), std::move(desc));

  return LocalAndRemoteSdp(std::move(answer), std::move(patched_answer));
}

std::unique_ptr<SessionDescription>
SignalingInterceptor::RestoreMediaSectionsOrder(
    std::unique_ptr<SessionDescription> source) {
  std::unique_ptr<SessionDescription> out = source->Clone();
  for (auto& mid : context_.mids_order) {
    RTC_CHECK(out->RemoveContentByName(mid));
  }
  RTC_CHECK_EQ(out->contents().size(), 0);
  for (auto& mid : context_.mids_order) {
    ContentInfo* content = source->GetContentByName(mid);
    RTC_CHECK(content);
    out->AddContent(mid, content->type, content->media_description()->Clone());
  }
  return out;
}

LocalAndRemoteSdp SignalingInterceptor::PatchVp9Answer(
    std::unique_ptr<SessionDescriptionInterface> answer) {
  auto answer_for_remote = answer->Clone();
  return LocalAndRemoteSdp(std::move(answer), std::move(answer_for_remote));
}

std::vector<std::unique_ptr<IceCandidate>>
SignalingInterceptor::PatchOffererIceCandidates(
    ArrayView<const IceCandidate* const> candidates) {
  std::vector<std::unique_ptr<IceCandidate>> out;
  for (auto* candidate : candidates) {
    auto simulcast_info_it =
        context_.simulcast_infos_by_mid.find(candidate->sdp_mid());
    if (simulcast_info_it != context_.simulcast_infos_by_mid.end()) {
      // This is candidate for simulcast section, so it should be transformed
      // into candidates for replicated sections. The sdpMLineIndex is set to
      // -1 and ignored if the rid is present.
      for (const std::string& rid : simulcast_info_it->second->rids) {
        out.push_back(CreateIceCandidate(rid, -1, candidate->candidate()));
      }
    } else {
      out.push_back(CreateIceCandidate(candidate->sdp_mid(),
                                       candidate->sdp_mline_index(),
                                       candidate->candidate()));
    }
  }
  RTC_CHECK_GT(out.size(), 0);
  return out;
}

std::vector<std::unique_ptr<IceCandidate>>
SignalingInterceptor::PatchAnswererIceCandidates(
    ArrayView<const IceCandidate* const> candidates) {
  std::vector<std::unique_ptr<IceCandidate>> out;
  for (auto* candidate : candidates) {
    auto simulcast_info_it =
        context_.simulcast_infos_by_rid.find(candidate->sdp_mid());
    if (simulcast_info_it != context_.simulcast_infos_by_rid.end()) {
      // This is candidate for replicated section, created from single simulcast
      // section, so it should be transformed into candidates for simulcast
      // section.
      out.push_back(CreateIceCandidate(simulcast_info_it->second->mid, 0,
                                       candidate->candidate()));
    } else if (!context_.simulcast_infos_by_rid.empty()) {
      // When using simulcast and bundle, put everything on the first m-line.
      out.push_back(CreateIceCandidate("", 0, candidate->candidate()));
    } else {
      out.push_back(CreateIceCandidate(candidate->sdp_mid(),
                                       candidate->sdp_mline_index(),
                                       candidate->candidate()));
    }
  }
  RTC_CHECK_GT(out.size(), 0);
  return out;
}

SignalingInterceptor::SimulcastSectionInfo::SimulcastSectionInfo(
    const std::string& mid,
    MediaProtocolType media_protocol_type,
    const std::vector<RidDescription>& rids_desc)
    : mid(mid), media_protocol_type(media_protocol_type) {
  for (auto& rid : rids_desc) {
    rids.push_back(rid.rid);
  }
}

void SignalingInterceptor::SignalingContext::AddSimulcastInfo(
    const SimulcastSectionInfo& info) {
  simulcast_infos.push_back(info);
  bool inserted =
      simulcast_infos_by_mid.insert({info.mid, &simulcast_infos.back()}).second;
  RTC_CHECK(inserted);
  for (auto& rid : info.rids) {
    inserted =
        simulcast_infos_by_rid.insert({rid, &simulcast_infos.back()}).second;
    RTC_CHECK(inserted);
  }
}

}  // namespace webrtc_pc_e2e
}  // namespace webrtc
