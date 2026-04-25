/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/ssl_stream_adapter.h"

#ifdef OPENSSL_IS_BORINGSSL
#include <openssl/digest.h>
#else
#include <openssl/evp.h>  // IWYU pragma: keep
#endif
#include <openssl/sha.h>
#include <openssl/ssl.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/crypto/crypto_options.h"
#include "api/field_trials.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/test/rtc_error_matchers.h"
#include "api/units/time_delta.h"
#include "rtc_base/buffer.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/logging.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/stream.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/create_test_field_trials.h"
#include "test/gmock.h"
#include "test/gtest.h"
#include "test/wait_until.h"

namespace webrtc {
namespace {

using ::testing::Combine;
using ::testing::NotNull;
using ::testing::tuple;
using ::testing::Values;
using ::testing::WithParamInterface;

// Generated using `openssl genrsa -out key.pem 2048`
constexpr char kRSA_PRIVATE_KEY_PEM[] =
    "-----BEGIN RSA PRI"  // Linebreak to avoid detection of private
    "VATE KEY-----\n"     // keys by linters.
    "MIIEvgIBADANBgkqhkiG9w0BAQEFAASCBKgwggSkAgEAAoIBAQC4XOJ6agj673j+\n"
    "O8sEnPmhVkjDOd858shAa07kVdeRePlE+wU4GUTY0i5JdXF8cUQLTSdKfqsR7f8L\n"
    "jtxhehZk7+OQs5P1VsSQeotr2L0WFBNQZ+cSswLBHt4DjG9vyDJMELwPYkLO/EZw\n"
    "Q1HBgrSSHUHE9mRak2JQzxEqdnj2ssUs+K9kTkYLnzq86dMRGc+TA4TiVA4U065M\n"
    "lwSe95QMJ5OqYBwbNsVF6BTvdnkkNyizunfoGWB8m9gqYIdlmo3uT21OEnF40Pei\n"
    "K5CjvB29IpO6cPmNDR7+vwCy/IeGkXwzvICq/ZrocFNBR5Z4tSm003HX6BbIHtnj\n"
    "tvxVaIeFAgMBAAECggEADxQ3yOPh0qZiCsc4smqlZzr/rgoOdjajhtNQC1BzFnii\n"
    "yK/QTDeS4DoGo6b5roA0HMmFcGweUVPaM6eOYmGiMcTGI9hwPlWHs7p2K065nnPr\n"
    "ZXzuEyM1kzaTWY5zsdyZsot+2jJC/Rt4pmd3KSDn5HiEn9e4OwlJdgsNoB+7ApBW\n"
    "G8UmI9IUYic+xgS0IADJIYFx99bVmjLi7zshQAHVemn15v9GcBTCA7uojxX+FLmR\n"
    "i8nuqUcTqGemE6PaQiX9MahgHU7NJ/gLs9dEeX4tD+8KVkrH/RRbg43eEATkRo8D\n"
    "bO3JZ6MBwVNL6BU4hr+BViXEkHqBa9adoImIWHaLGQKBgQC4zlmHrDm9Ftb6fgsc\n"
    "KXbEphPF/fuw4FJrPXP+0kRvF8AGbGqesBksX/JJCo46jfehNNGHmKFZ7oKMsHbS\n"
    "yZp1/YZlg020ZLJkJz4GGPF1HgaxdV1L6TvIlofKWKKUEyi3RpMhq6w8hb/+mz/C\n"
    "KverTah0EkZjZWwSZa4lQjwCaQKBgQD/YtL6WXiduF94pfVz7MmEoBa00C0rPFaC\n"
    "5TOMVH+W2RbcGyVoPoLmwf1H2lN9v+wzaTRaPeHWs5MwQ4HDUbACXtGQ+I+6VNvo\n"
    "iEo23jIK0hYzFgRGSMK7E0Uj8oBuPdJjkpCM4qqr0p8UHrktUOD8kB3DjdJrbqLm\n"
    "q+9qAWzAvQKBgQCGR5EwDojphuXvnpPuA4bDvjSR4Uj3LRdVypI07o1A903UnQQf\n"
    "h67S2mhOgDf1/d+XJ6yzTMi4cqAzH6lG4au03eDAc9aLI7unIAhmH8uaIJYWbUO7\n"
    "+50v04iZEywWUZF9Ee+oQHfmhfyKQD3klJnew4+Jvxmb8T7EY1NUyTqXOQKBgQDM\n"
    "EpsGZBJm7dqUXQE7Zh5NtWMPjz5YyzlSFXbQjwD5eHW04phMqY8OeDs9fG+1D3Te\n"
    "TBYCemqJlytpqLf7bL4Z1szdbFHlkkO7l5S+LWWNkf0dS12VEDVTKf3Y0MHh1dLV\n"
    "sFuDyOiaro5hlH9if7uY9kxiZGSdZmYTr5Z7fbH6fQKBgF+NKzivaJKz0a7ZCFhR\n"
    "UfjvWrldeRzvyOiq+6nohTy3WNUZ+jSjwXZ7B4HGbHeaTBbsaNeO7aPGNe+Rt3Sr\n"
    "rj6EzpBKk60ukkg49c+X/Rski/RmRosovJv4YUHtafafjAzeMhfU/tdKvjM00p9x\n"
    "yf5MmWCNPsPfGsRZJpnYGvg3\n"
    "-----END RSA PRIVATE KEY-----\n";
#undef RSA_PRIVATE_KEY_HEADER

// Generated using
// `openssl req -new -x509 -key key.pem -out cert.pem -days 365`
// after setting the machine date to something that will ensure the
// certificate is expired.
constexpr char kCERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjTCCAnWgAwIBAgIUTkCy4o8+4W/86RYmgWc8FEhWTzYwDQYJKoZIhvcNAQEL\n"
    "BQAwVjELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM\n"
    "GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDEPMA0GA1UEAwwGV2ViUlRDMB4XDTI0\n"
    "MDkwMzAwNTk0NloXDTI1MDkwMzAwNTk0NlowVjELMAkGA1UEBhMCQVUxEzARBgNV\n"
    "BAgMClNvbWUtU3RhdGUxITAfBgNVBAoMGEludGVybmV0IFdpZGdpdHMgUHR5IEx0\n"
    "ZDEPMA0GA1UEAwwGV2ViUlRDMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKC\n"
    "AQEAuFziemoI+u94/jvLBJz5oVZIwznfOfLIQGtO5FXXkXj5RPsFOBlE2NIuSXVx\n"
    "fHFEC00nSn6rEe3/C47cYXoWZO/jkLOT9VbEkHqLa9i9FhQTUGfnErMCwR7eA4xv\n"
    "b8gyTBC8D2JCzvxGcENRwYK0kh1BxPZkWpNiUM8RKnZ49rLFLPivZE5GC586vOnT\n"
    "ERnPkwOE4lQOFNOuTJcEnveUDCeTqmAcGzbFRegU73Z5JDcos7p36BlgfJvYKmCH\n"
    "ZZqN7k9tThJxeND3oiuQo7wdvSKTunD5jQ0e/r8AsvyHhpF8M7yAqv2a6HBTQUeW\n"
    "eLUptNNx1+gWyB7Z47b8VWiHhQIDAQABo1MwUTAdBgNVHQ4EFgQUlZmkvo2n5ZEa\n"
    "B/GCnl8SMQr8G04wHwYDVR0jBBgwFoAUlZmkvo2n5ZEaB/GCnl8SMQr8G04wDwYD\n"
    "VR0TAQH/BAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAnHDEEEOdPaujj3jVWDnk\n"
    "bxQYQXuymHr5oqIbGSNZaDiK1ZDwui6fywiUjQUgFipC4Gt3EvpEv8b/M9G4Kr3d\n"
    "ET1loApfl6dMRyRym8HydsF4rWs/KmUMpHEcgQzz6ehsX5kqQtStdsAxtTE2QkoY\n"
    "5YbQgTKQ0yrwsagKX8pWv0UmXQASJUa26h5H9YpNNfwHy5PZvQya0719qFd8r2EH\n"
    "JW67EJElwG5qE2N8DStPUjvVsydfbJflvRBjnf9IRuY9rGogeIOTkkkHAOyNWj3V\n"
    "3tZ0r8lKDpUSH6Z5fALuwfEQsWj1qZkZn2ysv1GzEJS2jhS/xPfzOqs8eLVi91lx\n"
    "1A==\n"
    "-----END CERTIFICATE-----\n";

// Google GTS CA 1C3 certificate. Obtained from https://www.webrtc.org
constexpr char kIntCert1[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFljCCA36gAwIBAgINAgO8U1lrNMcY9QFQZjANBgkqhkiG9w0BAQsFADBHMQsw\n"
    "CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU\n"
    "MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMjAwODEzMDAwMDQyWhcNMjcwOTMwMDAw\n"
    "MDQyWjBGMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp\n"
    "Y2VzIExMQzETMBEGA1UEAxMKR1RTIENBIDFDMzCCASIwDQYJKoZIhvcNAQEBBQAD\n"
    "ggEPADCCAQoCggEBAPWI3+dijB43+DdCkH9sh9D7ZYIl/ejLa6T/belaI+KZ9hzp\n"
    "kgOZE3wJCor6QtZeViSqejOEH9Hpabu5dOxXTGZok3c3VVP+ORBNtzS7XyV3NzsX\n"
    "lOo85Z3VvMO0Q+sup0fvsEQRY9i0QYXdQTBIkxu/t/bgRQIh4JZCF8/ZK2VWNAcm\n"
    "BA2o/X3KLu/qSHw3TT8An4Pf73WELnlXXPxXbhqW//yMmqaZviXZf5YsBvcRKgKA\n"
    "gOtjGDxQSYflispfGStZloEAoPtR28p3CwvJlk/vcEnHXG0g/Zm0tOLKLnf9LdwL\n"
    "tmsTDIwZKxeWmLnwi/agJ7u2441Rj72ux5uxiZ0CAwEAAaOCAYAwggF8MA4GA1Ud\n"
    "DwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwEgYDVR0T\n"
    "AQH/BAgwBgEB/wIBADAdBgNVHQ4EFgQUinR/r4XN7pXNPZzQ4kYU83E1HScwHwYD\n"
    "VR0jBBgwFoAU5K8rJnEaK0gnhS9SZizv8IkTcT4waAYIKwYBBQUHAQEEXDBaMCYG\n"
    "CCsGAQUFBzABhhpodHRwOi8vb2NzcC5wa2kuZ29vZy9ndHNyMTAwBggrBgEFBQcw\n"
    "AoYkaHR0cDovL3BraS5nb29nL3JlcG8vY2VydHMvZ3RzcjEuZGVyMDQGA1UdHwQt\n"
    "MCswKaAnoCWGI2h0dHA6Ly9jcmwucGtpLmdvb2cvZ3RzcjEvZ3RzcjEuY3JsMFcG\n"
    "A1UdIARQME4wOAYKKwYBBAHWeQIFAzAqMCgGCCsGAQUFBwIBFhxodHRwczovL3Br\n"
    "aS5nb29nL3JlcG9zaXRvcnkvMAgGBmeBDAECATAIBgZngQwBAgIwDQYJKoZIhvcN\n"
    "AQELBQADggIBAIl9rCBcDDy+mqhXlRu0rvqrpXJxtDaV/d9AEQNMwkYUuxQkq/BQ\n"
    "cSLbrcRuf8/xam/IgxvYzolfh2yHuKkMo5uhYpSTld9brmYZCwKWnvy15xBpPnrL\n"
    "RklfRuFBsdeYTWU0AIAaP0+fbH9JAIFTQaSSIYKCGvGjRFsqUBITTcFTNvNCCK9U\n"
    "+o53UxtkOCcXCb1YyRt8OS1b887U7ZfbFAO/CVMkH8IMBHmYJvJh8VNS/UKMG2Yr\n"
    "PxWhu//2m+OBmgEGcYk1KCTd4b3rGS3hSMs9WYNRtHTGnXzGsYZbr8w0xNPM1IER\n"
    "lQCh9BIiAfq0g3GvjLeMcySsN1PCAJA/Ef5c7TaUEDu9Ka7ixzpiO2xj2YC/WXGs\n"
    "Yye5TBeg2vZzFb8q3o/zpWwygTMD0IZRcZk0upONXbVRWPeyk+gB9lm+cZv9TSjO\n"
    "z23HFtz30dZGm6fKa+l3D/2gthsjgx0QGtkJAITgRNOidSOzNIb2ILCkXhAd4FJG\n"
    "AJ2xDx8hcFH1mt0G/FX0Kw4zd8NLQsLxdxP8c4CU6x+7Nz/OAipmsHMdMqUybDKw\n"
    "juDEI/9bfU1lcKwrmz3O2+BtjjKAvpafkmO8l7tdufThcV4q5O8DIrGKZTqPwJNl\n"
    "1IXNDw9bg1kWRxYtnCQ6yICmJhSFm/Y3m6xv+cXDBlHz4n/FsRC6UfTd\n"
    "-----END CERTIFICATE-----\n";

// Google GTS Root R1 certificate. Obtained from https://www.webrtc.org
constexpr char kCACert[] =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIFWjCCA0KgAwIBAgIQbkepxUtHDA3sM9CJuRz04TANBgkqhkiG9w0BAQwFADBH\n"
    "MQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExM\n"
    "QzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIy\n"
    "MDAwMDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNl\n"
    "cnZpY2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEB\n"
    "AQUAA4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaM\n"
    "f/vo27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vX\n"
    "mX7wCl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7\n"
    "zUjwTcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0P\n"
    "fyblqAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtc\n"
    "vfaHszVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4\n"
    "Zor8Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUsp\n"
    "zBmkMiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOO\n"
    "Rc92wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYW\n"
    "k70paDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+\n"
    "DVrNVjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgF\n"
    "lQIDAQABo0IwQDAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB/zAdBgNV\n"
    "HQ4EFgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBADiW\n"
    "Cu49tJYeX++dnAsznyvgyv3SjgofQXSlfKqE1OXyHuY3UjKcC9FhHb8owbZEKTV1\n"
    "d5iyfNm9dKyKaOOpMQkpAWBz40d8U6iQSifvS9efk+eCNs6aaAyC58/UEBZvXw6Z\n"
    "XPYfcX3v73svfuo21pdwCxXu11xWajOl40k4DLh9+42FpLFZXvRq4d2h9mREruZR\n"
    "gyFmxhE+885H7pwoHyXa/6xmld01D1zvICxi/ZG6qcz8WpyTgYMpl0p8WnK0OdC3\n"
    "d8t5/Wk6kjftbjhlRn7pYL15iJdfOBL07q9bgsiG1eGZbYwE8na6SfZu6W0eX6Dv\n"
    "J4J2QPim01hcDyxC2kLGe4g0x8HYRZvBPsVhHdljUEn2NIVq4BjFbkerQUIpm/Zg\n"
    "DdIx02OYI5NaAIFItO/Nis3Jz5nu2Z6qNuFoS3FJFDYoOj0dzpqPJeaAcWErtXvM\n"
    "+SUWgeExX6GjfhaknBZqlxi9dnKlC54dNuYvoS++cJEPqOba+MSSQGwlfnuzCdyy\n"
    "F62ARPBopY+Udf90WuioAnwMCeKpSwughQtiue+hMZL77/ZRBIls6Kl0obsXs7X9\n"
    "SQ98POyDGCBDTtWTurQ0sR8WNh8M5mQ5Fkzc4P4dyKliPUDqysU0ArSuiYgzNdws\n"
    "E3PYJ/HQcu51OyLemGhmW/HGY0dVHLqlCFF1pkgl\n"
    "-----END CERTIFICATE-----\n";

class SSLStreamAdapterTestBase;

// StreamWrapper is a middle layer between `stream`, which supports a single
// event callback, and test classes in this file that need that event forwarded
// to them. I.e. this class wraps a `stream` object that it delegates all calls
// to, but for the event callback, `StreamWrapper` additionally provides support
// for forwarding event notifications to test classes that call
// `SubscribeStreamEvent()`.
//
// This is needed because in this file, tests connect both client and server
// streams (SSLDummyStream) to the same underlying `stream` objects
// (see CreateClientStream() and CreateServerStream()).
class StreamWrapper : public StreamInterface {
 public:
  explicit StreamWrapper(std::unique_ptr<StreamInterface> stream)
      : stream_(std::move(stream)), flush_count_(0) {
    stream_->SetEventCallback([this](int events, int err) {
      RTC_DCHECK_RUN_ON(&callback_sequence_);
      callbacks_.Send(events, err);
      FireEvent(events, err);
    });
  }

  template <typename F>
  void SubscribeStreamEvent(const void* removal_tag, F&& callback) {
    callbacks_.AddReceiver(removal_tag, std::forward<F>(callback));
  }

  void UnsubscribeStreamEvent(const void* removal_tag) {
    callbacks_.RemoveReceivers(removal_tag);
  }

  StreamState GetState() const override { return stream_->GetState(); }

  void Close() override { stream_->Close(); }
  bool Flush() override {
    flush_count_++;
    return stream_->Flush();
  }
  size_t GetFlushCountForTesting() { return flush_count_; }

  StreamResult Read(ArrayView<uint8_t> buffer,
                    size_t& read,
                    int& error) override {
    return stream_->Read(buffer, read, error);
  }

  StreamResult Write(ArrayView<const uint8_t> data,
                     size_t& written,
                     int& error) override {
    return stream_->Write(data, written, error);
  }

 private:
  const std::unique_ptr<StreamInterface> stream_;
  CallbackList<int, int> callbacks_;
  size_t flush_count_;
};

class SSLDummyStream final : public StreamInterface {
 public:
  SSLDummyStream(SSLStreamAdapterTestBase* test,
                 absl::string_view side,
                 StreamWrapper* in,
                 StreamWrapper* out)
      : test_base_(test), side_(side), in_(in), out_(out), first_packet_(true) {
    RTC_CHECK(thread_);
    RTC_CHECK_NE(in, out);
    in_->SubscribeStreamEvent(
        this, [this](int events, int err) { OnEventIn(events, err); });
    out_->SubscribeStreamEvent(
        this, [this](int events, int err) { OnEventOut(events, err); });
  }

  ~SSLDummyStream() override {
    in_->UnsubscribeStreamEvent(this);
    out_->UnsubscribeStreamEvent(this);
  }

  StreamState GetState() const override { return SS_OPEN; }

  StreamResult Read(ArrayView<uint8_t> buffer,
                    size_t& read,
                    int& error) override {
    StreamResult r;

    r = in_->Read(buffer, read, error);
    if (r == SR_BLOCK)
      return SR_BLOCK;
    if (r == SR_EOS)
      return SR_EOS;

    if (r != SR_SUCCESS) {
      ADD_FAILURE();
      return SR_ERROR;
    }

    return SR_SUCCESS;
  }

  // Catch readability events on in and pass them up.
  void OnEventIn(int sig, int err) {
    int mask = (SE_READ | SE_CLOSE);

    if (sig & mask) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStream::OnEventIn side=" << side_
                          << " sig=" << sig << " forwarding upward";
      PostEvent(sig & mask, 0);
    }
  }

  // Catch writeability events on out and pass them up.
  void OnEventOut(int sig, int err) {
    if (sig & SE_WRITE) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStream::OnEventOut side=" << side_
                          << " sig=" << sig << " forwarding upward";

      PostEvent(sig & SE_WRITE, 0);
    }
  }

