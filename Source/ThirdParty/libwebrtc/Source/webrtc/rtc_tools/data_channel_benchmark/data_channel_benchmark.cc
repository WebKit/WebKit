/*
 *  Copyright (c) 2021 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 *  Data Channel Benchmarking tool.
 *
 *  Create a server using: ./data_channel_benchmark --server --port 12345
 *  Start the flow of data from the server to a client using:
 *  ./data_channel_benchmark --port 12345 --transfer_size 100 --packet_size 8196
 *  The throughput is reported on the server console.
 *
 *  The negotiation does not require a 3rd party server and is done over a gRPC
 *  transport. No TURN server is configured, so both peers need to be reachable
 *  using STUN only.
 */
#include <inttypes.h>

#include <charconv>

#include "absl/cleanup/cleanup.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "rtc_base/event.h"
#include "rtc_base/ssl_adapter.h"
#include "rtc_base/thread.h"
#include "rtc_tools/data_channel_benchmark/grpc_signaling.h"
#include "rtc_tools/data_channel_benchmark/peer_connection_client.h"
#include "system_wrappers/include/field_trial.h"

ABSL_FLAG(int, verbose, 0, "verbosity level (0-5)");
ABSL_FLAG(bool, server, false, "Server mode");
ABSL_FLAG(bool, oneshot, true, "Terminate after serving a client");
ABSL_FLAG(std::string, address, "localhost", "Connect to server address");
ABSL_FLAG(uint16_t, port, 0, "Connect to port (0 for random)");
ABSL_FLAG(uint64_t, transfer_size, 2, "Transfer size (MiB)");
ABSL_FLAG(uint64_t, packet_size, 256 * 1024, "Packet size");
ABSL_FLAG(std::string,
          force_fieldtrials,
          "",
          "Field trials control experimental feature code which can be forced. "
          "E.g. running with --force_fieldtrials=WebRTC-FooFeature/Enable/"
          " will assign the group Enable to field trial WebRTC-FooFeature.");

struct SetupMessage {
  size_t packet_size;
  size_t transfer_size;

  std::string ToString() {
    char buffer[64];
    rtc::SimpleStringBuilder sb(buffer);
    sb << packet_size << "," << transfer_size;

    return sb.str();
  }

  static SetupMessage FromString(absl::string_view sv) {
    SetupMessage result;
    auto parameters = rtc::split(sv, ',');
    std::from_chars(parameters[0].data(),
                    parameters[0].data() + parameters[0].size(),
                    result.packet_size, 10);
    std::from_chars(parameters[1].data(),
                    parameters[1].data() + parameters[1].size(),
                    result.transfer_size, 10);
    return result;
  }
};

class DataChannelObserverImpl : public webrtc::DataChannelObserver {
 public:
  explicit DataChannelObserverImpl(webrtc::DataChannelInterface* dc)
      : dc_(dc), bytes_received_(0) {}
  void OnStateChange() override {
    RTC_LOG(LS_INFO) << "State changed to " << dc_->state();
    switch (dc_->state()) {
      case webrtc::DataChannelInterface::DataState::kOpen:
        open_event_.Set();
        break;
      case webrtc::DataChannelInterface::DataState::kClosed:
        closed_event_.Set();
        break;
      default:
        break;
    }
  }
  void OnMessage(const webrtc::DataBuffer& buffer) override {
    bytes_received_ += buffer.data.size();
    if (bytes_received_threshold_ &&
        bytes_received_ >= bytes_received_threshold_) {
      bytes_received_event_.Set();
    }

    if (setup_message_.empty() && !buffer.binary) {
      setup_message_.assign(buffer.data.cdata<char>(), buffer.data.size());
      setup_message_event_.Set();
    }
  }
  void OnBufferedAmountChange(uint64_t sent_data_size) override {
    if (dc_->buffered_amount() <
        webrtc::DataChannelInterface::MaxSendQueueSize() / 2)
      low_buffered_threshold_event_.Set();
    else
      low_buffered_threshold_event_.Reset();
  }

