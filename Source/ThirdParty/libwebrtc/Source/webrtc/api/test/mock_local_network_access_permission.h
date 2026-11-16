/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_TEST_MOCK_LOCAL_NETWORK_ACCESS_PERMISSION_H_
#define API_TEST_MOCK_LOCAL_NETWORK_ACCESS_PERMISSION_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "api/local_network_access_permission.h"
#include "rtc_base/socket_address.h"
#include "test/gmock.h"

namespace webrtc {

class MockLocalNetworkAccessPermission
    : public LocalNetworkAccessPermissionInterface {
 public:
  MOCK_METHOD(bool,
              ShouldRequestPermission,
              (const SocketAddress& addr),
              (override));

  MOCK_METHOD(
      void,
      RequestPermission,
      (const SocketAddress& addr,
       absl::AnyInvocable<void(LocalNetworkAccessPermissionStatus)> callback),
      (override));
};

class MockLocalNetworkAccessPermissionFactory
    : public LocalNetworkAccessPermissionFactoryInterface {
 public:
  MOCK_METHOD(std::unique_ptr<LocalNetworkAccessPermissionInterface>,
              Create,
              (),
              (override));
};

// Class that returns LocalNetworkAccessPermission's that run their callback
// with the provided status.
class FakeLocalNetworkAccessPermissionFactory
    : public MockLocalNetworkAccessPermissionFactory {
 public:
  enum class Result {
    // Use when the permission is not needed i.e. ShouldRequestPermission will
    // return false.
    kPermissionNotNeeded,
    // Use when the permission is needed i.e. ShouldRequestPermission will
    // return true, and RequestPermission will return kGranted/kDenied
    // respectively.
    kPermissionGranted,
    kPermissionDenied,
  };

  explicit FakeLocalNetworkAccessPermissionFactory(Result result);
  ~FakeLocalNetworkAccessPermissionFactory() override;
};

}  // namespace webrtc

#endif  // API_TEST_MOCK_LOCAL_NETWORK_ACCESS_PERMISSION_H_
