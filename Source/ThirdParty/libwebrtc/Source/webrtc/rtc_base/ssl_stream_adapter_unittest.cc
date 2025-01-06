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

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "api/array_view.h"
#include "api/sequence_checker.h"
#include "api/task_queue/pending_task_safety_flag.h"
#include "api/units/time_delta.h"
#include "rtc_base/buffer_queue.h"
#include "rtc_base/callback_list.h"
#include "rtc_base/checks.h"
#include "rtc_base/crypto_random.h"
#include "rtc_base/fake_clock.h"
#include "rtc_base/gunit.h"
#include "rtc_base/logging.h"
#include "rtc_base/memory/fifo_buffer.h"
#include "rtc_base/memory_stream.h"
#include "rtc_base/message_digest.h"
#include "rtc_base/openssl_stream_adapter.h"
#include "rtc_base/ssl_identity.h"
#include "rtc_base/stream.h"
#include "rtc_base/third_party/sigslot/sigslot.h"
#include "rtc_base/thread.h"
#include "rtc_base/time_utils.h"
#include "test/field_trial.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::Combine;
using ::testing::NotNull;
using ::testing::tuple;
using ::testing::Values;
using ::testing::WithParamInterface;
using ::webrtc::SafeTask;

// A private key used for testing, broken into pieces in order to avoid
// issues with Git's checks for private keys in repos.
// Generated using `openssl genrsa -out key.pem 2048`
#define RSA_PRIVATE_KEY_HEADER "-----BEGIN RSA PRIVATE KEY-----\n"

static const char kRSA_PRIVATE_KEY_PEM[] = RSA_PRIVATE_KEY_HEADER
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
static const char kCERT_PEM[] =
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
static const char kIntCert1[] =
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
static const char kCACert[] =
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
class StreamWrapper : public rtc::StreamInterface {
 public:
  explicit StreamWrapper(std::unique_ptr<rtc::StreamInterface> stream)
      : stream_(std::move(stream)) {
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

  rtc::StreamState GetState() const override { return stream_->GetState(); }

  void Close() override { stream_->Close(); }

  rtc::StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                         size_t& read,
                         int& error) override {
    return stream_->Read(buffer, read, error);
  }

  rtc::StreamResult Write(rtc::ArrayView<const uint8_t> data,
                          size_t& written,
                          int& error) override {
    return stream_->Write(data, written, error);
  }

 private:
  const std::unique_ptr<rtc::StreamInterface> stream_;
  webrtc::CallbackList<int, int> callbacks_;
};

class SSLDummyStream final : public rtc::StreamInterface {
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

  rtc::StreamState GetState() const override { return rtc::SS_OPEN; }

  rtc::StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                         size_t& read,
                         int& error) override {
    rtc::StreamResult r;

    r = in_->Read(buffer, read, error);
    if (r == rtc::SR_BLOCK)
      return rtc::SR_BLOCK;
    if (r == rtc::SR_EOS)
      return rtc::SR_EOS;

    if (r != rtc::SR_SUCCESS) {
      ADD_FAILURE();
      return rtc::SR_ERROR;
    }

    return rtc::SR_SUCCESS;
  }

  // Catch readability events on in and pass them up.
  void OnEventIn(int sig, int err) {
    int mask = (rtc::SE_READ | rtc::SE_CLOSE);

    if (sig & mask) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStream::OnEventIn side=" << side_
                          << " sig=" << sig << " forwarding upward";
      PostEvent(sig & mask, 0);
    }
  }

  // Catch writeability events on out and pass them up.
  void OnEventOut(int sig, int err) {
    if (sig & rtc::SE_WRITE) {
      RTC_LOG(LS_VERBOSE) << "SSLDummyStream::OnEventOut side=" << side_
                          << " sig=" << sig << " forwarding upward";

      PostEvent(sig & rtc::SE_WRITE, 0);
    }
  }

  // Write to the outgoing FifoBuffer
  rtc::StreamResult WriteData(rtc::ArrayView<const uint8_t> data,
                              size_t& written,
                              int& error) {
    return out_->Write(data, written, error);
  }

  rtc::StreamResult Write(rtc::ArrayView<const uint8_t> data,
                          size_t& written,
                          int& error) override;

  void Close() override {
    RTC_LOG(LS_INFO) << "Closing outbound stream";
    out_->Close();
  }

 private:
  void PostEvent(int events, int err) {
    thread_->PostTask(SafeTask(task_safety_.flag(), [this, events, err]() {
      RTC_DCHECK_RUN_ON(&callback_sequence_);
      FireEvent(events, err);
    }));
  }

  webrtc::ScopedTaskSafety task_safety_;
  rtc::Thread* const thread_ = rtc::Thread::Current();
  SSLStreamAdapterTestBase* test_base_;
  const std::string side_;
  StreamWrapper* const in_;
  StreamWrapper* const out_;
  bool first_packet_;
};

