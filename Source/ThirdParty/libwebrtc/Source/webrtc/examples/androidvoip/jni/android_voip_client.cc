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
#include <sys/socket.h>

#include <algorithm>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "api/audio_codecs/builtin_audio_decoder_factory.h"
#include "api/audio_codecs/builtin_audio_encoder_factory.h"
#include "api/task_queue/default_task_queue_factory.h"
#include "api/voip/voip_codec.h"
#include "api/voip/voip_engine_factory.h"
#include "api/voip/voip_network.h"
#include "examples/androidvoip/generated_jni/VoipClient_jni.h"
#include "rtc_base/logging.h"
#include "rtc_base/network.h"
#include "rtc_base/socket_server.h"
#include "sdk/android/native_api/audio_device_module/audio_device_android.h"
#include "sdk/android/native_api/jni/java_types.h"
#include "sdk/android/native_api/jni/jvm.h"
#include "sdk/android/native_api/jni/scoped_java_ref.h"

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
rtc::IPAddress QueryDefaultLocalAddress(int family) {
  const char kPublicIPv4Host[] = "8.8.8.8";
  const char kPublicIPv6Host[] = "2001:4860:4860::8888";
  const int kPublicPort = 53;
  std::unique_ptr<rtc::Thread> thread = rtc::Thread::CreateWithSocketServer();

  RTC_DCHECK(thread->socketserver() != nullptr);
  RTC_DCHECK(family == AF_INET || family == AF_INET6);

  std::unique_ptr<rtc::Socket> socket(
      thread->socketserver()->CreateSocket(family, SOCK_DGRAM));
  if (!socket) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    return rtc::IPAddress();
  }

  auto host = family == AF_INET ? kPublicIPv4Host : kPublicIPv6Host;
  if (socket->Connect(rtc::SocketAddress(host, kPublicPort)) < 0) {
    if (socket->GetError() != ENETUNREACH &&
        socket->GetError() != EHOSTUNREACH) {
      RTC_LOG(LS_INFO) << "Connect failed with " << socket->GetError();
    }
    return rtc::IPAddress();
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
  } else if (codec_name == "ILBC") {
    return static_cast<int>(PayloadType::kIlbc);
  }

  RTC_DCHECK_NOTREACHED();
  return -1;
}

}  // namespace

namespace webrtc_examples {

void AndroidVoipClient::Init(
    JNIEnv* env,
    const webrtc::JavaParamRef<jobject>& application_context) {
  webrtc::VoipEngineConfig config;
  config.encoder_factory = webrtc::CreateBuiltinAudioEncoderFactory();
  config.decoder_factory = webrtc::CreateBuiltinAudioDecoderFactory();
  config.task_queue_factory = webrtc::CreateDefaultTaskQueueFactory();
  config.audio_device_module =
      webrtc::CreateJavaAudioDeviceModule(env, application_context.obj());
  config.audio_processing = webrtc::AudioProcessingBuilder().Create();

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
    const webrtc::JavaParamRef<jobject>& application_context,
    const webrtc::JavaParamRef<jobject>& j_voip_client) {
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
  webrtc::ScopedJavaLocalRef<jstring> (*convert_function)(
      JNIEnv*, const std::string&) = &webrtc::NativeToJavaString;
  Java_VoipClient_onGetSupportedCodecsCompleted(
      env_, j_voip_client_, NativeToJavaList(env_, names, convert_function));
}

void AndroidVoipClient::GetLocalIPAddress(JNIEnv* env) {
  RUN_ON_VOIP_THREAD(GetLocalIPAddress, env);

  std::string local_ip_address;
  rtc::IPAddress ipv4_address = QueryDefaultLocalAddress(AF_INET);
  if (!ipv4_address.IsNil()) {
    local_ip_address = ipv4_address.ToString();
  } else {
    rtc::IPAddress ipv6_address = QueryDefaultLocalAddress(AF_INET6);
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
    const webrtc::JavaParamRef<jstring>& j_encoder_string) {
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
    const webrtc::JavaParamRef<jobject>& j_decoder_strings) {
  const std::vector<std::string>& chosen_decoders =
      webrtc::JavaListToNativeVector<std::string, jstring>(
          env, j_decoder_strings, &webrtc::JavaToNativeString);
  voip_thread_->PostTask(
      [this, chosen_decoders] { SetDecoders(chosen_decoders); });
}

void AndroidVoipClient::SetLocalAddress(const std::string& ip_address,
                                        const int port_number) {
  RTC_DCHECK_RUN_ON(voip_thread_.get());

  rtp_local_address_ = rtc::SocketAddress(ip_address, port_number);
  rtcp_local_address_ = rtc::SocketAddress(ip_address, port_number + 1);
}

void AndroidVoipClient::SetLocalAddress(
    JNIEnv* env,
    const webrtc::JavaParamRef<jstring>& j_ip_address_string,
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

  rtp_remote_address_ = rtc::SocketAddress(ip_address, port_number);
  rtcp_remote_address_ = rtc::SocketAddress(ip_address, port_number + 1);
}

void AndroidVoipClient::SetRemoteAddress(
    JNIEnv* env,
    const webrtc::JavaParamRef<jstring>& j_ip_address_string,
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
  channel_ = voip_engine_->Base().CreateChannel(this, absl::nullopt);

  rtp_socket_.reset(rtc::AsyncUDPSocket::Create(voip_thread_->socketserver(),
                                                rtp_local_address_));
  if (!rtp_socket_) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                            /*isSuccessful=*/false);
    return;
  }
  rtp_socket_->SignalReadPacket.connect(
      this, &AndroidVoipClient::OnSignalReadRTPPacket);