  // Write to the outgoing FifoBuffer
  StreamResult WriteData(ArrayView<const uint8_t> data,
                         size_t& written,
                         int& error) {
    return out_->Write(data, written, error);
  }

  StreamResult Write(ArrayView<const uint8_t> data,
                     size_t& written,
                     int& error) override;

  void Close() override {
    RTC_LOG(LS_INFO) << "Closing outbound stream";
    out_->Close();
  }
  bool Flush() override { return in_->Flush(); }

 private:
  void PostEvent(int events, int err) {
    thread_->PostTask(SafeTask(task_safety_.flag(), [this, events, err]() {
      RTC_DCHECK_RUN_ON(&callback_sequence_);
      FireEvent(events, err);
    }));
  }

  ScopedTaskSafety task_safety_;
  Thread* const thread_ = Thread::Current();
  SSLStreamAdapterTestBase* test_base_;
  const std::string side_;
  StreamWrapper* const in_;
  StreamWrapper* const out_;
  bool first_packet_;
};

class BufferQueueStream : public StreamInterface {
 public:
  BufferQueueStream(size_t capacity, size_t default_size)
      : buffer_(capacity, default_size) {}

  // Implementation of abstract StreamInterface methods.

  // A buffer queue stream is always "open".
  StreamState GetState() const override { return SS_OPEN; }

