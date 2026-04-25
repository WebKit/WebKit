/*
 *  Copyright 2020 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "examples/androidvoip/jni/android_voip_client.h"

#include <errno.h>
#include <jni.h>
#include <sys/socket.h>  // no-presubmit-check

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/array_view.h"
#include "api/audio/builtin_audio_processing_builder.h"
#include "api/audio_codecs/audio_format.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/call/transport.h"
#include "api/environment/environment_factory.h"
#include "api/sequence_checker.h"
#include "api/units/time_delta.h"
#include "api/voip/voip_base.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_engine_factory.h"
#include "api/voip/voip_network.h"
#include "api/voip/voip_statistics.h"
#include "examples/androidvoip/generated_jni/VoipClient_jni.h"
#include "rtc_base/async_packet_socket.h"
#include "rtc_base/async_udp_socket.h"
#include "rtc_base/checks.h"
#include "rtc_base/ip_address.h"
#include "rtc_base/logging.h"
#include "rtc_base/network/received_packet.h"
#include "rtc_base/socket.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/socket_server.h"
#include "sdk/android/native_api/audio_device_module/audio_device_android.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "third_party/jni_zero/jni_zero.h"

namespace {

#define RUN_ON_VOIP_THREAD(method, ...)                              \
  if (!voip_thread_->IsCurrent()) {                                  \
    voip_thread_->PostTask(                                          \
        std::bind(&AndroidVoipClient::method, this, ##__VA_ARGS__)); \
    return;                                                          \
  }                                                                  \
  RTC_DCHECK_RUN_ON(voip_thread_.get());

// Connects a UDP socket to a public address and returns the local
// address associated with it. Since it binds to the "any" address
// internally, it returns the default local address on a multi-homed
// endpoint. Implementation copied from
// BasicNetworkManager::QueryDefaultLocalAddress.
webrtc::IPAddress QueryDefaultLocalAddress(int family) {
  const char kPublicIPv4Host[] = "8.8.8.8";
  const char kPublicIPv6Host[] = "2001:4860:4860::8888";
  const int kPublicPort = 53;
  std::unique_ptr<webrtc::Thread> thread =
      webrtc::Thread::CreateWithSocketServer();

  RTC_DCHECK(thread->socketserver() != nullptr);
  RTC_DCHECK(family == AF_INET || family == AF_INET6);

  std::unique_ptr<webrtc::Socket> socket(
      thread->socketserver()->CreateSocket(family, SOCK_DGRAM));
  if (!socket) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    return webrtc::IPAddress();
  }

  auto host = family == AF_INET ? kPublicIPv4Host : kPublicIPv6Host;
  if (socket->Connect(webrtc::SocketAddress(host, kPublicPort)) < 0) {
    if (socket->GetError() != ENETUNREACH &&
        socket->GetError() != EHOSTUNREACH) {
      RTC_LOG(LS_INFO) << "Connect failed with " << socket->GetError();
    }
    return webrtc::IPAddress();
  }
  return socket->GetLocalAddress().ipaddr();
}

// Assigned payload type for supported built-in codecs. PCMU, PCMA,
// and G722 have set payload types. Whereas opus, ISAC, and ILBC
// have dynamic payload types.
enum class PayloadType : int {
  kPcmu = 0,
  kPcma = 8,
  kG722 = 9,
  kOpus = 96,
  kIsac = 97,
  kIlbc = 98,
};

// Returns the payload type corresponding to codec_name. Only
// supports the built-in codecs.
int GetPayloadType(const std::string& codec_name) {
  RTC_DCHECK(codec_name == "PCMU" || codec_name == "PCMA" ||
             codec_name == "G722" || codec_name == "opus" ||
             codec_name == "ISAC" || codec_name == "ILBC");

  if (codec_name == "PCMU") {
    return static_cast<int>(PayloadType::kPcmu);
  } else if (codec_name == "PCMA") {
    return static_cast<int>(PayloadType::kPcma);
  } else if (codec_name == "G722") {
    return static_cast<int>(PayloadType::kG722);
  } else if (codec_name == "opus") {
    return static_cast<int>(PayloadType::kOpus);
  } else if (codec_name == "ISAC") {
    return static_cast<int>(PayloadType::kIsac);
  }

  RTC_DCHECK_NOTREACHED();
  return -1;
}

}  // namespace

namespace webrtc_examples {

AndroidVoipClient::AndroidVoipClient(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_voip_client)
    : webrtc_env_(webrtc::CreateEnvironment()),
      voip_thread_(webrtc::Thread::CreateWithSocketServer()),
      j_voip_client_(env, j_voip_client) {}

void AndroidVoipClient::Init(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& application_context) {
  webrtc::VoipEngineConfig config;
  config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  config.decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  config.env = webrtc_env_;
  config.audio_device_module = webrtc::CreateJavaAudioDeviceModule(
      env, *config.env, application_context.obj());
  config.audio_processing_builder =
      std::make_unique<webrtc::BuiltinAudioProcessingBuilder>();

  voip_thread_->Start();

  // Due to consistent thread requirement on
  // modules/audio_device/android/audio_device_template.h,
  // code is invoked in the context of voip_thread_.
  voip_thread_->BlockingCall([this, &config] {
    RTC_DCHECK_RUN_ON(voip_thread_.get());

    supported_codecs_ = config.encoder_factory->GetSupportedEncoders();
    env_ = webrtc::AttachCurrentThreadIfNeeded();
    voip_engine_ = webrtc::CreateVoipEngine(std::move(config));
  });
}

AndroidVoipClient::~AndroidVoipClient() {
  voip_thread_->BlockingCall([this] {
    RTC_DCHECK_RUN_ON(voip_thread_.get());

    JavaVM* jvm = nullptr;
    env_->GetJavaVM(&jvm);
    if (!jvm) {
      RTC_LOG(LS_ERROR) << "Failed to retrieve JVM";
      return;
    }
    jint res = jvm->DetachCurrentThread();
    if (res != JNI_OK) {
      RTC_LOG(LS_ERROR) << "DetachCurrentThread failed: " << res;
    }
  });

  voip_thread_->Stop();
}

AndroidVoipClient* AndroidVoipClient::Create(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& application_context,
    const jni_zero::JavaRef<jobject>& j_voip_client) {
  // Using `new` to access a non-public constructor.
  auto voip_client =
      absl::WrapUnique(new AndroidVoipClient(env, j_voip_client));
  voip_client->Init(env, application_context);
  return voip_client.release();
}

void AndroidVoipClient::GetSupportedCodecs(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(GetSupportedCodecs, env);

  std::vector<std::string> names;
  for (const webrtc::AudioCodecSpec& spec : supported_codecs_) {
    names.push_back(spec.format.name);
  }
  jni_zero::ScopedJavaLocalRef<jstring> (*convert_function)(
      JNIEnv*, const std::string&) = &webrtc::NativeToJavaString;
  Java_VoipClient_onGetSupportedCodecsCompleted(
      env_, j_voip_client_,
      webrtc::NativeToJavaList(env_, names, convert_function));
}

void AndroidVoipClient::GetLocalIPAddress(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(GetLocalIPAddress, env);

  std::string local_ip_address;
  webrtc::IPAddress ipv4_address = QueryDefaultLocalAddress(AF_INET);
  if (!ipv4_address.IsNil()) {
    local_ip_address = ipv4_address.ToString();
  } else {
    webrtc::IPAddress ipv6_address = QueryDefaultLocalAddress(AF_INET6);
    if (!ipv6_address.IsNil()) {
      local_ip_address = ipv6_address.ToString();
    }
  }
  Java_VoipClient_onGetLocalIPAddressCompleted(
      env_, j_voip_client_, webrtc::NativeToJavaString(env_, local_ip_address));
}

void AndroidVoipClient::SetEncoder(const std::string& encoder) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    return;
  }
  for (const webrtc::AudioCodecSpec& codec : supported_codecs_) {
    if (codec.format.name == encoder) {
      webrtc::VoipResult result = voip_engine_->Codec().SetSendCodec(
          *channel_, GetPayloadType(codec.format.name), codec.format);
      RTC_CHECK(result == webrtc::VoipResult::kOk);
      return;
    }
  }
}

void AndroidVoipClient::SetEncoder(
    JNIEnv* env,
    const jni_zero::JavaRef<jstring>& j_encoder_string) {
  const std::string& chosen_encoder =
      webrtc::JavaToNativeString(env, j_encoder_string);
  voip_thread_->PostTask(
      [this, chosen_encoder] { SetEncoder(chosen_encoder); });
}

void AndroidVoipClient::SetDecoders(const std::vector<std::string>& decoders) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    return;
  }
  std::map<int, webrtc::SdpAudioFormat> decoder_specs;
  for (const webrtc::AudioCodecSpec& codec : supported_codecs_) {
    if (std::find(decoders.begin(), decoders.end(), codec.format.name) !=
        decoders.end()) {
      decoder_specs.insert({GetPayloadType(codec.format.name), codec.format});
    }
  }

  webrtc::VoipResult result =
      voip_engine_->Codec().SetReceiveCodecs(*channel_, decoder_specs);
  RTC_CHECK(result == webrtc::VoipResult::kOk);
}

void AndroidVoipClient::SetDecoders(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& j_decoder_strings) {
  const std::vector<std::string>& chosen_decoders =
      webrtc::JavaListToNativeVector<std::string, jstring>(
          env, j_decoder_strings, &webrtc::JavaToNativeString);
  voip_thread_->PostTask(
      [this, chosen_decoders] { SetDecoders(chosen_decoders); });
}

void AndroidVoipClient::SetLocalAddress(const std::string& ip_address,
                                        const int port_number) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  rtp_local_address_ = webrtc::SocketAddress(ip_address, port_number);
  rtcp_local_address_ = webrtc::SocketAddress(ip_address, port_number + 1);
}

void AndroidVoipClient::SetLocalAddress(
    JNIEnv* env,
    const jni_zero::JavaRef<jstring>& j_ip_address_string,
    jint j_port_number_int) {
  const std::string& ip_address =
      webrtc::JavaToNativeString(env, j_ip_address_string);
  voip_thread_->PostTask([this, ip_address, j_port_number_int] {
    SetLocalAddress(ip_address, j_port_number_int);
  });
}

void AndroidVoipClient::SetRemoteAddress(const std::string& ip_address,
                                         const int port_number) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  rtp_remote_address_ = webrtc::SocketAddress(ip_address, port_number);
  rtcp_remote_address_ = webrtc::SocketAddress(ip_address, port_number + 1);
}

void AndroidVoipClient::SetRemoteAddress(
    JNIEnv* env,
    const jni_zero::JavaRef<jstring>& j_ip_address_string,
    jint j_port_number_int) {
  const std::string& ip_address =
      webrtc::JavaToNativeString(env, j_ip_address_string);
  voip_thread_->PostTask([this, ip_address, j_port_number_int] {
    SetRemoteAddress(ip_address, j_port_number_int);
  });
}

void AndroidVoipClient::StartSession(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StartSession, env);

  // CreateChannel guarantees to return valid channel id.
  channel_ = voip_engine_->Base().CreateChannel(this, std::nullopt);

  rtp_socket_ = webrtc::AsyncUDPSocket::Create(webrtc_env_, rtp_local_address_,
                                               *voip_thread_->socketserver());
  if (!rtp_socket_) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                            /*isSuccessful=*/false);
    return;
  }
  rtp_socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket* socket,
          const webrtc::ReceivedIpPacket& packet) {
        OnSignalReadRTPPacket(socket, packet);
      });

  rtcp_socket_ = webrtc::AsyncUDPSocket::Create(
      webrtc_env_, rtcp_local_address_, *voip_thread_->socketserver());
  if (!rtcp_socket_) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                            /*isSuccessful=*/false);
    return;
  }
  rtcp_socket_->RegisterReceivedPacketCallback(
      [&](webrtc::AsyncPacketSocket* socket,
          const webrtc::ReceivedIpPacket& packet) {
        OnSignalReadRTCPPacket(socket, packet);
      });
  Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                          /*isSuccessful=*/true);
  voip_thread_->PostTask([this, env] { LogChannelStatistics(env); });
}

