/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "rtc_tools/data_channel_benchmark/grpc_signaling.h"

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#include <string>
#include <utility>

#include "api/jsep.h"
#include "api/jsep_ice_candidate.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/peer_connection_signaling.grpc.pb.h"

namespace webrtc {
namespace {

using GrpcSignaling::IceCandidate;
using GrpcSignaling::PeerConnectionSignaling;
using GrpcSignaling::SessionDescription;
using GrpcSignaling::SignalingMessage;

template <class T>
class SessionData : public webrtc::SignalingInterface {
 public:
  SessionData() {}
  explicit SessionData(T* stream) : stream_(stream) {}
  void SetStream(T* stream) { stream_ = stream; }

  void SendIceCandidate(const IceCandidateInterface* candidate) override {
    RTC_LOG(LS_INFO) << "SendIceCandidate";
    std::string serialized_candidate;
    if (!candidate->ToString(&serialized_candidate)) {
      RTC_LOG(LS_ERROR) << "Failed to serialize ICE candidate";
      return;
    }

    SignalingMessage message;
    IceCandidate* proto_candidate = message.mutable_candidate();
    proto_candidate->set_description(serialized_candidate);
    proto_candidate->set_mid(candidate->sdp_mid());
    proto_candidate->set_mline_index(candidate->sdp_mline_index());

    stream_->Write(message);
  }

  void SendDescription(const SessionDescriptionInterface* sdp) override {
    RTC_LOG(LS_INFO) << "SendDescription";

    std::string serialized_sdp;
    sdp->ToString(&serialized_sdp);

    SignalingMessage message;
    if (sdp->GetType() == SdpType::kOffer)
      message.mutable_description()->set_type(SessionDescription::OFFER);
    else if (sdp->GetType() == SdpType::kAnswer)
      message.mutable_description()->set_type(SessionDescription::ANSWER);
    message.mutable_description()->set_content(serialized_sdp);

    stream_->Write(message);
  }

  void OnRemoteDescription(
      std::function<void(std::unique_ptr<SessionDescriptionInterface> sdp)>
          callback) override {
    RTC_LOG(LS_INFO) << "OnRemoteDescription";
    remote_description_callback_ = callback;
  }

  void OnIceCandidate(
      std::function<void(std::unique_ptr<IceCandidateInterface> candidate)>
          callback) override {
    RTC_LOG(LS_INFO) << "OnIceCandidate";
    ice_candidate_callback_ = callback;
  }

  T* stream_;