  // Reading a buffer queue stream will either succeed or block.
  StreamResult Read(ArrayView<uint8_t> buffer,
                    size_t& read,
                    int& error) override {
    const bool was_writable = buffer_.is_writable();
    if (!buffer_.ReadFront(buffer.data(), buffer.size(), &read))
      return SR_BLOCK;

    if (!was_writable)
      NotifyWritableForTest();

    return SR_SUCCESS;
  }

  // Writing to a buffer queue stream will either succeed or block.
  StreamResult Write(ArrayView<const uint8_t> data,
                     size_t& written,
                     int& error) override {
    const bool was_readable = buffer_.is_readable();
    if (!buffer_.WriteBack(data.data(), data.size(), &written))
      return SR_BLOCK;

    if (!was_readable)
      NotifyReadableForTest();

    return SR_SUCCESS;
  }

  // A buffer queue stream can not be closed.
  void Close() override {}
  bool Flush() { return false; }

 protected:
  void NotifyReadableForTest() { PostEvent(SE_READ, 0); }
  void NotifyWritableForTest() { PostEvent(SE_WRITE, 0); }

 private:
  void PostEvent(int events, int err) {
    thread_->PostTask(SafeTask(task_safety_.flag(), [this, events, err]() {
      RTC_DCHECK_RUN_ON(&callback_sequence_);
      FireEvent(events, err);
    }));
  }

  Thread* const thread_ = Thread::Current();
  ScopedTaskSafety task_safety_;
  BufferQueue buffer_;
};

constexpr int kBufferCapacity = 1;
constexpr size_t kDefaultBufferSize = 2048;

class SSLStreamAdapterTestBase : public ::testing::Test {
 public:
  SSLStreamAdapterTestBase(absl::string_view client_cert_pem,
                           absl::string_view client_private_key_pem,
                           bool dtls,
                           KeyParams client_key_type = KeyParams(KT_DEFAULT),
                           KeyParams server_key_type = KeyParams(KT_DEFAULT),
                           std::pair<std::string, size_t> digest =
                               std::make_pair(DIGEST_SHA_256,
                                              SHA256_DIGEST_LENGTH))
      : client_cert_pem_(client_cert_pem),
        client_private_key_pem_(client_private_key_pem),
        client_key_type_(client_key_type),
        server_key_type_(server_key_type),
        digest_algorithm_(digest.first),
        digest_length_(digest.second),
        delay_(0),
        mtu_(1460),
        loss_(0),
        lose_first_packet_(false),
        damage_(false),
        dtls_(dtls),
        handshake_wait_(TimeDelta::Millis(5000)),
        identities_set_(false) {
    // Set use of the test RNG to get predictable loss patterns.
    SetRandomTestMode(true);
  }

  ~SSLStreamAdapterTestBase() override {
    // Put it back for the next test.
    SetRandomTestMode(false);
  }

  void SetUp() override {
    InitializeClientAndServerStreams();

    std::unique_ptr<SSLIdentity> client_identity;
    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity = SSLIdentity::CreateFromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity = SSLIdentity::Create("client", client_key_type_);
    }
    auto server_identity = SSLIdentity::Create("server", server_key_type_);

    client_ssl_->SetIdentity(std::move(client_identity));
    server_ssl_->SetIdentity(std::move(server_identity));
  }

  void TearDown() override {
    client_ssl_.reset(nullptr);
    server_ssl_.reset(nullptr);
  }

  virtual std::unique_ptr<StreamInterface> CreateClientStream() = 0;
  virtual std::unique_ptr<StreamInterface> CreateServerStream() = 0;