  rtcp_socket_.reset(rtc::AsyncUDPSocket::Create(voip_thread_->socketserver(),
                                                 rtcp_local_address_));
  if (!rtcp_socket_) {
    RTC_LOG_ERR(LS_ERROR) << "Socket creation failed";
    Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                            /*isSuccessful=*/false);
    return;
  }
  rtcp_socket_->SignalReadPacket.connect(
      this, &AndroidVoipClient::OnSignalReadRTCPPacket);
  Java_VoipClient_onStartSessionCompleted(env_, j_voip_client_,
                                          /*isSuccessful=*/true);
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

  channel_ = absl::nullopt;
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
                           rtp_remote_address_, rtc::PacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTP packet";
  }
}

bool AndroidVoipClient::SendRtp(rtc::ArrayView<const uint8_t> packet,
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
                            rtcp_remote_address_, rtc::PacketOptions())) {
    RTC_LOG(LS_ERROR) << "Failed to send RTCP packet";
  }
}

bool AndroidVoipClient::SendRtcp(rtc::ArrayView<const uint8_t> packet) {
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
      rtc::ArrayView<const uint8_t>(packet_copy.data(), packet_copy.size()));
  RTC_CHECK(result == webrtc::VoipResult::kOk);
}

void AndroidVoipClient::OnSignalReadRTPPacket(rtc::AsyncPacketSocket* socket,
                                              const char* rtp_packet,
                                              size_t size,
                                              const rtc::SocketAddress& addr,
                                              const int64_t& timestamp) {
  std::vector<uint8_t> packet_copy(rtp_packet, rtp_packet + size);
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
      rtc::ArrayView<const uint8_t>(packet_copy.data(), packet_copy.size()));
  RTC_CHECK(result == webrtc::VoipResult::kOk);
}

void AndroidVoipClient::OnSignalReadRTCPPacket(rtc::AsyncPacketSocket* socket,
                                               const char* rtcp_packet,
                                               size_t size,
                                               const rtc::SocketAddress& addr,
                                               const int64_t& timestamp) {
  std::vector<uint8_t> packet_copy(rtcp_packet, rtcp_packet + size);
  voip_thread_->PostTask([this, packet_copy = std::move(packet_copy)] {
    ReadRTCPPacket(packet_copy);
  });
}

static jlong JNI_VoipClient_CreateClient(
    JNIEnv* env,
    const webrtc::JavaParamRef<jobject>& application_context,
    const webrtc::JavaParamRef<jobject>& j_voip_client) {
  return webrtc::NativeToJavaPointer(
      AndroidVoipClient::Create(env, application_context, j_voip_client));
}

}  // namespace webrtc_examples
