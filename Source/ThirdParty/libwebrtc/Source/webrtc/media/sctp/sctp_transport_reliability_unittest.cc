/*
 *  Copyright (c) 2019 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "media/sctp/sctp_transport.h"

#include <memory>
#include <queue>
#include <string>

#include "media/sctp/sctp_transport_internal.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/random.h"
#include "rtc_base/thread.h"
#include "test/gtest.h"

namespace {

static constexpr int kDefaultTimeout = 10000;  // 10 seconds.
static constexpr int kTransport1Port = 15001;
static constexpr int kTransport2Port = 25002;
static constexpr int kLogPerMessagesCount = 100;

/**
 * An simple packet transport implementation which can be
 * configured to simulate uniform random packet loss and
 * configurable random packet delay and reordering.
 */
class SimulatedPacketTransport final : public rtc::PacketTransportInternal {
 public:
  SimulatedPacketTransport(std::string name,
                           rtc::Thread* transport_thread,
                           uint8_t packet_loss_percents,
                           uint16_t avg_send_delay_millis)
      : transport_name_(name),
        transport_thread_(transport_thread),
        packet_loss_percents_(packet_loss_percents),
        avg_send_delay_millis_(avg_send_delay_millis),
        random_(42) {
    RTC_DCHECK(transport_thread_);
    RTC_DCHECK_LE(packet_loss_percents_, 100);
    RTC_DCHECK_RUN_ON(transport_thread_);
  }

  ~SimulatedPacketTransport() override {
    RTC_DCHECK_RUN_ON(transport_thread_);
    auto destination = destination_.load();
    if (destination != nullptr) {
      invoker_.Flush(destination->transport_thread_);
    }
    invoker_.Flush(transport_thread_);
    destination_ = nullptr;
    SignalWritableState(this);
  }

  const std::string& transport_name() const override { return transport_name_; }

  bool writable() const override { return destination_ != nullptr; }

  bool receiving() const override { return true; }

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags = 0) {
    RTC_DCHECK_RUN_ON(transport_thread_);
    auto destination = destination_.load();
    if (destination == nullptr) {
      return -1;
    }
    if (random_.Rand(100) < packet_loss_percents_) {
      // silent packet loss
      return 0;
    }
    rtc::CopyOnWriteBuffer buffer(data, len);
    auto send_job = [this, flags, buffer = std::move(buffer)] {
      auto destination = destination_.load();
      if (destination == nullptr) {
        return;
      }
      destination->SignalReadPacket(
          destination, reinterpret_cast<const char*>(buffer.data()),
          buffer.size(), rtc::Time(), flags);
    };
    // Introduce random send delay in range [0 .. 2 * avg_send_delay_millis_]
    // millis, which will also work as random packet reordering mechanism.
    uint16_t actual_send_delay = avg_send_delay_millis_;
    int16_t reorder_delay =
        avg_send_delay_millis_ *
        std::min(1.0, std::max(-1.0, random_.Gaussian(0, 0.5)));
    actual_send_delay += reorder_delay;

    if (actual_send_delay > 0) {
      invoker_.AsyncInvokeDelayed<void>(RTC_FROM_HERE,
                                        destination->transport_thread_,
                                        std::move(send_job), actual_send_delay);
    } else {
      invoker_.AsyncInvoke<void>(RTC_FROM_HERE, destination->transport_thread_,
                                 std::move(send_job));
    }
    return 0;
  }

  int SetOption(rtc::Socket::Option opt, int value) override { return 0; }

  bool GetOption(rtc::Socket::Option opt, int* value) override { return false; }

  int GetError() override { return 0; }

  absl::optional<rtc::NetworkRoute> network_route() const override {
    return absl::nullopt;
  }

  void SetDestination(SimulatedPacketTransport* destination) {
    RTC_DCHECK_RUN_ON(transport_thread_);
    if (destination == this) {
      return;
    }
    destination_ = destination;
    SignalWritableState(this);
  }

 private:
  const std::string transport_name_;
  rtc::Thread* const transport_thread_;
  const uint8_t packet_loss_percents_;
  const uint16_t avg_send_delay_millis_;
  std::atomic<SimulatedPacketTransport*> destination_ ATOMIC_VAR_INIT(nullptr);
  rtc::AsyncInvoker invoker_;
  webrtc::Random random_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SimulatedPacketTransport);
};