class BufferQueueStream : public rtc::StreamInterface {
 public:
  BufferQueueStream(size_t capacity, size_t default_size)
      : buffer_(capacity, default_size) {}

  // Implementation of abstract StreamInterface methods.

  // A buffer queue stream is always "open".
  rtc::StreamState GetState() const override { return rtc::SS_OPEN; }

  // Reading a buffer queue stream will either succeed or block.
  rtc::StreamResult Read(rtc::ArrayView<uint8_t> buffer,
                         size_t& read,
                         int& error) override {
    const bool was_writable = buffer_.is_writable();
    if (!buffer_.ReadFront(buffer.data(), buffer.size(), &read))
      return rtc::SR_BLOCK;

    if (!was_writable)
      NotifyWritableForTest();

    return rtc::SR_SUCCESS;
  }

  // Writing to a buffer queue stream will either succeed or block.
  rtc::StreamResult Write(rtc::ArrayView<const uint8_t> data,
                          size_t& written,
                          int& error) override {
    const bool was_readable = buffer_.is_readable();
    if (!buffer_.WriteBack(data.data(), data.size(), &written))
      return rtc::SR_BLOCK;

    if (!was_readable)
      NotifyReadableForTest();

    return rtc::SR_SUCCESS;
  }

  // A buffer queue stream can not be closed.
  void Close() override {}

 protected:
  void NotifyReadableForTest() { PostEvent(rtc::SE_READ, 0); }
  void NotifyWritableForTest() { PostEvent(rtc::SE_WRITE, 0); }

 private:
  void PostEvent(int events, int err) {
    thread_->PostTask(SafeTask(task_safety_.flag(), [this, events, err]() {
      RTC_DCHECK_RUN_ON(&callback_sequence_);
      FireEvent(events, err);
    }));
  }

  rtc::Thread* const thread_ = rtc::Thread::Current();
  webrtc::ScopedTaskSafety task_safety_;
  rtc::BufferQueue buffer_;
};

static const int kBufferCapacity = 1;
static const size_t kDefaultBufferSize = 2048;