  bool WaitForOpenState() {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kOpen ||
           open_event_.Wait(rtc::Event::kForever);
  }
  bool WaitForClosedState() {
    return dc_->state() == webrtc::DataChannelInterface::DataState::kClosed ||
           closed_event_.Wait(rtc::Event::kForever);
  }

  // Set how many received bytes are required until
  // WaitForBytesReceivedThreshold return true.
  void SetBytesReceivedThreshold(uint64_t bytes_received_threshold) {
    bytes_received_threshold_ = bytes_received_threshold;
    if (bytes_received_ >= bytes_received_threshold_)
      bytes_received_event_.Set();
  }
  // Wait until the received byte count reaches the desired value.
  bool WaitForBytesReceivedThreshold() {
    return (bytes_received_threshold_ &&
            bytes_received_ >= bytes_received_threshold_) ||
           bytes_received_event_.Wait(rtc::Event::kForever);
  }

  bool WaitForLowbufferedThreshold() {
    return low_buffered_threshold_event_.Wait(rtc::Event::kForever);
  }
  std::string SetupMessage() { return setup_message_; }
  bool WaitForSetupMessage() {
    return setup_message_event_.Wait(rtc::Event::kForever);
  }

 private:
  webrtc::DataChannelInterface* dc_;
  rtc::Event open_event_;
  rtc::Event closed_event_;
  rtc::Event bytes_received_event_;
  absl::optional<uint64_t> bytes_received_threshold_;
  uint64_t bytes_received_;
  rtc::Event low_buffered_threshold_event_;
  std::string setup_message_;
  rtc::Event setup_message_event_;
};

int RunServer() {
  bool oneshot = absl::GetFlag(FLAGS_oneshot);
  uint16_t port = absl::GetFlag(FLAGS_port);

  auto signaling_thread = rtc::Thread::Create();
  signaling_thread->Start();
  {
    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());

    auto grpc_server = webrtc::GrpcSignalingServerInterface::Create(
        [factory = rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface>(
             factory)](webrtc::SignalingInterface* signaling) {
          webrtc::PeerConnectionClient client(factory.get(), signaling);
          client.StartPeerConnection();
          auto peer_connection = client.peerConnection();

          // Set up the data channel
          auto dc_or_error =
              peer_connection->CreateDataChannelOrError("benchmark", nullptr);
          auto data_channel = dc_or_error.MoveValue();
          auto data_channel_observer =
              std::make_unique<DataChannelObserverImpl>(data_channel.get());
          data_channel->RegisterObserver(data_channel_observer.get());
          absl::Cleanup unregister_observer(
              [data_channel] { data_channel->UnregisterObserver(); });

          // Wait for a first message from the remote peer.
          // It configures how much data should be sent and how big the packets
          // should be.
          // First message is "packet_size,transfer_size".
          data_channel_observer->WaitForSetupMessage();
          auto parameters =
              SetupMessage::FromString(data_channel_observer->SetupMessage());

          // Wait for the sender and receiver peers to stabilize (send all ACKs)
          // This makes it easier to isolate the sending part when profiling.
          absl::SleepFor(absl::Seconds(1));

          std::string data(parameters.packet_size, '0');
          size_t remaining_data = parameters.transfer_size;

          auto begin_time = webrtc::Clock::GetRealTimeClock()->CurrentTime();

          while (remaining_data) {
            if (remaining_data < data.size())
              data.resize(remaining_data);

            rtc::CopyOnWriteBuffer buffer(data);
            webrtc::DataBuffer data_buffer(buffer, true);
            if (!data_channel->Send(data_buffer)) {
              // If the send() call failed, the buffers are full.
              // We wait until there's more room.
              data_channel_observer->WaitForLowbufferedThreshold();
              continue;
            }
            remaining_data -= buffer.size();
            fprintf(stderr, "Progress: %zu / %zu (%zu%%)\n",
                    (parameters.transfer_size - remaining_data),
                    parameters.transfer_size,
                    (100 - remaining_data * 100 / parameters.transfer_size));
          }

          // Receiver signals the data channel close event when it has received
          // all the data it requested.
          data_channel_observer->WaitForClosedState();

          auto end_time = webrtc::Clock::GetRealTimeClock()->CurrentTime();
          auto duration_ms = (end_time - begin_time).ms<size_t>();
          double throughput = (parameters.transfer_size / 1024. / 1024.) /
                              (duration_ms / 1000.);
          printf("Elapsed time: %zums %gMiB/s\n", duration_ms, throughput);
        },
        port, oneshot);
    grpc_server->Start();

    printf("Server listening on port %d\n", grpc_server->SelectedPort());
    grpc_server->Wait();
  }

  signaling_thread->Quit();
  return 0;
}