  void InitializeClientAndServerStreams(
      absl::string_view client_experiment = "",
      absl::string_view server_experiment = "") {
    // Note: `client_ssl_` and `server_ssl_` may be non-nullptr.

    // The field trials are read when the OpenSSLStreamAdapter is initialized.
    {
      FieldTrials trial = CreateTestFieldTrials(client_experiment);
      client_ssl_ =
          SSLStreamAdapter::Create(CreateClientStream(), nullptr, &trial);
    }
    {
      FieldTrials trial = CreateTestFieldTrials(server_experiment);
      server_ssl_ =
          SSLStreamAdapter::Create(CreateServerStream(), nullptr, &trial);
    }
    client_ssl_->SetEventCallback(
        [this](int events, int err) { OnClientEvent(events, err); });
    server_ssl_->SetEventCallback(
        [this](int events, int err) { OnServerEvent(events, err); });
  }

  // Recreate the client/server identities with the specified validity period.
  // `not_before` and `not_after` are offsets from the current time in number
  // of seconds.
  void ResetIdentitiesWithValidity(int not_before, int not_after) {
    InitializeClientAndServerStreams();

    time_t now = time(nullptr);

    SSLIdentityParams client_params;
    client_params.key_params = KeyParams(KT_DEFAULT);
    client_params.common_name = "client";
    client_params.not_before = now + not_before;
    client_params.not_after = now + not_after;
    auto client_identity = SSLIdentity::CreateForTest(client_params);

    SSLIdentityParams server_params;
    server_params.key_params = KeyParams(KT_DEFAULT);
    server_params.common_name = "server";
    server_params.not_before = now + not_before;
    server_params.not_after = now + not_after;
    auto server_identity = SSLIdentity::CreateForTest(server_params);

    client_ssl_->SetIdentity(std::move(client_identity));
    server_ssl_->SetIdentity(std::move(server_identity));
  }

  void SetPeerIdentitiesByDigest(bool correct, bool expect_success) {
    Buffer server_digest(0, EVP_MAX_MD_SIZE);
    Buffer client_digest(0, EVP_MAX_MD_SIZE);
    SSLPeerCertificateDigestError err;
    SSLPeerCertificateDigestError expected_err =
        expect_success ? SSLPeerCertificateDigestError::NONE
                       : SSLPeerCertificateDigestError::VERIFICATION_FAILED;

    RTC_LOG(LS_INFO) << "Setting peer identities by digest";
    RTC_DCHECK(server_identity());
    RTC_DCHECK(client_identity());

    ASSERT_TRUE(server_identity()->certificate().ComputeDigest(
        digest_algorithm_, server_digest));
    ASSERT_TRUE(client_identity()->certificate().ComputeDigest(
        digest_algorithm_, client_digest));

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for server cert";
      server_digest[0]++;
    }
    err =
        client_ssl_->SetPeerCertificateDigest(digest_algorithm_, server_digest);
    EXPECT_EQ(expected_err, err);

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for client cert";
      client_digest[0]++;
    }
    err =
        server_ssl_->SetPeerCertificateDigest(digest_algorithm_, client_digest);
    EXPECT_EQ(expected_err, err);

    identities_set_ = true;
  }

  void SetupProtocolVersions(SSLProtocolVersion server_version,
                             SSLProtocolVersion client_version) {
    server_ssl_->SetMaxProtocolVersion(server_version);
    client_ssl_->SetMaxProtocolVersion(client_version);
  }

  void TestHandshake(bool expect_success = true) {
    if (!dtls_) {
      // Make sure we simulate a reliable network for TLS.
      // This is just a check to make sure that people don't write wrong
      // tests.
      RTC_CHECK_EQ(1460, mtu_);
      RTC_CHECK(!loss_);
      RTC_CHECK(!lose_first_packet_);
    }

    if (!identities_set_)
      SetPeerIdentitiesByDigest(true, true);

    // Start the handshake
    int rv;

    server_ssl_->SetServerRole();
    rv = server_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    rv = client_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    // Now run the handshake
    if (expect_success) {
      EXPECT_THAT(WaitUntil(
                      [&] {
                        return (client_ssl_->GetState() == SS_OPEN) &&
                               (server_ssl_->GetState() == SS_OPEN);
                      },
                      ::testing::IsTrue(),
                      {.timeout = handshake_wait_, .clock = &clock_}),
                  IsRtcOk());
    } else {
      EXPECT_THAT(WaitUntil([&] { return client_ssl_->GetState(); },
                            ::testing::Eq(SS_CLOSED),
                            {.timeout = handshake_wait_, .clock = &clock_}),
                  IsRtcOk());
    }
  }

  // This tests that we give up after one hour.
  // Only works for BoringSSL which allows advancing the fake clock.
  void TestHandshakeTimeout() {
    int64_t time_start = clock_.TimeNanos();
    TimeDelta time_increment = TimeDelta::Millis(1000);

    if (!dtls_) {
      // Make sure we simulate a reliable network for TLS.
      // This is just a check to make sure that people don't write wrong
      // tests.
      RTC_CHECK_EQ(1460, mtu_);
      RTC_CHECK(!loss_);
      RTC_CHECK(!lose_first_packet_);
    }

    if (!identities_set_)
      SetPeerIdentitiesByDigest(true, true);

    // Start the handshake
    int rv;

    server_ssl_->SetServerRole();
    rv = server_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    rv = client_ssl_->StartSSL();
    ASSERT_EQ(0, rv);

    // Now wait for the handshake to timeout (or fail after an hour of simulated
    // time).
    while (client_ssl_->GetState() == SS_OPENING &&
           (TimeDiff(clock_.TimeNanos(), time_start) <
            3600 * kNumNanosecsPerSec)) {
      EXPECT_THAT(WaitUntil(
                      [&] {
                        return !((client_ssl_->GetState() == SS_OPEN) &&
                                 (server_ssl_->GetState() == SS_OPEN));
                      },
                      ::testing::IsTrue(), {.clock = &clock_}),
                  IsRtcOk());
      clock_.AdvanceTime(time_increment);
    }
    EXPECT_EQ(client_ssl_->GetState(), SS_CLOSED);
  }

  // This tests that the handshake can complete before the identity is verified,
  // and the identity will be verified after the fact. It also verifies that
  // packets can't be read or written before the identity has been verified.
  void TestHandshakeWithDelayedIdentity(bool valid_identity) {
    if (!dtls_) {
      // Make sure we simulate a reliable network for TLS.
      // This is just a check to make sure that people don't write wrong
      // tests.
      RTC_CHECK_EQ(1460, mtu_);
      RTC_CHECK(!loss_);
      RTC_CHECK(!lose_first_packet_);
    }

    // Start the handshake
    server_ssl_->SetServerRole();
    ASSERT_EQ(0, server_ssl_->StartSSL());
    ASSERT_EQ(0, client_ssl_->StartSSL());

    // Now run the handshake.
    EXPECT_THAT(WaitUntil(
                    [&] {
                      return client_ssl_->IsTlsConnected() &&
                             server_ssl_->IsTlsConnected();
                    },
                    ::testing::IsTrue(),
                    {.timeout = handshake_wait_, .clock = &clock_}),
                IsRtcOk());

    // Until the identity has been verified, the state should still be
    // SS_OPENING and writes should return SR_BLOCK.
    EXPECT_EQ(SS_OPENING, client_ssl_->GetState());
    EXPECT_EQ(SS_OPENING, server_ssl_->GetState());
    uint8_t packet[1] = {0};
    size_t sent;
    size_t read;
    int error;
    EXPECT_EQ(SR_BLOCK, client_ssl_->Write(packet, sent, error));
    EXPECT_EQ(SR_BLOCK, server_ssl_->Write(packet, sent, error));

    // Collect both of the certificate digests; needs to be done before calling
    // SetPeerCertificateDigest as that may reset the identity.
    Buffer server_digest(0, EVP_MAX_MD_SIZE);
    Buffer client_digest(0, EVP_MAX_MD_SIZE);

    ASSERT_THAT(server_identity(), NotNull());
    ASSERT_TRUE(server_identity()->certificate().ComputeDigest(
        digest_algorithm_, server_digest));

    ASSERT_THAT(client_identity(), NotNull());
    ASSERT_TRUE(client_identity()->certificate().ComputeDigest(
        digest_algorithm_, client_digest));

    if (!valid_identity) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for client/server certs";
      client_digest[0]++;
      server_digest[0]++;
    }

    // Set the peer certificate digest for the client.
    SSLPeerCertificateDigestError err;
    SSLPeerCertificateDigestError expected_err =
        valid_identity ? SSLPeerCertificateDigestError::NONE
                       : SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    err =
        client_ssl_->SetPeerCertificateDigest(digest_algorithm_, server_digest);
    EXPECT_EQ(expected_err, err);
    // State should then transition to SS_OPEN or SS_CLOSED based on validation
    // of the identity.
    if (valid_identity) {
      EXPECT_EQ(SS_OPEN, client_ssl_->GetState());
      // If the client sends a packet while the server still hasn't verified the
      // client identity, the server should continue to return SR_BLOCK.
      EXPECT_EQ(SR_SUCCESS, client_ssl_->Write(packet, sent, error));
      EXPECT_EQ(SR_BLOCK, server_ssl_->Read(packet, read, error));
    } else {
      EXPECT_EQ(SS_CLOSED, client_ssl_->GetState());
    }

    // Set the peer certificate digest for the server.
    err =
        server_ssl_->SetPeerCertificateDigest(digest_algorithm_, client_digest);
    EXPECT_EQ(expected_err, err);
    if (valid_identity) {
      EXPECT_EQ(SS_OPEN, server_ssl_->GetState());
    } else {
      EXPECT_EQ(SS_CLOSED, server_ssl_->GetState());
    }
  }

  StreamResult DataWritten(SSLDummyStream* from,
                           const void* data,
                           size_t data_len,
                           size_t& written,
                           int& error) {
    // Randomly drop loss_ percent of packets
    if (CreateRandomId() % 100 < static_cast<uint32_t>(loss_)) {
      RTC_LOG(LS_VERBOSE) << "Randomly dropping packet, size=" << data_len;
      written = data_len;
      return SR_SUCCESS;
    }
    if (dtls_ && (data_len > mtu_)) {
      RTC_LOG(LS_VERBOSE) << "Dropping packet > mtu, size=" << data_len;
      written = data_len;
      return SR_SUCCESS;
    }
    max_seen_mtu_ = std::max(max_seen_mtu_, data_len);

    // Optionally damage application data (type 23). Note that we don't damage
    // handshake packets and we damage the last byte to keep the header
    // intact but break the MAC.
    uint8_t data0 = static_cast<const unsigned char*>(data)[0];
    if (damage_ && (data0 == 23 || data0 == 47)) {
      std::vector<uint8_t> buf(data_len);
      RTC_LOG(LS_VERBOSE) << "Damaging packet";
      memcpy(&buf[0], data, data_len);
      buf[data_len - 1]++;
      return from->WriteData(MakeArrayView(&buf[0], data_len), written, error);
    }

    return from->WriteData(
        MakeArrayView(reinterpret_cast<const uint8_t*>(data), data_len),
        written, error);
  }

  void SetDelay(int delay) { delay_ = delay; }
  int GetDelay() { return delay_; }

  void SetLoseFirstPacket(bool lose) { lose_first_packet_ = lose; }
  bool GetLoseFirstPacket() { return lose_first_packet_; }

  void SetLoss(int percent) { loss_ = percent; }

  void SetDamage() { damage_ = true; }

  void SetMtu(size_t mtu) { mtu_ = mtu; }
  size_t GetMaxSeenMtu() const { return max_seen_mtu_; }

  void SetHandshakeWait(int wait) { handshake_wait_ = TimeDelta::Millis(wait); }

  void SetDtlsSrtpCryptoSuites(const std::vector<int>& ciphers, bool client) {
    if (client)
      client_ssl_->SetDtlsSrtpCryptoSuites(ciphers);
    else
      server_ssl_->SetDtlsSrtpCryptoSuites(ciphers);
  }

  bool GetDtlsSrtpCryptoSuite(bool client, int* retval) {
    if (client)
      return client_ssl_->GetDtlsSrtpCryptoSuite(retval);
    else
      return server_ssl_->GetDtlsSrtpCryptoSuite(retval);
  }

  std::unique_ptr<SSLCertificate> GetPeerCertificate(bool client) {
    std::unique_ptr<SSLCertChain> chain;
    if (client)
      chain = client_ssl_->GetPeerSSLCertChain();
    else
      chain = server_ssl_->GetPeerSSLCertChain();
    return (chain && chain->GetSize()) ? chain->Get(0).Clone() : nullptr;
  }

  bool GetSslCipherSuite(bool client, int* retval) {
    if (client)
      return client_ssl_->GetSslCipherSuite(retval);
    else
      return server_ssl_->GetSslCipherSuite(retval);
  }

  bool GetSslVersionBytes(bool client, int* version) {
    if (client)
      return client_ssl_->GetSslVersionBytes(version);
    else
      return server_ssl_->GetSslVersionBytes(version);
  }

  // To be implemented by subclasses.
  virtual void WriteData() = 0;
  virtual void ReadData(StreamInterface* stream) = 0;
  virtual void TestTransfer(int size) = 0;

 private:
  void OnClientEvent(int sig, int err) {
    RTC_LOG(LS_VERBOSE) << "SSLStreamAdapterTestBase::OnClientEvent sig="
                        << sig;

    if (sig & SE_READ) {
      ReadData(client_ssl_.get());
    }

    if (sig & SE_WRITE) {
      WriteData();
    }
  }

  void OnServerEvent(int sig, int err) {
    RTC_LOG(LS_VERBOSE) << "SSLStreamAdapterTestBase::OnServerEvent sig="
                        << sig;
    if (sig & SE_READ) {
      ReadData(server_ssl_.get());
    }
  }

 protected:
  SSLIdentity* client_identity() const {
    if (!client_ssl_) {
      return nullptr;
    }
    return client_ssl_->GetIdentityForTesting();
  }
  SSLIdentity* server_identity() const {
    if (!server_ssl_) {
      return nullptr;
    }
    return server_ssl_->GetIdentityForTesting();
  }

  AutoThread main_thread_;
  ScopedFakeClock clock_;
  std::string client_cert_pem_;
  std::string client_private_key_pem_;
  KeyParams client_key_type_;
  KeyParams server_key_type_;
  std::string digest_algorithm_;
  size_t digest_length_;
  std::unique_ptr<SSLStreamAdapter> client_ssl_;
  std::unique_ptr<SSLStreamAdapter> server_ssl_;
  int delay_;
  size_t mtu_;
  size_t max_seen_mtu_ = 0;
  int loss_;
  bool lose_first_packet_;
  bool damage_;
  bool dtls_;
  TimeDelta handshake_wait_;
  bool identities_set_;
};