  std::function<void(std::unique_ptr<webrtc::IceCandidateInterface>)>
      ice_candidate_callback_;
  std::function<void(std::unique_ptr<webrtc::SessionDescriptionInterface>)>
      remote_description_callback_;
};

using ServerSessionData =
    SessionData<grpc::ServerReaderWriter<SignalingMessage, SignalingMessage>>;
using ClientSessionData =
    SessionData<grpc::ClientReaderWriter<SignalingMessage, SignalingMessage>>;

template <class MessageType, class StreamReader, class SessionData>
void ProcessMessages(StreamReader* stream, SessionData* session) {
  MessageType message;

  while (stream->Read(&message)) {
    switch (message.Content_case()) {
      case SignalingMessage::ContentCase::kCandidate: {
        webrtc::SdpParseError error;
        auto jsep_candidate = std::make_unique<webrtc::JsepIceCandidate>(
            message.candidate().mid(), message.candidate().mline_index());
        if (!jsep_candidate->Initialize(message.candidate().description(),
                                        &error)) {
          RTC_LOG(LS_ERROR) << "Failed to deserialize ICE candidate '"
                            << message.candidate().description() << "'";
          RTC_LOG(LS_ERROR)
              << "Error at line " << error.line << ":" << error.description;
          continue;
        }

        session->ice_candidate_callback_(std::move(jsep_candidate));
        break;
      }
      case SignalingMessage::ContentCase::kDescription: {
        auto& description = message.description();
        auto content = description.content();

        auto sdp = webrtc::CreateSessionDescription(
            description.type() == SessionDescription::OFFER
                ? webrtc::SdpType::kOffer
                : webrtc::SdpType::kAnswer,
            description.content());
        session->remote_description_callback_(std::move(sdp));
        break;
      }
      default:
        RTC_DCHECK_NOTREACHED();
    }
  }
}

class GrpcNegotiationServer : public GrpcSignalingServerInterface,
                              public PeerConnectionSignaling::Service {
 public:
  GrpcNegotiationServer(
      std::function<void(webrtc::SignalingInterface*)> callback,
      int port,
      bool oneshot)
      : connect_callback_(std::move(callback)),
        requested_port_(port),
        oneshot_(oneshot) {}
  ~GrpcNegotiationServer() override {
    Stop();
    if (server_stop_thread_)
      server_stop_thread_->Stop();
  }

  void Start() override {
    std::string server_address = "[::]";

    grpc::ServerBuilder builder;
    builder.AddListeningPort(
        server_address + ":" + std::to_string(requested_port_),
        grpc::InsecureServerCredentials(), &selected_port_);
    builder.RegisterService(this);
    server_ = builder.BuildAndStart();
  }

  void Wait() override { server_->Wait(); }

  void Stop() override { server_->Shutdown(); }

  int SelectedPort() override { return selected_port_; }

  grpc::Status Connect(
      grpc::ServerContext* context,
      grpc::ServerReaderWriter<SignalingMessage, SignalingMessage>* stream)
      override {
    if (oneshot_) {
      // Request the termination of the server early so we don't serve another
      // client in parallel.
      server_stop_thread_ = rtc::Thread::Create();
      server_stop_thread_->Start();
      server_stop_thread_->PostTask([this] { Stop(); });
    }

    ServerSessionData session(stream);

    auto reading_thread = rtc::Thread::Create();
    reading_thread->Start();
    reading_thread->PostTask([&session, &stream] {
      ProcessMessages<SignalingMessage>(stream, &session);
    });

    connect_callback_(&session);

    reading_thread->Stop();

    return grpc::Status::OK;
  }

 private:
  std::function<void(webrtc::SignalingInterface*)> connect_callback_;
  int requested_port_;
  int selected_port_;
  bool oneshot_;

  std::unique_ptr<grpc::Server> server_;
  std::unique_ptr<rtc::Thread> server_stop_thread_;
};

class GrpcNegotiationClient : public GrpcSignalingClientInterface {
 public:
  explicit GrpcNegotiationClient(const std::string& server) {
    channel_ = grpc::CreateChannel(server, grpc::InsecureChannelCredentials());
    stub_ = PeerConnectionSignaling::NewStub(channel_);
  }

  ~GrpcNegotiationClient() override {
    context_.TryCancel();
    if (reading_thread_)
      reading_thread_->Stop();
  }

  bool Start() override {
    if (!channel_->WaitForConnected(
            absl::ToChronoTime(absl::Now() + absl::Seconds(3)))) {
      return false;
    }

    stream_ = stub_->Connect(&context_);
    session_.SetStream(stream_.get());

    reading_thread_ = rtc::Thread::Create();
    reading_thread_->Start();
    reading_thread_->PostTask([this] {
      ProcessMessages<SignalingMessage>(stream_.get(), &session_);
    });

    return true;
  }

  webrtc::SignalingInterface* signaling_client() override { return &session_; }

 private:
  std::shared_ptr<grpc::Channel> channel_;
  std::unique_ptr<PeerConnectionSignaling::Stub> stub_;
  std::unique_ptr<rtc::Thread> reading_thread_;
  grpc::ClientContext context_;
  std::unique_ptr<
      ::grpc::ClientReaderWriter<SignalingMessage, SignalingMessage>>
      stream_;
  ClientSessionData session_;
};
}  // namespace

std::unique_ptr<GrpcSignalingServerInterface>
GrpcSignalingServerInterface::Create(
    std::function<void(webrtc::SignalingInterface*)> callback,
    int port,
    bool oneshot) {
  return std::make_unique<GrpcNegotiationServer>(std::move(callback), port,
                                                 oneshot);
}

std::unique_ptr<GrpcSignalingClientInterface>
GrpcSignalingClientInterface::Create(const std::string& server) {
  return std::make_unique<GrpcNegotiationClient>(server);
}

}  // namespace webrtc