class SSLStreamAdapterTestBase : public ::testing::Test,
                                 public sigslot::has_slots<> {
 public:
  SSLStreamAdapterTestBase(
      absl::string_view client_cert_pem,
      absl::string_view client_private_key_pem,
      bool dtls,
      rtc::KeyParams client_key_type = rtc::KeyParams(rtc::KT_DEFAULT),
      rtc::KeyParams server_key_type = rtc::KeyParams(rtc::KT_DEFAULT),
      std::pair<std::string, size_t> digest =
          std::make_pair(rtc::DIGEST_SHA_256, SHA256_DIGEST_LENGTH))
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
        handshake_wait_(5000),
        identities_set_(false) {
    // Set use of the test RNG to get predictable loss patterns.
    rtc::SetRandomTestMode(true);
  }

  ~SSLStreamAdapterTestBase() override {
    // Put it back for the next test.
    rtc::SetRandomTestMode(false);
  }

  void SetUp() override {
    InitializeClientAndServerStreams();

    std::unique_ptr<rtc::SSLIdentity> client_identity;
    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity = rtc::SSLIdentity::CreateFromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity = rtc::SSLIdentity::Create("client", client_key_type_);
    }
    auto server_identity = rtc::SSLIdentity::Create("server", server_key_type_);

    client_ssl_->SetIdentity(std::move(client_identity));
    server_ssl_->SetIdentity(std::move(server_identity));
  }

  void TearDown() override {
    client_ssl_.reset(nullptr);
    server_ssl_.reset(nullptr);
  }

  virtual std::unique_ptr<rtc::StreamInterface> CreateClientStream() = 0;
  virtual std::unique_ptr<rtc::StreamInterface> CreateServerStream() = 0;

  void InitializeClientAndServerStreams(
      absl::string_view client_experiment = "",
      absl::string_view server_experiment = "") {
    // Note: `client_ssl_` and `server_ssl_` may be non-nullptr.

    // The legacy TLS protocols flag is read when the OpenSSLStreamAdapter is
    // initialized, so we set the field trials while constructing the adapters.
    using webrtc::test::ScopedFieldTrials;
    {
      std::unique_ptr<ScopedFieldTrials> trial(
          client_experiment.empty() ? nullptr
                                    : new ScopedFieldTrials(client_experiment));
      client_ssl_ = rtc::SSLStreamAdapter::Create(CreateClientStream());
    }
    {
      std::unique_ptr<ScopedFieldTrials> trial(
          server_experiment.empty() ? nullptr
                                    : new ScopedFieldTrials(server_experiment));
      server_ssl_ = rtc::SSLStreamAdapter::Create(CreateServerStream());
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

    rtc::SSLIdentityParams client_params;
    client_params.key_params = rtc::KeyParams(rtc::KT_DEFAULT);
    client_params.common_name = "client";
    client_params.not_before = now + not_before;
    client_params.not_after = now + not_after;
    auto client_identity = rtc::SSLIdentity::CreateForTest(client_params);

    rtc::SSLIdentityParams server_params;
    server_params.key_params = rtc::KeyParams(rtc::KT_DEFAULT);
    server_params.common_name = "server";
    server_params.not_before = now + not_before;
    server_params.not_after = now + not_after;
    auto server_identity = rtc::SSLIdentity::CreateForTest(server_params);

    client_ssl_->SetIdentity(std::move(client_identity));
    server_ssl_->SetIdentity(std::move(server_identity));
  }

  void SetPeerIdentitiesByDigest(bool correct, bool expect_success) {
    unsigned char server_digest[EVP_MAX_MD_SIZE];
    size_t server_digest_len;
    unsigned char client_digest[EVP_MAX_MD_SIZE];
    size_t client_digest_len;
    bool rv;
    rtc::SSLPeerCertificateDigestError err;
    rtc::SSLPeerCertificateDigestError expected_err =
        expect_success
            ? rtc::SSLPeerCertificateDigestError::NONE
            : rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;

    RTC_LOG(LS_INFO) << "Setting peer identities by digest";
    RTC_DCHECK(server_identity());
    RTC_DCHECK(client_identity());

    rv = server_identity()->certificate().ComputeDigest(
        digest_algorithm_, server_digest, digest_length_, &server_digest_len);
    ASSERT_TRUE(rv);
    rv = client_identity()->certificate().ComputeDigest(
        digest_algorithm_, client_digest, digest_length_, &client_digest_len);
    ASSERT_TRUE(rv);

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for server cert";
      server_digest[0]++;
    }
    rv = client_ssl_->SetPeerCertificateDigest(digest_algorithm_, server_digest,
                                               server_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(expect_success, rv);

    if (!correct) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for client cert";
      client_digest[0]++;
    }
    rv = server_ssl_->SetPeerCertificateDigest(digest_algorithm_, client_digest,
                                               client_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(expect_success, rv);

    identities_set_ = true;
  }

  void SetupProtocolVersions(rtc::SSLProtocolVersion server_version,
                             rtc::SSLProtocolVersion client_version) {
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
      EXPECT_TRUE_SIMULATED_WAIT((client_ssl_->GetState() == rtc::SS_OPEN) &&
                                     (server_ssl_->GetState() == rtc::SS_OPEN),
                                 handshake_wait_, clock_);
    } else {
      EXPECT_TRUE_SIMULATED_WAIT(client_ssl_->GetState() == rtc::SS_CLOSED,
                                 handshake_wait_, clock_);
    }
  }

  // This tests that we give up after 12 DTLS resends.
  // Only works for BoringSSL which allows advancing the fake clock.
  void TestHandshakeTimeout() {
    int64_t time_start = clock_.TimeNanos();
    webrtc::TimeDelta time_increment = webrtc::TimeDelta::Millis(1000);

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
    while (client_ssl_->GetState() == rtc::SS_OPENING &&
           (rtc::TimeDiff(clock_.TimeNanos(), time_start) <
            3600 * rtc::kNumNanosecsPerSec)) {
      EXPECT_TRUE_SIMULATED_WAIT(!((client_ssl_->GetState() == rtc::SS_OPEN) &&
                                   (server_ssl_->GetState() == rtc::SS_OPEN)),
                                 1000, clock_);
      clock_.AdvanceTime(time_increment);
    }
    EXPECT_EQ(client_ssl_->GetState(), rtc::SS_CLOSED);
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
    EXPECT_TRUE_SIMULATED_WAIT(
        client_ssl_->IsTlsConnected() && server_ssl_->IsTlsConnected(),
        handshake_wait_, clock_);

    // Until the identity has been verified, the state should still be
    // SS_OPENING and writes should return SR_BLOCK.
    EXPECT_EQ(rtc::SS_OPENING, client_ssl_->GetState());
    EXPECT_EQ(rtc::SS_OPENING, server_ssl_->GetState());
    uint8_t packet[1];
    size_t sent;
    int error;
    EXPECT_EQ(rtc::SR_BLOCK, client_ssl_->Write(packet, sent, error));
    EXPECT_EQ(rtc::SR_BLOCK, server_ssl_->Write(packet, sent, error));

    // Collect both of the certificate digests; needs to be done before calling
    // SetPeerCertificateDigest as that may reset the identity.
    unsigned char server_digest[EVP_MAX_MD_SIZE];
    size_t server_digest_len;
    unsigned char client_digest[EVP_MAX_MD_SIZE];
    size_t client_digest_len;
    bool rv;

    ASSERT_THAT(server_identity(), NotNull());
    rv = server_identity()->certificate().ComputeDigest(
        digest_algorithm_, server_digest, digest_length_, &server_digest_len);
    ASSERT_TRUE(rv);

    ASSERT_THAT(client_identity(), NotNull());
    rv = client_identity()->certificate().ComputeDigest(
        digest_algorithm_, client_digest, digest_length_, &client_digest_len);
    ASSERT_TRUE(rv);

    if (!valid_identity) {
      RTC_LOG(LS_INFO) << "Setting bogus digest for client/server certs";
      client_digest[0]++;
      server_digest[0]++;
    }

    // Set the peer certificate digest for the client.
    rtc::SSLPeerCertificateDigestError err;
    rtc::SSLPeerCertificateDigestError expected_err =
        valid_identity
            ? rtc::SSLPeerCertificateDigestError::NONE
            : rtc::SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    rv = client_ssl_->SetPeerCertificateDigest(digest_algorithm_, server_digest,
                                               server_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(valid_identity, rv);
    // State should then transition to SS_OPEN or SS_CLOSED based on validation
    // of the identity.
    if (valid_identity) {
      EXPECT_EQ(rtc::SS_OPEN, client_ssl_->GetState());
      // If the client sends a packet while the server still hasn't verified the
      // client identity, the server should continue to return SR_BLOCK.
      int error;
      EXPECT_EQ(rtc::SR_SUCCESS, client_ssl_->Write(packet, sent, error));
      size_t read;
      EXPECT_EQ(rtc::SR_BLOCK, server_ssl_->Read(packet, read, error));
    } else {
      EXPECT_EQ(rtc::SS_CLOSED, client_ssl_->GetState());
    }

    // Set the peer certificate digest for the server.
    rv = server_ssl_->SetPeerCertificateDigest(digest_algorithm_, client_digest,
                                               client_digest_len, &err);
    EXPECT_EQ(expected_err, err);
    EXPECT_EQ(valid_identity, rv);
    if (valid_identity) {
      EXPECT_EQ(rtc::SS_OPEN, server_ssl_->GetState());
    } else {
      EXPECT_EQ(rtc::SS_CLOSED, server_ssl_->GetState());
    }
  }

  rtc::StreamResult DataWritten(SSLDummyStream* from,
                                const void* data,
                                size_t data_len,
                                size_t& written,
                                int& error) {
    // Randomly drop loss_ percent of packets
    if (rtc::CreateRandomId() % 100 < static_cast<uint32_t>(loss_)) {
      RTC_LOG(LS_VERBOSE) << "Randomly dropping packet, size=" << data_len;
      written = data_len;
      return rtc::SR_SUCCESS;
    }
    if (dtls_ && (data_len > mtu_)) {
      RTC_LOG(LS_VERBOSE) << "Dropping packet > mtu, size=" << data_len;
      written = data_len;
      return rtc::SR_SUCCESS;
    }

    // Optionally damage application data (type 23). Note that we don't damage
    // handshake packets and we damage the last byte to keep the header
    // intact but break the MAC.
    if (damage_ && (*static_cast<const unsigned char*>(data) == 23)) {
      std::vector<uint8_t> buf(data_len);

      RTC_LOG(LS_VERBOSE) << "Damaging packet";

      memcpy(&buf[0], data, data_len);
      buf[data_len - 1]++;
      return from->WriteData(rtc::MakeArrayView(&buf[0], data_len), written,
                             error);
    }

    return from->WriteData(
        rtc::MakeArrayView(reinterpret_cast<const uint8_t*>(data), data_len),
        written, error);
  }

  void SetDelay(int delay) { delay_ = delay; }
  int GetDelay() { return delay_; }

  void SetLoseFirstPacket(bool lose) { lose_first_packet_ = lose; }
  bool GetLoseFirstPacket() { return lose_first_packet_; }

  void SetLoss(int percent) { loss_ = percent; }

  void SetDamage() { damage_ = true; }

  void SetMtu(size_t mtu) { mtu_ = mtu; }

  void SetHandshakeWait(int wait) { handshake_wait_ = wait; }

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

  std::unique_ptr<rtc::SSLCertificate> GetPeerCertificate(bool client) {
    std::unique_ptr<rtc::SSLCertChain> chain;
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
  virtual void ReadData(rtc::StreamInterface* stream) = 0;
  virtual void TestTransfer(int size) = 0;

 private:
  void OnClientEvent(int sig, int err) {
    RTC_LOG(LS_VERBOSE) << "SSLStreamAdapterTestBase::OnClientEvent sig="
                        << sig;

    if (sig & rtc::SE_READ) {
      ReadData(client_ssl_.get());
    }

    if (sig & rtc::SE_WRITE) {
      WriteData();
    }
  }

  void OnServerEvent(int sig, int err) {
    RTC_LOG(LS_VERBOSE) << "SSLStreamAdapterTestBase::OnServerEvent sig="
                        << sig;
    if (sig & rtc::SE_READ) {
      ReadData(server_ssl_.get());
    }
  }

 protected:
  rtc::SSLIdentity* client_identity() const {
    if (!client_ssl_) {
      return nullptr;
    }
    return client_ssl_->GetIdentityForTesting();
  }
  rtc::SSLIdentity* server_identity() const {
    if (!server_ssl_) {
      return nullptr;
    }
    return server_ssl_->GetIdentityForTesting();
  }

  rtc::AutoThread main_thread_;
  rtc::ScopedFakeClock clock_;
  std::string client_cert_pem_;
  std::string client_private_key_pem_;
  rtc::KeyParams client_key_type_;
  rtc::KeyParams server_key_type_;
  std::string digest_algorithm_;
  size_t digest_length_;
  std::unique_ptr<rtc::SSLStreamAdapter> client_ssl_;
  std::unique_ptr<rtc::SSLStreamAdapter> server_ssl_;
  int delay_;
  size_t mtu_;
  int loss_;
  bool lose_first_packet_;
  bool damage_;
  bool dtls_;
  int handshake_wait_;
  bool identities_set_;
};

class SSLStreamAdapterTestDTLSBase : public SSLStreamAdapterTestBase {
 public:
  SSLStreamAdapterTestDTLSBase(rtc::KeyParams param1,
                               rtc::KeyParams param2,
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

  std::unique_ptr<rtc::StreamInterface> CreateClientStream() override final {
    return absl::WrapUnique(
        new SSLDummyStream(this, "c2s", &client_buffer_, &server_buffer_));
  }

  std::unique_ptr<rtc::StreamInterface> CreateServerStream() override final {
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
      rtc::StreamResult rv = client_ssl_->Write(
          rtc::MakeArrayView(packet, packet_size_), sent, error);
      if (rv == rtc::SR_SUCCESS) {
        RTC_LOG(LS_VERBOSE) << "Sent: " << sent_;
        sent_++;
      } else if (rv == rtc::SR_BLOCK) {
        RTC_LOG(LS_VERBOSE) << "Blocked...";
        break;
      } else {
        ADD_FAILURE();
        break;
      }
    }

    delete[] packet;
  }

  void ReadData(rtc::StreamInterface* stream) override final {
    uint8_t buffer[2000];
    size_t bread;
    int err2;
    rtc::StreamResult r;

    for (;;) {
      r = stream->Read(buffer, bread, err2);

      if (r == rtc::SR_ERROR) {
        // Unfortunately, errors are the way that the stream adapter
        // signals close right now
        stream->Close();
        return;
      }

      if (r == rtc::SR_BLOCK)
        break;

      ASSERT_EQ(rtc::SR_SUCCESS, r);
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

    EXPECT_TRUE_SIMULATED_WAIT(sent_ == count_, 10000, clock_);
    RTC_LOG(LS_INFO) << "sent_ == " << sent_;

    if (damage_) {
      SIMULATED_WAIT(false, 2000, clock_);
      EXPECT_EQ(0U, received_.size());
    } else if (loss_ == 0) {
      EXPECT_EQ_SIMULATED_WAIT(static_cast<size_t>(sent_), received_.size(),
                               1000, clock_);
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

rtc::StreamResult SSLDummyStream::Write(rtc::ArrayView<const uint8_t> data,
                                        size_t& written,
                                        int& error) {
  RTC_LOG(LS_VERBOSE) << "Writing to loopback " << data.size();

  if (first_packet_) {
    first_packet_ = false;
    if (test_base_->GetLoseFirstPacket()) {
      RTC_LOG(LS_INFO) << "Losing initial packet of length " << data.size();
      written = data.size();  // Fake successful writing also to writer.
      return rtc::SR_SUCCESS;
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

    std::unique_ptr<rtc::SSLIdentity> client_identity;
    if (!client_cert_pem_.empty() && !client_private_key_pem_.empty()) {
      client_identity = rtc::SSLIdentity::CreateFromPEMStrings(
          client_private_key_pem_, client_cert_pem_);
    } else {
      client_identity = rtc::SSLIdentity::Create("client", client_key_type_);
    }

    client_ssl_->SetIdentity(std::move(client_identity));
  }
};

TEST_F(SSLStreamAdapterTestDTLSCertChain, TwoCertHandshake) {
  auto server_identity = rtc::SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kCACert);
  server_ssl_->SetIdentity(std::move(server_identity));
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
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
  server_ssl_->SetIdentity(rtc::SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kCACert));
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
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
  server_ssl_->SetIdentity(rtc::SSLIdentity::CreateFromPEMChainStrings(
      kRSA_PRIVATE_KEY_PEM, std::string(kCERT_PEM) + kIntCert1 + kCACert));
  TestHandshake();
  std::unique_ptr<rtc::SSLCertChain> peer_cert_chain =
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
      public WithParamInterface<tuple<rtc::KeyParams,
                                      rtc::KeyParams,
                                      std::pair<std::string, size_t>>> {
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
// DTLS 1.2 is max version for client and server.
TEST_P(SSLStreamAdapterTestDTLSHandshake, TestGetSslCipherSuite) {
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_12);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetSslCipherSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetSslCipherSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_TRUE(rtc::SSLStreamAdapter::IsAcceptableCipher(
      server_cipher, ::testing::get<1>(GetParam()).type()));
}

// Test different key sizes with SHA-256, then different signature algorithms
// with ECDSA. Two different RSA sizes are tested on the client and server.
// TODO: bugs.webrtc.org/375552698 - these tests are slow in debug builds
// and have caused flakyness in the past with a key size of 2048.
INSTANTIATE_TEST_SUITE_P(
    SSLStreamAdapterTestDTLSHandshakeKeyParameters,
    SSLStreamAdapterTestDTLSHandshake,
    Values(std::make_tuple(rtc::KeyParams::ECDSA(rtc::EC_NIST_P256),
                           rtc::KeyParams::RSA(rtc::kRsaDefaultModSize,
                                               rtc::kRsaDefaultExponent),
                           std::make_pair(rtc::DIGEST_SHA_256,
                                          SHA256_DIGEST_LENGTH)),
           std::make_tuple(rtc::KeyParams::RSA(1152, rtc::kRsaDefaultExponent),
                           rtc::KeyParams::ECDSA(rtc::EC_NIST_P256),
                           std::make_pair(rtc::DIGEST_SHA_256,
                                          SHA256_DIGEST_LENGTH))));

INSTANTIATE_TEST_SUITE_P(
    SSLStreamAdapterTestDTLSHandshakeSignatureAlgorithms,
    SSLStreamAdapterTestDTLSHandshake,
    Combine(Values(rtc::KeyParams::ECDSA(rtc::EC_NIST_P256)),
            Values(rtc::KeyParams::ECDSA(rtc::EC_NIST_P256)),
            Values(std::make_pair(rtc::DIGEST_SHA_1, SHA_DIGEST_LENGTH),
                   std::make_pair(rtc::DIGEST_SHA_224, SHA224_DIGEST_LENGTH),
                   std::make_pair(rtc::DIGEST_SHA_256, SHA256_DIGEST_LENGTH),
                   std::make_pair(rtc::DIGEST_SHA_384, SHA384_DIGEST_LENGTH),
                   std::make_pair(rtc::DIGEST_SHA_512, SHA512_DIGEST_LENGTH))));

// Basic tests done with ECDSA certificates and SHA-256.
class SSLStreamAdapterTestDTLS : public SSLStreamAdapterTestDTLSBase {
 public:
  SSLStreamAdapterTestDTLS()
      : SSLStreamAdapterTestDTLSBase(
            rtc::KeyParams::ECDSA(rtc::EC_NIST_P256),
            rtc::KeyParams::ECDSA(rtc::EC_NIST_P256),
            std::make_pair(rtc::DIGEST_SHA_256, SHA256_DIGEST_LENGTH)) {}
};

// Test that we can make a handshake work if the first packet in
// each direction is lost. This gives us predictable loss
// rather than having to tune random
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSConnectWithLostFirstPacket) {
  SetLoseFirstPacket(true);
  TestHandshake();
}

// Test a handshake with loss and delay
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSConnectWithLostFirstPacketDelay2s) {
  SetLoseFirstPacket(true);
  SetDelay(2000);
  SetHandshakeWait(20000);
  TestHandshake();
}

// Test a handshake with small MTU
// Disabled due to https://code.google.com/p/webrtc/issues/detail?id=3910
TEST_F(SSLStreamAdapterTestDTLS, DISABLED_TestDTLSConnectWithSmallMtu) {
  SetMtu(700);
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
}

// Test transfer -- trivial
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSTransfer) {
  TestHandshake();
  TestTransfer(100);
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
  const std::vector<int> crypto_suites = {rtc::kSrtpAes128CmSha1_80};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAes128CmSha1_80);
}

// Test DTLS-SRTP with SrtpAes128CmSha1_32
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpAes128CmSha1_32) {
  const std::vector<int> crypto_suites = {rtc::kSrtpAes128CmSha1_32};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAes128CmSha1_32);
}