/**
 * A helper class to send specified number of messages
 * over SctpTransport with SCTP reliability settings
 * provided by user. The reliability settings are specified
 * by passing a template instance of SendDataParams.
 * When .sid field inside SendDataParams is specified to
 * negative value it means that actual .sid will be
 * assigned by sender itself, .sid will be assigned from
 * range [cricket::kMinSctpSid; cricket::kMaxSctpSid].
 * The wide range of sids are used to possibly trigger
 * more execution paths inside usrsctp.
 */
class SctpDataSender final {
 public:
  SctpDataSender(rtc::Thread* thread,
                 cricket::SctpTransport* transport,
                 uint64_t target_messages_count,
                 cricket::SendDataParams send_params,
                 uint32_t sender_id)
      : thread_(thread),
        transport_(transport),
        target_messages_count_(target_messages_count),
        send_params_(send_params),
        sender_id_(sender_id) {
    RTC_DCHECK(thread_);
    RTC_DCHECK(transport_);
  }

  void Start() {
    invoker_.AsyncInvoke<void>(RTC_FROM_HERE, thread_, [this] {
      if (started_) {
        RTC_LOG(LS_INFO) << sender_id_ << " sender is already started";
        return;
      }
      started_ = true;
      SendNextMessage();
    });
  }

  uint64_t BytesSentCount() const { return num_bytes_sent_; }

  uint64_t MessagesSentCount() const { return num_messages_sent_; }

  absl::optional<std::string> GetLastError() {
    absl::optional<std::string> result = absl::nullopt;
    thread_->Invoke<void>(RTC_FROM_HERE,
                          [this, &result] { result = last_error_; });
    return result;
  }

  bool WaitForCompletion(int give_up_after_ms) {
    return sent_target_messages_count_.Wait(give_up_after_ms, kDefaultTimeout);
  }

 private:
  void SendNextMessage() {
    RTC_DCHECK_RUN_ON(thread_);
    if (!started_ || num_messages_sent_ >= target_messages_count_) {
      sent_target_messages_count_.Set();
      return;
    }

    if (num_messages_sent_ % kLogPerMessagesCount == 0) {
      RTC_LOG(LS_INFO) << sender_id_ << " sender will try send message "
                       << (num_messages_sent_ + 1) << " out of "
                       << target_messages_count_;
    }

    cricket::SendDataParams params(send_params_);
    if (params.sid < 0) {
      params.sid = cricket::kMinSctpSid +
                   (num_messages_sent_ % cricket::kMaxSctpStreams);
    }

    cricket::SendDataResult result;
    transport_->SendData(params, payload_, &result);
    switch (result) {
      case cricket::SDR_BLOCK:
        // retry after timeout
        invoker_.AsyncInvokeDelayed<void>(
            RTC_FROM_HERE, thread_,
            rtc::Bind(&SctpDataSender::SendNextMessage, this), 500);
        break;
      case cricket::SDR_SUCCESS:
        // send next
        num_bytes_sent_ += payload_.size();
        ++num_messages_sent_;
        invoker_.AsyncInvoke<void>(
            RTC_FROM_HERE, thread_,
            rtc::Bind(&SctpDataSender::SendNextMessage, this));
        break;
      case cricket::SDR_ERROR:
        // give up
        last_error_ = "SctpTransport::SendData error returned";
        sent_target_messages_count_.Set();
        break;
    }
  }