void AndroidVoipClient::LogChannelStatistics(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(LogChannelStatistics, env)

  if (!channel_)
    return;
  webrtc::ChannelStatistics stats;
  if (voip_engine_->Statistics().GetChannelStatistics(*channel_, stats) ==
      webrtc::VoipResult::kInvalidArgument)
    return;

  RTC_LOG(LS_INFO) << "PACKETS SENT: " << stats.packets_sent
                   << " BYTES SENT: " << stats.bytes_sent
                   << " PACKETS RECV: " << stats.packets_received
                   << " BYTES RECV: " << stats.bytes_received
                   << " JITTER: " << stats.jitter
                   << " PACKETS LOST: " << stats.packets_lost;

  voip_thread_->PostDelayedTask([this, env] { LogChannelStatistics(env); },
                                webrtc::TimeDelta::Seconds(1));
}

void AndroidVoipClient::StopSession(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StopSession, env);

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    Java_VoipClient_onStopSessionCompleted(env_, j_voip_client_,
                                           /*isSuccessful=*/false);
    return;
  }
  if (voip_engine_->Base().StopSend(*channel_) != webrtc::VoipResult::kOk ||
      voip_engine_->Base().StopPlayout(*channel_) != webrtc::VoipResult::kOk) {
    Java_VoipClient_onStopSessionCompleted(env_, j_voip_client_,
                                           /*isSuccessful=*/false);
    return;
  }

  rtp_socket_->Close();
  rtcp_socket_->Close();

  webrtc::VoipResult result = voip_engine_->Base().ReleaseChannel(*channel_);
  RTC_CHECK(result == webrtc::VoipResult::kOk);

  channel_ = std::nullopt;
  Java_VoipClient_onStopSessionCompleted(env_, j_voip_client_,
                                         /*isSuccessful=*/true);
}