// Test DTLS-SRTP with incompatible cipher suites -- should not converge.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpIncompatibleCipherSuites) {
  SetDtlsSrtpCryptoSuites({rtc::kSrtpAes128CmSha1_80}, true);
  SetDtlsSrtpCryptoSuites({rtc::kSrtpAes128CmSha1_32}, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
}

// Test DTLS-SRTP with each side being mixed -- should select the stronger
// cipher.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpMixed) {
  const std::vector<int> crypto_suites = {rtc::kSrtpAes128CmSha1_80,
                                          rtc::kSrtpAes128CmSha1_32};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAes128CmSha1_80);
}

// Test DTLS-SRTP with SrtpAeadAes128Gcm.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpAeadAes128Gcm) {
  std::vector<int> crypto_suites = {rtc::kSrtpAeadAes128Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAeadAes128Gcm);
}

// Test DTLS-SRTP with all GCM-256 ciphers.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCM256) {
  std::vector<int> crypto_suites = {rtc::kSrtpAeadAes256Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAeadAes256Gcm);
}

// Test DTLS-SRTP with incompatbile GCM-128/-256 ciphers -- should not converge.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpIncompatibleGcmCipherSuites) {
  SetDtlsSrtpCryptoSuites({rtc::kSrtpAeadAes128Gcm}, true);
  SetDtlsSrtpCryptoSuites({rtc::kSrtpAeadAes256Gcm}, false);
  TestHandshake();

  int client_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_FALSE(GetDtlsSrtpCryptoSuite(false, &server_cipher));
}

