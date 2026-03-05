/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/test/mock_local_network_access_permission.h"

#include <memory>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "api/local_network_access_permission.h"
#include "rtc_base/socket_address.h"
#include "rtc_base/thread.h"
#include "test/gmock.h"
#include "test/gtest.h"

using ::testing::_;

namespace webrtc {

FakeLocalNetworkAccessPermissionFactory::
    FakeLocalNetworkAccessPermissionFactory(Result result) {
  EXPECT_CALL(*this, Create()).WillRepeatedly([result]() {
    auto mock_lna_permission =
        std::make_unique<MockLocalNetworkAccessPermission>();
    EXPECT_CALL(*mock_lna_permission, ShouldRequestPermission(_))
        .WillRepeatedly(
            ::testing::Return(result != Result::kPermissionNotNeeded));
    if (result != Result::kPermissionNotNeeded) {
      EXPECT_CALL(*mock_lna_permission, RequestPermission(_, _))
          .WillRepeatedly(
              [result](
                  const SocketAddress& /* addr */,
                  absl::AnyInvocable<void(
                      webrtc::LocalNetworkAccessPermissionStatus)> callback) {
                Thread::Current()->PostTask([callback = std::move(callback),
                                             result]() mutable {
                  callback(result == Result::kPermissionGranted
                               ? LocalNetworkAccessPermissionStatus::kGranted
                               : LocalNetworkAccessPermissionStatus::kDenied);
                });
              });
    }
    return mock_lna_permission;
  });
}

FakeLocalNetworkAccessPermissionFactory::
    ~FakeLocalNetworkAccessPermissionFactory() = default;

}  // namespace webrtc