void AndroidVoipClient::StartSend(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StartSend, env);

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    Java_VoipClient_onStartSendCompleted(env_, j_voip_client_,
                                         /*isSuccessful=*/false);
    return;
  }
  bool sending_started =
      (voip_engine_->Base().StartSend(*channel_) == webrtc::VoipResult::kOk);
  Java_VoipClient_onStartSendCompleted(env_, j_voip_client_, sending_started);
}

void AndroidVoipClient::StopSend(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StopSend, env);

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    Java_VoipClient_onStopSendCompleted(env_, j_voip_client_,
                                        /*isSuccessful=*/false);
    return;
  }
  bool sending_stopped =
      (voip_engine_->Base().StopSend(*channel_) == webrtc::VoipResult::kOk);
  Java_VoipClient_onStopSendCompleted(env_, j_voip_client_, sending_stopped);
}

void AndroidVoipClient::StartPlayout(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StartPlayout, env);

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    Java_VoipClient_onStartPlayoutCompleted(env_, j_voip_client_,
                                            /*isSuccessful=*/false);
    return;
  }
  bool playout_started =
      (voip_engine_->Base().StartPlayout(*channel_) == webrtc::VoipResult::kOk);
  Java_VoipClient_onStartPlayoutCompleted(env_, j_voip_client_,
                                          playout_started);
}