// Test DTLS-SRTP with both GCM-128/-256 ciphers -- should select GCM-256.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpGCMMixed) {
  std::vector<int> crypto_suites = {rtc::kSrtpAeadAes256Gcm,
                                    rtc::kSrtpAeadAes128Gcm};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);
  TestHandshake();

  int client_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(true, &client_cipher));
  int server_cipher;
  ASSERT_TRUE(GetDtlsSrtpCryptoSuite(false, &server_cipher));

  ASSERT_EQ(client_cipher, server_cipher);
  ASSERT_EQ(client_cipher, rtc::kSrtpAeadAes256Gcm);
}

// Test SRTP cipher suite lengths.
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSSrtpKeyAndSaltLengths) {
  int key_len;
  int salt_len;

  ASSERT_FALSE(rtc::GetSrtpKeyAndSaltLengths(rtc::kSrtpInvalidCryptoSuite,
                                             &key_len, &salt_len));

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::kSrtpAes128CmSha1_32, &key_len,
                                            &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::kSrtpAes128CmSha1_80, &key_len,
                                            &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(112 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::kSrtpAeadAes128Gcm, &key_len,
                                            &salt_len));
  ASSERT_EQ(128 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);

  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(rtc::kSrtpAeadAes256Gcm, &key_len,
                                            &salt_len));
  ASSERT_EQ(256 / 8, key_len);
  ASSERT_EQ(96 / 8, salt_len);
}