int RunClient() {
  uint16_t port = absl::GetFlag(FLAGS_port);
  std::string server_address = absl::GetFlag(FLAGS_address);
  size_t transfer_size = absl::GetFlag(FLAGS_transfer_size) * 1024 * 1024;
  size_t packet_size = absl::GetFlag(FLAGS_packet_size);

  auto signaling_thread = rtc::Thread::Create();
  signaling_thread->Start();
  {
    auto factory = webrtc::PeerConnectionClient::CreateDefaultFactory(
        signaling_thread.get());
    auto grpc_client = webrtc::GrpcSignalingClientInterface::Create(
        server_address + ":" + std::to_string(port));
    webrtc::PeerConnectionClient client(factory.get(),
                                        grpc_client->signaling_client());

    // Set up the callback to receive the data channel from the sender.
    rtc::scoped_refptr<webrtc::DataChannelInterface> data_channel;
    rtc::Event got_data_channel;
    client.SetOnDataChannel(
        [&data_channel, &got_data_channel](
            rtc::scoped_refptr<webrtc::DataChannelInterface> channel) {
          data_channel = channel;
          got_data_channel.Set();
        });

    // Connect to the server.
    if (!grpc_client->Start()) {
      fprintf(stderr, "Failed to connect to server\n");
      return 1;
    }

    // Wait for the data channel to be received
    got_data_channel.Wait(rtc::Event::kForever);

    // DataChannel needs an observer to start draining the read queue
    DataChannelObserverImpl observer(data_channel.get());
    observer.SetBytesReceivedThreshold(transfer_size);
    data_channel->RegisterObserver(&observer);
    absl::Cleanup unregister_observer(
        [data_channel] { data_channel->UnregisterObserver(); });

    // Send a configuration string to the server to tell it to send
    // 'packet_size' bytes packets and send a total of 'transfer_size' MB.
    observer.WaitForOpenState();
    SetupMessage setup_message = {
        .packet_size = packet_size,
        .transfer_size = transfer_size,
    };
    if (!data_channel->Send(webrtc::DataBuffer(setup_message.ToString()))) {
      fprintf(stderr, "Failed to send parameter string\n");
      return 1;
    }

    // Wait until we have received all the data
    observer.WaitForBytesReceivedThreshold();

    // Close the data channel, signaling to the server we have received
    // all the requested data.
    data_channel->Close();
  }

  signaling_thread->Quit();

  return 0;
}

int main(int argc, char** argv) {
  rtc::InitializeSSL();
  absl::ParseCommandLine(argc, argv);

  // Make sure that higher severity number means more logs by reversing the
  // rtc::LoggingSeverity values.
  auto logging_severity =
      std::max(0, rtc::LS_NONE - absl::GetFlag(FLAGS_verbose));
  rtc::LogMessage::LogToDebug(
      static_cast<rtc::LoggingSeverity>(logging_severity));

  bool is_server = absl::GetFlag(FLAGS_server);
  std::string field_trials = absl::GetFlag(FLAGS_force_fieldtrials);

  webrtc::field_trial::InitFieldTrialsFromString(field_trials.c_str());

  return is_server ? RunServer() : RunClient();
}