  rtc::Thread* const thread_;
  cricket::SctpTransport* const transport_;
  const uint64_t target_messages_count_;
  const cricket::SendDataParams send_params_;
  const uint32_t sender_id_;
  rtc::CopyOnWriteBuffer payload_{std::string(1400, '.').c_str(), 1400};
  std::atomic<bool> started_ ATOMIC_VAR_INIT(false);
  rtc::AsyncInvoker invoker_;
  std::atomic<uint64_t> num_messages_sent_ ATOMIC_VAR_INIT(0);
  rtc::Event sent_target_messages_count_{true, false};
  std::atomic<uint64_t> num_bytes_sent_ ATOMIC_VAR_INIT(0);
  absl::optional<std::string> last_error_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpDataSender);
};

/**
 * A helper class which counts number of received messages
 * and bytes over SctpTransport. Also allow waiting until
 * specified number of messages received.
 */
class SctpDataReceiver final : public sigslot::has_slots<> {
 public:
  explicit SctpDataReceiver(uint32_t receiver_id,
                            uint64_t target_messages_count)
      : receiver_id_(receiver_id),
        target_messages_count_(target_messages_count) {}

  void OnDataReceived(const cricket::ReceiveDataParams& params,
                      const rtc::CopyOnWriteBuffer& data) {
    num_bytes_received_ += data.size();
    if (++num_messages_received_ == target_messages_count_) {
      received_target_messages_count_.Set();
    }

    if (num_messages_received_ % kLogPerMessagesCount == 0) {
      RTC_LOG(INFO) << receiver_id_ << " receiver got "
                    << num_messages_received_ << " messages";
    }
  }

  uint64_t MessagesReceivedCount() const { return num_messages_received_; }

  uint64_t BytesReceivedCount() const { return num_bytes_received_; }

  bool WaitForMessagesReceived(int timeout_millis) {
    return received_target_messages_count_.Wait(timeout_millis);
  }

 private:
  std::atomic<uint64_t> num_messages_received_ ATOMIC_VAR_INIT(0);
  std::atomic<uint64_t> num_bytes_received_ ATOMIC_VAR_INIT(0);
  rtc::Event received_target_messages_count_{true, false};
  const uint32_t receiver_id_;
  const uint64_t target_messages_count_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpDataReceiver);
};

/**
 * Simple class to manage set of threads.
 */
class ThreadPool final {
 public:
  explicit ThreadPool(size_t threads_count) : random_(42) {
    RTC_DCHECK(threads_count > 0);
    threads_.reserve(threads_count);
    for (size_t i = 0; i < threads_count; i++) {
      auto thread = rtc::Thread::Create();
      thread->SetName("Thread #" + rtc::ToString(i + 1) + " from Pool", this);
      thread->Start();
      threads_.emplace_back(std::move(thread));
    }
  }

  rtc::Thread* GetRandomThread() {
    return threads_[random_.Rand(0U, threads_.size() - 1)].get();
  }

 private:
  webrtc::Random random_;
  std::vector<std::unique_ptr<rtc::Thread>> threads_;
  RTC_DISALLOW_COPY_AND_ASSIGN(ThreadPool);
};

/**
 * Represents single ping-pong test over SctpTransport.
 * User can specify target number of message for bidirectional
 * send, underlying transport packets loss and average packet delay
 * and SCTP delivery settings.
 */
class SctpPingPong final {
 public:
  SctpPingPong(uint32_t id,
               uint16_t port1,
               uint16_t port2,
               rtc::Thread* transport_thread1,
               rtc::Thread* transport_thread2,
               uint32_t messages_count,
               uint8_t packet_loss_percents,
               uint16_t avg_send_delay_millis,
               cricket::SendDataParams send_params)
      : id_(id),
        port1_(port1),
        port2_(port2),
        transport_thread1_(transport_thread1),
        transport_thread2_(transport_thread2),
        messages_count_(messages_count),
        packet_loss_percents_(packet_loss_percents),
        avg_send_delay_millis_(avg_send_delay_millis),
        send_params_(send_params) {
    RTC_DCHECK(transport_thread1_ != nullptr);
    RTC_DCHECK(transport_thread2_ != nullptr);
  }