class SSLStreamAdapterTestDTLSBase : public SSLStreamAdapterTestBase {
 public:
  SSLStreamAdapterTestDTLSBase(KeyParams param1,
                               KeyParams param2,
                               std::pair<std::string, size_t> digest)
      : SSLStreamAdapterTestBase("", "", true, param1, param2, digest),
        packet_size_(1000),
        count_(0),
        sent_(0) {}

  SSLStreamAdapterTestDTLSBase(absl::string_view cert_pem,
                               absl::string_view private_key_pem)
      : SSLStreamAdapterTestBase(cert_pem, private_key_pem, true),
        packet_size_(1000),
        count_(0),
        sent_(0) {}

  std::unique_ptr<StreamInterface> CreateClientStream() final {
    return absl::WrapUnique(
        new SSLDummyStream(this, "c2s", &client_buffer_, &server_buffer_));
  }

  std::unique_ptr<StreamInterface> CreateServerStream() final {
    return absl::WrapUnique(
        new SSLDummyStream(this, "s2c", &server_buffer_, &client_buffer_));
  }

  void WriteData() override {
    uint8_t* packet = new uint8_t[1600];

    while (sent_ < count_) {
      unsigned int rand_state = sent_;
      packet[0] = sent_;
      for (size_t i = 1; i < packet_size_; i++) {
        // This is a simple LC PRNG.  Keep in synch with identical code below.
        rand_state = (rand_state * 251 + 19937) >> 7;
        packet[i] = rand_state & 0xff;
      }

      size_t sent;
      int error;
      StreamResult rv =
          client_ssl_->Write(MakeArrayView(packet, packet_size_), sent, error);
      if (rv == SR_SUCCESS) {
        RTC_LOG(LS_VERBOSE) << "Sent: " << sent_;
        sent_++;
      } else if (rv == SR_BLOCK) {
        RTC_LOG(LS_VERBOSE) << "Blocked...";
        break;
      } else {
        ADD_FAILURE();
        break;
      }
    }

    delete[] packet;
  }