// Test an exporter
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
TEST_F(SSLStreamAdapterTestDTLS, TestDTLSExporter) {
  const std::vector<int> crypto_suites = {rtc::kSrtpAes128CmSha1_80};
  SetDtlsSrtpCryptoSuites(crypto_suites, true);
  SetDtlsSrtpCryptoSuites(crypto_suites, false);

  TestHandshake();
  int selected_crypto_suite;
  EXPECT_TRUE(GetDtlsSrtpCryptoSuite(/*client=*/false, &selected_crypto_suite));
  int key_len;
  int salt_len;
  ASSERT_TRUE(rtc::GetSrtpKeyAndSaltLengths(selected_crypto_suite, &key_len,
                                            &salt_len));
  rtc::ZeroOnFreeBuffer<uint8_t> client_out(2 * (key_len + salt_len));
  rtc::ZeroOnFreeBuffer<uint8_t> server_out(2 * (key_len + salt_len));

  EXPECT_TRUE(client_ssl_->ExportSrtpKeyingMaterial(client_out));
  EXPECT_TRUE(server_ssl_->ExportSrtpKeyingMaterial(server_out));
  EXPECT_EQ(client_out, server_out);

  // Legacy variant.
  rtc::ZeroOnFreeBuffer<uint8_t> legacy_out(2 * (key_len + salt_len));
  EXPECT_TRUE(client_ssl_->ExportKeyingMaterial("EXTRACTOR-dtls_srtp", nullptr,
                                                0, false, legacy_out.data(),
                                                legacy_out.size()));
  EXPECT_EQ(client_out, legacy_out);
}
#pragma clang diagnostic pop

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
  std::unique_ptr<rtc::SSLCertificate> client_peer_cert =
      GetPeerCertificate(true);
  ASSERT_TRUE(client_peer_cert);

  // It's not kCERT_PEM.
  std::string client_peer_string = client_peer_cert->ToPEMString();
  ASSERT_NE(kCERT_PEM, client_peer_string);

  // The server should have a peer certificate after the handshake.
  std::unique_ptr<rtc::SSLCertificate> server_peer_cert =
      GetPeerCertificate(false);
  ASSERT_TRUE(server_peer_cert);

  // It's kCERT_PEM
  ASSERT_EQ(kCERT_PEM, server_peer_cert->ToPEMString());
}

// Test getting the DTLS 1.2 version.
TEST_F(SSLStreamAdapterTestDTLS, TestGetSslVersionBytes) {
  // https://datatracker.ietf.org/doc/html/rfc9147#section-5.3
  const int kDtls1_2 = 0xFEFD;
  SetupProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_12);
  TestHandshake();

  int client_version;
  ASSERT_TRUE(GetSslVersionBytes(true, &client_version));
  EXPECT_EQ(client_version, kDtls1_2);

  int server_version;
  ASSERT_TRUE(GetSslVersionBytes(false, &server_version));
  EXPECT_EQ(server_version, kDtls1_2);
}