  virtual ~SctpPingPong() {
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      data_sender1_.reset();
      sctp_transport1_->SetDtlsTransport(nullptr);
      packet_transport1_->SetDestination(nullptr);
    });
    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      data_sender2_.reset();
      sctp_transport2_->SetDtlsTransport(nullptr);
      packet_transport2_->SetDestination(nullptr);
    });
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      sctp_transport1_.reset();
      data_receiver1_.reset();
      packet_transport1_.reset();
    });
    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      sctp_transport2_.reset();
      data_receiver2_.reset();
      packet_transport2_.reset();
    });
  }

  bool Start() {
    CreateTwoConnectedSctpTransportsWithAllStreams();

    {
      rtc::CritScope cs(&lock_);
      if (!errors_list_.empty()) {
        return false;
      }
    }

    data_sender1_.reset(new SctpDataSender(transport_thread1_,
                                           sctp_transport1_.get(),
                                           messages_count_, send_params_, id_));
    data_sender2_.reset(new SctpDataSender(transport_thread2_,
                                           sctp_transport2_.get(),
                                           messages_count_, send_params_, id_));
    data_sender1_->Start();
    data_sender2_->Start();
    return true;
  }

  std::vector<std::string> GetErrorsList() const {
    std::vector<std::string> result;
    {
      rtc::CritScope cs(&lock_);
      result = errors_list_;
    }
    return result;
  }

  void WaitForCompletion(int32_t timeout_millis) {
    if (data_sender1_ == nullptr) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 is not created");
      return;
    }
    if (data_sender2_ == nullptr) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 is not created");
      return;
    }

    if (!data_sender1_->WaitForCompletion(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 failed to complete within " +
                  rtc::ToString(timeout_millis) + " millis");
      return;
    }

    auto sender1_error = data_sender1_->GetLastError();
    if (sender1_error.has_value()) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 error: " + sender1_error.value());
      return;
    }

    if (!data_sender2_->WaitForCompletion(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 failed to complete within " +
                  rtc::ToString(timeout_millis) + " millis");
      return;
    }

    auto sender2_error = data_sender2_->GetLastError();
    if (sender2_error.has_value()) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 error: " + sender1_error.value());
      return;
    }

    if ((data_sender1_->MessagesSentCount() != messages_count_)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 1 sent only " +
                  rtc::ToString(data_sender1_->MessagesSentCount()) +
                  " out of " + rtc::ToString(messages_count_));
      return;
    }

    if ((data_sender2_->MessagesSentCount() != messages_count_)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", sender 2 sent only " +
                  rtc::ToString(data_sender2_->MessagesSentCount()) +
                  " out of " + rtc::ToString(messages_count_));
      return;
    }

    if (!data_receiver1_->WaitForMessagesReceived(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", receiver 1 did not complete within " +
                  rtc::ToString(messages_count_));
      return;
    }

    if (!data_receiver2_->WaitForMessagesReceived(timeout_millis)) {
      ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                  ", receiver 2 did not complete within " +
                  rtc::ToString(messages_count_));
      return;
    }

    if (data_receiver1_->BytesReceivedCount() !=
        data_sender2_->BytesSentCount()) {
      ReportError(
          "SctpPingPong id = " + rtc::ToString(id_) + ", receiver 1 received " +
          rtc::ToString(data_receiver1_->BytesReceivedCount()) +
          " bytes, but sender 2 send " +
          rtc::ToString(rtc::ToString(data_sender2_->BytesSentCount())));
      return;
    }

    if (data_receiver2_->BytesReceivedCount() !=
        data_sender1_->BytesSentCount()) {
      ReportError(
          "SctpPingPong id = " + rtc::ToString(id_) + ", receiver 2 received " +
          rtc::ToString(data_receiver2_->BytesReceivedCount()) +
          " bytes, but sender 1 send " +
          rtc::ToString(rtc::ToString(data_sender1_->BytesSentCount())));
      return;
    }

    RTC_LOG(LS_INFO) << "SctpPingPong id = " << id_ << " is done";
  }

 private:
  void CreateTwoConnectedSctpTransportsWithAllStreams() {
    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport1_.reset(new SimulatedPacketTransport(
          "SctpPingPong id = " + rtc::ToString(id_) + ", packet transport 1",
          transport_thread1_, packet_loss_percents_, avg_send_delay_millis_));
      data_receiver1_.reset(new SctpDataReceiver(id_, messages_count_));
      sctp_transport1_.reset(new cricket::SctpTransport(
          transport_thread1_, packet_transport1_.get()));
      sctp_transport1_->set_debug_name_for_testing("sctp transport 1");

      sctp_transport1_->SignalDataReceived.connect(
          data_receiver1_.get(), &SctpDataReceiver::OnDataReceived);

      for (uint32_t i = cricket::kMinSctpSid; i <= cricket::kMaxSctpSid; i++) {
        if (!sctp_transport1_->OpenStream(i)) {
          ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                      ", sctp transport 1 stream " + rtc::ToString(i) +
                      " failed to open");
          break;
        }
      }
    });

    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport2_.reset(new SimulatedPacketTransport(
          "SctpPingPong id = " + rtc::ToString(id_) + "packet transport 2",
          transport_thread2_, packet_loss_percents_, avg_send_delay_millis_));
      data_receiver2_.reset(new SctpDataReceiver(id_, messages_count_));
      sctp_transport2_.reset(new cricket::SctpTransport(
          transport_thread2_, packet_transport2_.get()));
      sctp_transport2_->set_debug_name_for_testing("sctp transport 2");
      sctp_transport2_->SignalDataReceived.connect(
          data_receiver2_.get(), &SctpDataReceiver::OnDataReceived);

      for (uint32_t i = cricket::kMinSctpSid; i <= cricket::kMaxSctpSid; i++) {
        if (!sctp_transport2_->OpenStream(i)) {
          ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                      ", sctp transport 2 stream " + rtc::ToString(i) +
                      " failed to open");
          break;
        }
      }
    });

    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport1_->SetDestination(packet_transport2_.get());
    });
    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      packet_transport2_->SetDestination(packet_transport1_.get());
    });

    transport_thread1_->Invoke<void>(RTC_FROM_HERE, [this] {
      if (!sctp_transport1_->Start(port1_, port2_,
                                   cricket::kSctpSendBufferSize)) {
        ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                    ", failed to start sctp transport 1");
      }
    });

    transport_thread2_->Invoke<void>(RTC_FROM_HERE, [this] {
      if (!sctp_transport2_->Start(port2_, port1_,
                                   cricket::kSctpSendBufferSize)) {
        ReportError("SctpPingPong id = " + rtc::ToString(id_) +
                    ", failed to start sctp transport 2");
      }
    });
  }

  void ReportError(std::string error) {
    rtc::CritScope cs(&lock_);
    errors_list_.push_back(std::move(error));
  }

  std::unique_ptr<SimulatedPacketTransport> packet_transport1_;
  std::unique_ptr<SimulatedPacketTransport> packet_transport2_;
  std::unique_ptr<SctpDataReceiver> data_receiver1_;
  std::unique_ptr<SctpDataReceiver> data_receiver2_;
  std::unique_ptr<cricket::SctpTransport> sctp_transport1_;
  std::unique_ptr<cricket::SctpTransport> sctp_transport2_;
  std::unique_ptr<SctpDataSender> data_sender1_;
  std::unique_ptr<SctpDataSender> data_sender2_;
  rtc::CriticalSection lock_;
  std::vector<std::string> errors_list_ RTC_GUARDED_BY(lock_);

  const uint32_t id_;
  const uint16_t port1_;
  const uint16_t port2_;
  rtc::Thread* const transport_thread1_;
  rtc::Thread* const transport_thread2_;
  const uint32_t messages_count_;
  const uint8_t packet_loss_percents_;
  const uint16_t avg_send_delay_millis_;
  const cricket::SendDataParams send_params_;
  RTC_DISALLOW_COPY_AND_ASSIGN(SctpPingPong);
};