  void ReadData(StreamInterface* stream) final {
    uint8_t buffer[2000];
    size_t bread;
    int err2;
    StreamResult r;

    for (;;) {
      r = stream->Read(buffer, bread, err2);

      if (r == SR_ERROR) {
        // Unfortunately, errors are the way that the stream adapter
        // signals close right now
        stream->Close();
        return;
      }

      if (r == SR_BLOCK)
        break;

      ASSERT_EQ(SR_SUCCESS, r);
      RTC_LOG(LS_VERBOSE) << "Read " << bread;

      // Now parse the datagram
      ASSERT_EQ(packet_size_, bread);
      unsigned char packet_num = buffer[0];

      unsigned int rand_state = packet_num;
      for (size_t i = 1; i < packet_size_; i++) {
        // This is a simple LC PRNG.  Keep in synch with identical code above.
        rand_state = (rand_state * 251 + 19937) >> 7;
        ASSERT_EQ(rand_state & 0xff, buffer[i]);
      }
      received_.insert(packet_num);
    }
  }

  void TestTransfer(int count) override {
    count_ = count;

    WriteData();

    EXPECT_THAT(
        WaitUntil([&] { return sent_; }, ::testing::Eq(count_),
                  {.timeout = TimeDelta::Millis(10000), .clock = &clock_}),
        IsRtcOk());
    RTC_LOG(LS_INFO) << "sent_ == " << sent_;

    if (damage_) {
      clock_.AdvanceTime(TimeDelta::Millis(2000));
      EXPECT_EQ(0U, received_.size());
    } else if (loss_ == 0) {
      EXPECT_THAT(WaitUntil([&] { return received_.size(); },
                            ::testing::Eq(static_cast<size_t>(sent_)),
                            {.clock = &clock_}),
                  IsRtcOk());
    } else {
      RTC_LOG(LS_INFO) << "Sent " << sent_ << " packets; received "
                       << received_.size();
    }
  }

 protected:
  StreamWrapper client_buffer_{
      std::make_unique<BufferQueueStream>(kBufferCapacity, kDefaultBufferSize)};
  StreamWrapper server_buffer_{
      std::make_unique<BufferQueueStream>(kBufferCapacity, kDefaultBufferSize)};

 private:
  size_t packet_size_;
  int count_;
  int sent_;
  std::set<int> received_;
};

webrtc::StreamResult SSLDummyStream::Write(ArrayView<const uint8_t> data,
                                           size_t& written,
                                           int& error) {
  RTC_LOG(LS_VERBOSE) << "Writing to loopback " << data.size();

  if (first_packet_) {
    first_packet_ = false;
    if (test_base_->GetLoseFirstPacket()) {
      RTC_LOG(LS_INFO) << "Losing initial packet of length " << data.size();
      written = data.size();  // Fake successful writing also to writer.
      return SR_SUCCESS;
    }
  }

  return test_base_->DataWritten(this, data.data(), data.size(), written,
                                 error);
}

// Test fixture for certificate chaining. Server will push more than one
// certificate. Note: these tests use RSA keys and SHA1 digests.
class SSLStreamAdapterTestDTLSCertChain : public SSLStreamAdapterTestDTLSBase {
 public:
  SSLStreamAdapterTestDTLSCertChain() : SSLStreamAdapterTestDTLSBase("", "") {}
  void SetUp() override {
    InitializeClientAndServerStreams();
    // These tests apparently need a longer DTLS timeout due to the larger
    // handshake. If the client triggers a resend before the handshake is
    // complete, the handshake fails.
    client_ssl_->SetInitialRetransmissionTimeout(/*timeout_ms=*/1000);
    server_ssl_->SetInitialRetransmissionTimeout(/*timeout_ms=*/1000);

    std::unique_ptr<SSLIdentity> client_identity;
    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity = SSLIdentity::CreateFromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity = SSLIdentity::Create("client", client_key_type_);
    }

    client_ssl_->SetIdentity(std::move(client_identity));
  }
};

TEST_F(SSLStreamAdapterTestDTLSCertChain, TwoCertHandshake) {
  auto server_identity = SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kCACert);
  server_ssl_->SetIdentity(std::move(server_identity));
  TestHandshake();
  std::unique_ptr<SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  // TODO(bugs.webrtc.org/15153): Fix peer_cert_chain to return multiple
  // certificates under OpenSSL. Today it only works with BoringSSL.
#ifdef OPENSSL_IS_BORINGSSL
  ASSERT_EQ(2u, peer_cert_chain->GetSize());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(1).ToPEMString());
#endif
}

TEST_F(SSLStreamAdapterTestDTLSCertChain, TwoCertHandshakeWithCopy) {
  server_ssl_->SetIdentity(SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kCACert));
  TestHandshake();
  std::unique_ptr<SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  // TODO(bugs.webrtc.org/15153): Fix peer_cert_chain to return multiple
  // certificates under OpenSSL. Today it only works with BoringSSL.
#ifdef OPENSSL_IS_BORINGSSL
  ASSERT_EQ(2u, peer_cert_chain->GetSize());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(1).ToPEMString());
#endif
}

TEST_F(SSLStreamAdapterTestDTLSCertChain, ThreeCertHandshake) {
  server_ssl_->SetIdentity(SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kIntCert1 + kCACert));
  TestHandshake();
  std::unique_ptr<SSLCertChain> peer_cert_chain =
      client_ssl_->GetPeerSSLCertChain();
  ASSERT_NE(nullptr, peer_cert_chain);
  EXPECT_EQ(kCERT_PEM, peer_cert_chain->Get(0).ToPEMString());
  // TODO(bugs.webrtc.org/15153): Fix peer_cert_chain to return multiple
  // certificates under OpenSSL. Today it only works with BoringSSL.
#ifdef OPENSSL_IS_BORINGSSL
  ASSERT_EQ(3u, peer_cert_chain->GetSize());
  EXPECT_EQ(kIntCert1, peer_cert_chain->Get(1).ToPEMString());
  EXPECT_EQ(kCACert, peer_cert_chain->Get(2).ToPEMString());
#endif
}

class SSLStreamAdapterTestDTLSHandshake
    : public SSLStreamAdapterTestDTLSBase,
      public WithParamInterface<
          tuple<KeyParams, KeyParams, std::pair<std::string, size_t>>> {
 public:
  SSLStreamAdapterTestDTLSHandshake()
      : SSLStreamAdapterTestDTLSBase(::testing::get<0>(GetParam()),
                                     ::testing::get<1>(GetParam()),
                                     ::testing::get<2>(GetParam())) {}
};

// Test that we can make a handshake work with different parameters.
TEST_P(SSLStreamAdapterTestDTLSHandshake, TestDTLSConnect) {
  TestHandshake();
}