void AndroidVoipClient::StopPlayout(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(StopPlayout, env);

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    Java_VoipClient_onStopPlayoutCompleted(env_, j_voip_client_,
                                           /*isSuccessful=*/false);
    return;
  }
  bool playout_stopped =
      (voip_engine_->Base().StopPlayout(*channel_) == webrtc::VoipResult::kOk);
  Java_VoipClient_onStopPlayoutCompleted(env_, j_voip_client_, playout_stopped);
}

void AndroidVoipClient::Delete(JNIEnv* env) {
  delete this;
}

void AndroidVoipClient::SendRtpPacket(const std::vector<uint8_t>& packet_copy) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!rtp_socket_->SendTo(packet_copy.data(), packet_copy.size(),
                           rtp_remote_address_,
                           webrtc::AsyncSocketPacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTP packet";
  }
}

bool AndroidVoipClient::SendRtp(webrtc::ArrayView<const uint8_t> packet,
                                const webrtc::PacketOptions& options) {
  std::vector<uint8_t> packet_copy(packet.begin(), packet.end());
  voip_thread_->PostTask([this, packet_copy = std::move(packet_copy)] {
    SendRtpPacket(packet_copy);
  });
  return true;
}

void AndroidVoipClient::SendRtcpPacket(
    const std::vector<uint8_t>& packet_copy) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!rtcp_socket_->SendTo(packet_copy.data(), packet_copy.size(),
                            rtcp_remote_address_,
                            webrtc::AsyncSocketPacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTCP packet";
  }
}