/**
 * Helper function to calculate max number of milliseconds
 * allowed for test to run based on test configuration.
 */
constexpr int32_t GetExecutionTimeLimitInMillis(uint32_t total_messages,
                                                uint8_t packet_loss_percents) {
  return std::min<int64_t>(
      std::numeric_limits<int32_t>::max(),
      std::max<int64_t>(
          1LL * total_messages * 100 *
              std::max(1, packet_loss_percents * packet_loss_percents),
          kDefaultTimeout));
}

}  // namespace

namespace cricket {

/**
 * The set of tests intended to check usrsctp reliability on
 * stress conditions: multiple sockets, concurrent access,
 * lossy network link. It was observed in the past that
 * usrsctp might misbehave in concurrent environment
 * under load on lossy networks: deadlocks and memory corruption
 * issues might happen in non-basic usage scenarios.
 * It's recommended to run this test whenever usrsctp version
 * used is updated to verify it properly works in stress
 * conditions under higher than usual load.
 * It is also recommended to enable ASAN when these tests
 * are executed, so whenever memory bug is happen inside usrsctp,
 * it will be easier to understand what went wrong with ASAN
 * provided diagnostics information.
 * The tests cases currently disabled by default due to
 * long execution time and due to unresolved issue inside
 * `usrsctp` library detected by try-bots with ThreadSanitizer.
 */
class UsrSctpReliabilityTest : public ::testing::Test {};

/**
 * A simple test which send multiple messages over reliable
 * connection, usefull to verify test infrastructure works.
 * Execution time is less than 1 second.
 */
TEST_F(UsrSctpReliabilityTest,
       DISABLED_AllMessagesAreDeliveredOverReliableConnection) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint8_t packet_loss_percents = 0;
  constexpr uint16_t avg_send_delay_millis = 10;
  constexpr uint32_t messages_count = 100;
  constexpr int32_t wait_timeout =
      GetExecutionTimeLimitInMillis(messages_count, packet_loss_percents);
  static_assert(wait_timeout > 0,
                "Timeout computation must produce positive value");

  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;

  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    avg_send_delay_millis, send_params);
  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(wait_timeout);
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