// Test getting the used DTLS ciphers.
// DTLS 1.2 has different cipher suite than 1.3.
TEST_P(SSLStreamAdapterTestDTLSHandshake, TestGetSslCipherSuite) {
  SetupProtocolVersions(SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_12);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// Test different key sizes with SHA-256, then different signature algorithms
// with ECDSA. Two different RSA sizes are tested on the client and server.
// TODO: bugs.webrtc.org/375552698 - these tests are slow in debug builds
// and have caused flakyness in the past with a key size of 2048.
INSTANTIATE_TEST_SUITE_P(
    SSLStreamAdapterTestDTLSHandshakeKeyParameters,
    SSLStreamAdapterTestDTLSHandshake,
    Values(
        std::make_tuple(KeyParams::ECDSA(EC_NIST_P256),
                        KeyParams::RSA(kRsaDefaultModSize, kRsaDefaultExponent),
                        std::make_pair(DIGEST_SHA_256, SHA256_DIGEST_LENGTH)),
        std::make_tuple(KeyParams::RSA(1152, kRsaDefaultExponent),
                        KeyParams::ECDSA(EC_NIST_P256),
                        std::make_pair(DIGEST_SHA_256, SHA256_DIGEST_LENGTH))));

INSTANTIATE_TEST_SUITE_P(
    SSLStreamAdapterTestDTLSHandshakeSignatureAlgorithms,
    SSLStreamAdapterTestDTLSHandshake,
    Combine(Values(KeyParams::ECDSA(EC_NIST_P256)),
            Values(KeyParams::ECDSA(EC_NIST_P256)),
            Values(std::make_pair(DIGEST_SHA_1, SHA_DIGEST_LENGTH),
                   std::make_pair(DIGEST_SHA_224, SHA224_DIGEST_LENGTH),
                   std::make_pair(DIGEST_SHA_256, SHA256_DIGEST_LENGTH),
                   std::make_pair(DIGEST_SHA_384, SHA384_DIGEST_LENGTH),
                   std::make_pair(DIGEST_SHA_512, SHA512_DIGEST_LENGTH))));

// Basic tests done with ECDSA certificates and SHA-256.
class SSLStreamAdapterTestDTLS : public SSLStreamAdapterTestDTLSBase {
 public:
  SSLStreamAdapterTestDTLS()
      : SSLStreamAdapterTestDTLSBase(
            KeyParams::ECDSA(EC_NIST_P256),
            KeyParams::ECDSA(EC_NIST_P256),
            std::make_pair(DIGEST_SHA_256, SHA256_DIGEST_LENGTH)) {}
};

#ifdef OPENSSL_IS_BORINGSSL
#define MAYBE_TestDTLSConnectWithLostFirstPacketNoDelay \
  TestDTLSConnectWithLostFirstPacketNoDelay
#else
#define MAYBE_TestDTLSConnectWithLostFirstPacketNoDelay \
  DISABLED_TestDTLSConnectWithLostFirstPacketNoDelay
#endif
// Test that we can make a handshake work if the first packet in
// each direction is lost. This gives us predictable loss
// rather than having to tune random.
TEST_F(SSLStreamAdapterTestDTLS,
       MAYBE_TestDTLSConnectWithLostFirstPacketNoDelay) {
  SetLoseFirstPacket(true);
  TestHandshake();
  // 2 client flights and 1 resend.
  EXPECT_EQ(client_buffer_.GetFlushCountForTesting(), 3u);
}

#ifdef OPENSSL_IS_BORINGSSL
#define MAYBE_TestDTLSConnectWithLostFirstPacketDelay2s \
  TestDTLSConnectWithLostFirstPacketDelay2s
#else
#define MAYBE_TestDTLSConnectWithLostFirstPacketDelay2s \
  DISABLED_TestDTLSConnectWithLostFirstPacketDelay2s
#endif
// Test a handshake with loss and delay.
TEST_F(SSLStreamAdapterTestDTLS,
       MAYBE_TestDTLSConnectWithLostFirstPacketDelay2s) {
  SetLoseFirstPacket(true);
  SetDelay(2000);
  SetHandshakeWait(20000);
  TestHandshake();
}

// Test a handshake with total loss and timing out.
// Only works in BoringSSL.
#ifdef OPENSSL_IS_BORINGSSL
#define MAYBE_TestDTLSConnectTimeout TestDTLSConnectTimeout
#else
#define MAYBE_TestDTLSConnectTimeout DISABLED_TestDTLSConnectTimeout
#endif
TEST_F(SSLStreamAdapterTestDTLS, MAYBE_TestDTLSConnectTimeout) {
  SetLoss(100);
  TestHandshakeTimeout();
  // 1 flush for the initial send, 13 for the resends.
  EXPECT_EQ(client_buffer_.GetFlushCountForTesting(), 14u);
}

// Test transfer -- trivial
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSTransfer) {
  TestHandshake();
  TestTransfer(100);
}

TEST_F(SSLStreamAdapterTestDTLS, TestSetMTU) {
  SetMtu(400);
  client_ssl_->SetMTU(300);
  server_ssl_->SetMTU(300);
  TestHandshake();
  EXPECT_LE(GetMaxSeenMtu(), 300u);
}

TEST_F(SSLStreamAdapterTestDTLS, TestDTLSTransferWithLoss) {
  TestHandshake();
  SetLoss(10);
  TestTransfer(100);
}

TEST_F(SSLStreamAdapterTestDTLS, TestDTLSTransferWithDamage) {
  SetDamage();  // Must be called first because first packet
                // write happens at end of handshake.
  TestHandshake();
  TestTransfer(100);
}

TEST_F(SSLStreamAdapterTestDTLS, TestDTLSDelayedIdentity) {
  TestHandshakeWithDelayedIdentity(true);
}

TEST_F(SSLStreamAdapterTestDTLS, TestDTLSDelayedIdentityWithBogusDigest) {
  TestHandshakeWithDelayedIdentity(false);
}

// Test DTLS-SRTP with SrtpAes128CmSha1_80
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpAes128CmSha1_80) {
  const std::vector<int> crypto_suites = {kSrtpAes128CmSha1_80};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAes128CmSha1_80);
}

// Test DTLS-SRTP with SrtpAes128CmSha1_32
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpAes128CmSha1_32) {
  const std::vector<int> crypto_suites = {kSrtpAes128CmSha1_32};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAes128CmSha1_32);
}

// Test DTLS-SRTP with incompatible cipher suites -- should not converge.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpIncompatibleCipherSuites) {
  SetDtlsSrtpCryptoSuites({kSrtpAes128CmSha1_80}, true);
  SetDtlsSrtpCryptoSuites({kSrtpAes128CmSha1_32}, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
}

// Test DTLS-SRTP with each side being mixed -- should select the stronger
// cipher.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpMixed) {
  const std::vector<int> crypto_suites = {kSrtpAes128CmSha1_80,
                                          kSrtpAes128CmSha1_32};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAes128CmSha1_80);
}

// Test DTLS-SRTP with SrtpAeadAes128Gcm.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpAeadAes128Gcm) {
  std::vector<int> crypto_suites = {kSrtpAeadAes128Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAeadAes128Gcm);
}

// Test DTLS-SRTP with all GCM-256 ciphers.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCM256) {
  std::vector<int> crypto_suites = {kSrtpAeadAes256Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAeadAes256Gcm);
}

// Test DTLS-SRTP with incompatbile GCM-128/-256 ciphers -- should not converge.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpIncompatibleGcmCipherSuites) {
  SetDtlsSrtpCryptoSuites({kSrtpAeadAes128Gcm}, true);
  SetDtlsSrtpCryptoSuites({kSrtpAeadAes256Gcm}, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
}

// Test DTLS-SRTP with both GCM-128/-256 ciphers -- should select GCM-256.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCMMixed) {
  std::vector<int> crypto_suites = {kSrtpAeadAes256Gcm, kSrtpAeadAes128Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, kSrtpAeadAes256Gcm);
}

// Test SRTP cipher suite lengths.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpKeyAndSaltLengths) {
  int key_len;
  int salt_len;

  ASSERT_FALSE(
      GetSrtpKeyAndSaltLengths(kSrtpInvalidCryptoSuite, &key_len, &salt_len));

  ASSERT_TRUE(
      GetSrtpKeyAndSaltLengths(kSrtpAes128CmSha1_32, &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(
      GetSrtpKeyAndSaltLengths(kSrtpAes128CmSha1_80, &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(
      GetSrtpKeyAndSaltLengths(kSrtpAeadAes128Gcm, &key_len, &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);

  ASSERT_TRUE(
      GetSrtpKeyAndSaltLengths(kSrtpAeadAes256Gcm, &key_len, &salt_len));
  ASSERT_EQ(256 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);
}

// Test the DTLS-SRTP key exporter
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpExporter) {
  const std::vector<int> crypto_suites = {kSrtpAes128CmSha1_80};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);

  TestHandshake();
  int selected_crypto_suite;
  EXPECT_TRUE(GetDtlsSrtpCryptoSuite(/*client=*/false, &selected_crypto_suite));
  int key_len;
  int salt_len;
  ASSERT_TRUE(
      GetSrtpKeyAndSaltLengths(selected_crypto_suite, &key_len, &salt_len));
  ZeroOnFreeBuffer<uint8_t> client_out =
      ZeroOnFreeBuffer<uint8_t>::CreateUninitializedWithSize(
          2 * (key_len + salt_len));
  ZeroOnFreeBuffer<uint8_t> server_out =
      ZeroOnFreeBuffer<uint8_t>::CreateUninitializedWithSize(
          2 * (key_len + salt_len));
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  EXPECT_TRUE(client_ssl_->ExportSrtpKeyingMaterial(client_out));
  EXPECT_TRUE(server_ssl_->ExportSrtpKeyingMaterial(server_out));
  EXPECT_EQ(client_out, server_out);
#pragma clang diagnostic pop

  ZeroOnFreeBuffer<uint8_t> append_client_out;
  ZeroOnFreeBuffer<uint8_t> append_server_out;

  EXPECT_TRUE(client_ssl_->AppendSrtpKeyingMaterial(append_client_out));
  EXPECT_TRUE(server_ssl_->AppendSrtpKeyingMaterial(append_server_out));
  EXPECT_EQ(client_out, append_client_out);
  EXPECT_EQ(client_out, append_server_out);
}

TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpExporterWithAppend) {
  const std::vector<int> crypto_suites = {kSrtpAes128CmSha1_80};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);

  TestHandshake();
  ZeroOnFreeBuffer<uint8_t> client_out;
  ZeroOnFreeBuffer<uint8_t> server_out;

  EXPECT_TRUE(client_ssl_->AppendSrtpKeyingMaterial(client_out));
  EXPECT_TRUE(server_ssl_->AppendSrtpKeyingMaterial(server_out));
  EXPECT_EQ(client_out, server_out);
}