bool AndroidVoipClient::SendRtcp(webrtc::ArrayView<const uint8_t> packet,
                                 const webrtc::PacketOptions& options) {
  std::vector<uint8_t> packet_copy(packet.begin(), packet.end());
  voip_thread_->PostTask([this, packet_copy = std::move(packet_copy)] {
    SendRtcpPacket(packet_copy);
  });
  return true;
}

void AndroidVoipClient::ReadRTPPacket(const std::vector<uint8_t>& packet_copy) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    return;
  }
  webrtc::VoipResult result = voip_engine_->Network().ReceivedRTPPacket(
      *channel_,
      webrtc::ArrayView<const uint8_t>(packet_copy.data(), packet_copy.size()));
  RTC_CHECK(result == webrtc::VoipResult::kOk);
}

void AndroidVoipClient::OnSignalReadRTPPacket(
    webrtc::AsyncPacketSocket* socket,
    const webrtc::ReceivedIpPacket& packet) {
  std::vector<uint8_t> packet_copy(packet.payload().begin(),
                                   packet.payload().end());
  voip_thread_->PostTask([this, packet_copy = std::move(packet_copy)] {
    ReadRTPPacket(packet_copy);
  });
}

void AndroidVoipClient::ReadRTCPPacket(
    const std::vector<uint8_t>& packet_copy) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  if (!channel_) {
    RTC_LOG(LS_ERROR) << "Channel has not been created";
    return;
  }
  webrtc::VoipResult result = voip_engine_->Network().ReceivedRTCPPacket(
      *channel_,
      webrtc::ArrayView<const uint8_t>(packet_copy.data(), packet_copy.size()));
  RTC_CHECK(result == webrtc::VoipResult::kOk);
}

void AndroidVoipClient::OnSignalReadRTCPPacket(
    webrtc::AsyncPacketSocket* socket,
    const webrtc::ReceivedIpPacket& packet) {
  std::vector<uint8_t> packet_copy(packet.payload().begin(),
                                   packet.payload().end());
  voip_thread_->PostTask([this, packet_copy = std::move(packet_copy)] {
    ReadRTCPPacket(packet_copy);
  });
}

static jlong JNI_VoipClient_CreateClient(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& application_context,
    const jni_zero::JavaRef<jobject>& j_voip_client) {
  return webrtc::NativeToJavaPointer(
      AndroidVoipClient::Create(env, application_context, j_voip_client));
}

}  // namespace webrtc_examples