/**
 * A test to verify that multiple messages can be reliably delivered
 * over lossy network when usrsctp configured to guarantee reliably
 * and in order delivery.
 * The test case is disabled by default because it takes
 * long time to run.
 * Execution time is about 2.5 minutes.
 */
TEST_F(UsrSctpReliabilityTest,
       DISABLED_AllMessagesAreDeliveredOverLossyConnectionReliableAndInOrder) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint8_t packet_loss_percents = 5;
  constexpr uint16_t avg_send_delay_millis = 16;
  constexpr uint32_t messages_count = 10000;
  constexpr int32_t wait_timeout =
      GetExecutionTimeLimitInMillis(messages_count, packet_loss_percents);
  static_assert(wait_timeout > 0,
                "Timeout computation must produce positive value");

  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;

  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    avg_send_delay_millis, send_params);

  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(wait_timeout);
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

/**
 * A test to verify that multiple messages can be reliably delivered
 * over lossy network when usrsctp configured to retransmit lost
 * packets.
 * The test case is disabled by default because it takes
 * long time to run.
 * Execution time is about 2.5 minutes.
 */
TEST_F(UsrSctpReliabilityTest,
       DISABLED_AllMessagesAreDeliveredOverLossyConnectionWithRetries) {
  auto thread1 = rtc::Thread::Create();
  auto thread2 = rtc::Thread::Create();
  thread1->Start();
  thread2->Start();
  constexpr uint8_t packet_loss_percents = 5;
  constexpr uint16_t avg_send_delay_millis = 16;
  constexpr uint32_t messages_count = 10000;
  constexpr int32_t wait_timeout =
      GetExecutionTimeLimitInMillis(messages_count, packet_loss_percents);
  static_assert(wait_timeout > 0,
                "Timeout computation must produce positive value");

  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = false;
  send_params.reliable = false;
  send_params.max_rtx_count = INT_MAX;
  send_params.max_rtx_ms = INT_MAX;

  SctpPingPong test(1, kTransport1Port, kTransport2Port, thread1.get(),
                    thread2.get(), messages_count, packet_loss_percents,
                    avg_send_delay_millis, send_params);

  EXPECT_TRUE(test.Start()) << rtc::join(test.GetErrorsList(), ';');
  test.WaitForCompletion(wait_timeout);
  auto errors_list = test.GetErrorsList();
  EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
}