// Test not yet valid certificates are not rejected.
TEST_F(SSLStreamAdapterTestDTLS, TestCertNotYetValid) {
  long one_day = 60 * 60 * 24;
  // Make the certificates not valid until one day later.
  ResetIdentitiesWithValidity(one_day, one_day);
  TestHandshake();
}

// Test expired certificates are not rejected.
TEST_F(SSLStreamAdapterTestDTLS, TestCertExpired) {
  long one_day = 60 * 60 * 24;
  // Make the certificates already expired.
  ResetIdentitiesWithValidity(-one_day, -one_day);
  TestHandshake();
}

class SSLStreamAdapterTestDTLSFromPEMStrings
    : public SSLStreamAdapterTestDTLSBase {
 public:
  SSLStreamAdapterTestDTLSFromPEMStrings()
      : SSLStreamAdapterTestDTLSBase(kCERT_PEM, kRSA_PRIVATE_KEY_PEM) {}
};

// Test data transfer using certs created from strings.
TEST_F(SSLStreamAdapterTestDTLSFromPEMStrings, TestTransfer) {
  TestHandshake();
  TestTransfer(100);
}

// Test getting the remote certificate.
TEST_F(SSLStreamAdapterTestDTLSFromPEMStrings, TestDTLSGetPeerCertificate) {
  // Peer certificates haven't been received yet.
  ASSERT_FALSE(GetPeerCertificate(true));
  ASSERT_FALSE(GetPeerCertificate(false));

  TestHandshake();

  // The client should have a peer certificate after the handshake.
  std::unique_ptr<SSLCertificate> client_peer_cert = GetPeerCertificate(true);
  ASSERT_TRUE(client_peer_cert);

  // It's not kCERT_PEM.
  std::string client_peer_string = client_peer_cert->ToPEMString();
  ASSERT_NE(kCERT_PEM, client_peer_string);

  // The server should have a peer certificate after the handshake.
  std::unique_ptr<SSLCertificate> server_peer_cert = GetPeerCertificate(false);
  ASSERT_TRUE(server_peer_cert);

  // It's kCERT_PEM
  ASSERT_EQ(kCERT_PEM, server_peer_cert->ToPEMString());
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST_F(SSLStreamAdapterTestDTLSFromPEMStrings,
       DeprecatedSetPeerCertificateDigest) {
  SSLPeerCertificateDigestError error;
  // Pass in a wrong length to trigger an error.
  bool ret = client_ssl_->SetPeerCertificateDigest(DIGEST_SHA_256, {},
                                                   /*length=*/0, &error);
  EXPECT_FALSE(ret);
  EXPECT_EQ(error, SSLPeerCertificateDigestError::INVALID_LENGTH);
}
#pragma clang diagnostic pop

struct SSLStreamAdapterTestDTLSHandshakeVersion
    : public SSLStreamAdapterTestDTLS,
      public WithParamInterface<std::tuple<
          /* client*/ SSLProtocolVersion,
          /* server*/ SSLProtocolVersion>> {
  SSLProtocolVersion GetMin(const std::vector<SSLProtocolVersion>& array) {
    SSLProtocolVersion min = array[0];
    for (const auto& e : array) {
      if (static_cast<int>(e) < static_cast<int>(min)) {
        min = e;
      }
    }
    return min;
  }
  uint16_t AsDtlsVersionBytes(SSLProtocolVersion version) {
    switch (version) {
      case SSL_PROTOCOL_DTLS_10:
        return kDtls10VersionBytes;
      case SSL_PROTOCOL_DTLS_12:
        return kDtls12VersionBytes;
      case SSL_PROTOCOL_DTLS_13:
        return kDtls13VersionBytes;
      default:
        break;
    }
    RTC_CHECK(false) << "Unknown version: " << static_cast<int>(version);
  }
};

INSTANTIATE_TEST_SUITE_P(
    SSLStreamAdapterTestDTLSHandshakeVersion,
    SSLStreamAdapterTestDTLSHandshakeVersion,
    Combine(Values(SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_13),
            Values(SSL_PROTOCOL_DTLS_12, SSL_PROTOCOL_DTLS_13)));

TEST_P(SSLStreamAdapterTestDTLSHandshakeVersion, TestGetSslVersionBytes) {
  auto client = ::testing::get<0>(GetParam());
  auto server = ::testing::get<1>(GetParam());
  SetupProtocolVersions(client, server);
  TestHandshake();

  int client_version;
  int server_version;
  ASSERT_TRUE(GetSslVersionBytes(true, &client_version));
  ASSERT_TRUE(GetSslVersionBytes(false, &server_version));

  SSLProtocolVersion expect = GetMin(
      {client, server, SSLStreamAdapter::GetMaxSupportedDTLSProtocolVersion()});

  auto expect_bytes = AsDtlsVersionBytes(expect);
  EXPECT_EQ(client_version, expect_bytes);
  EXPECT_EQ(server_version, expect_bytes);
}

TEST_P(SSLStreamAdapterTestDTLSHandshakeVersion, TestGetSslCipherSuite) {
  auto client = ::testing::get<0>(GetParam());
  auto server = ::testing::get<1>(GetParam());
  SetupProtocolVersions(client, server);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(SSLStreamAdapter::IsAcceptableCipher(server_cipher, KT_DEFAULT));
}

#ifdef OPENSSL_IS_BORINGSSL
TEST_P(SSLStreamAdapterTestDTLSHandshakeVersion, TestGetSslGroupIdWithPqc) {
  auto client_version = ::testing::get<0>(GetParam());
  auto server_version = ::testing::get<1>(GetParam());
  SetupProtocolVersions(client_version, server_version);

  CryptoOptions::EphemeralKeyExchangeCipherGroups groups;
  std::vector<uint16_t> enabled = groups.GetEnabled();
  std::vector<uint16_t> groups_with_pqc;
  if (std::find(
          enabled.begin(), enabled.end(),
          CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519_MLKEM768) ==
      enabled.end()) {
    groups_with_pqc.push_back(
        CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519_MLKEM768);
  }
  for (auto val : enabled) {
    groups_with_pqc.push_back(val);
  }
  RTC_CHECK(client_ssl_->SetSslGroupIds(groups_with_pqc));
  RTC_CHECK(server_ssl_->SetSslGroupIds(groups_with_pqc));

  EXPECT_EQ(client_ssl_->GetSslGroupId(), 0);
  EXPECT_EQ(server_ssl_->GetSslGroupId(), 0);

  TestHandshake();
  if (client_version == SSL_PROTOCOL_DTLS_13 &&
      server_version == SSL_PROTOCOL_DTLS_13) {
    EXPECT_EQ(
        client_ssl_->GetSslGroupId(),
        CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519_MLKEM768);
    EXPECT_EQ(
        server_ssl_->GetSslGroupId(),
        CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519_MLKEM768);
  } else {
    EXPECT_EQ(client_ssl_->GetSslGroupId(),
              CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519);
    EXPECT_EQ(server_ssl_->GetSslGroupId(),
              CryptoOptions::EphemeralKeyExchangeCipherGroups::kX25519);
  }
}
#endif

}  // namespace
}  // namespace webrtc