/**
 * This is kind of reliability stress-test of usrsctp to verify
 * that all messages are delivered when multiple usrsctp
 * sockets used concurrently and underlying transport is lossy.
 *
 * It was observed in the past that in stress condtions usrsctp
 * might encounter deadlock and memory corruption bugs:
 * https://github.com/sctplab/usrsctp/issues/325
 *
 * It is recoomended to run this test whenever usrsctp version
 * used by WebRTC is updated.
 *
 * The test case is disabled by default because it takes
 * long time to run.
 * Execution time of this test is about 1-2 hours.
 */
TEST_F(UsrSctpReliabilityTest,
       DISABLED_AllMessagesAreDeliveredOverLossyConnectionConcurrentTests) {
  ThreadPool pool(16);

  cricket::SendDataParams send_params;
  send_params.sid = -1;
  send_params.ordered = true;
  send_params.reliable = true;
  send_params.max_rtx_count = 0;
  send_params.max_rtx_ms = 0;
  constexpr uint32_t base_sctp_port = 5000;

  // The constants value below were experimentally chosen
  // to have reasonable execution time and to reproduce
  // particular deadlock issue inside usrsctp:
  // https://github.com/sctplab/usrsctp/issues/325
  // The constants values may be adjusted next time
  // some other issue inside usrsctp need to be debugged.
  constexpr uint32_t messages_count = 200;
  constexpr uint8_t packet_loss_percents = 5;
  constexpr uint16_t avg_send_delay_millis = 0;
  constexpr uint32_t parallel_ping_pongs = 16 * 1024;
  constexpr uint32_t total_ping_pong_tests = 16 * parallel_ping_pongs;

  constexpr int32_t wait_timeout = GetExecutionTimeLimitInMillis(
      total_ping_pong_tests * messages_count, packet_loss_percents);
  static_assert(wait_timeout > 0,
                "Timeout computation must produce positive value");

  std::queue<std::unique_ptr<SctpPingPong>> tests;

  for (uint32_t i = 0; i < total_ping_pong_tests; i++) {
    uint32_t port1 =
        base_sctp_port + (2 * i) % (UINT16_MAX - base_sctp_port - 1);

    auto test = std::make_unique<SctpPingPong>(
        i, port1, port1 + 1, pool.GetRandomThread(), pool.GetRandomThread(),
        messages_count, packet_loss_percents, avg_send_delay_millis,
        send_params);

    EXPECT_TRUE(test->Start()) << rtc::join(test->GetErrorsList(), ';');
    tests.emplace(std::move(test));

    while (tests.size() >= parallel_ping_pongs) {
      auto& oldest_test = tests.front();
      oldest_test->WaitForCompletion(wait_timeout);

      auto errors_list = oldest_test->GetErrorsList();
      EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
      tests.pop();
    }
  }

  while (!tests.empty()) {
    auto& oldest_test = tests.front();
    oldest_test->WaitForCompletion(wait_timeout);

    auto errors_list = oldest_test->GetErrorsList();
    EXPECT_TRUE(errors_list.empty()) << rtc::join(errors_list, ';');
    tests.pop();
  }
}

}  // namespace cricket
