/*
 *  Copyright 2025 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_LOCAL_NETWORK_ACCESS_PERMISSION_H_
#define API_LOCAL_NETWORK_ACCESS_PERMISSION_H_

#include <memory>

#include "absl/functional/any_invocable.h"
#include "rtc_base/socket_address.h"

namespace webrtc {

// This interface defines methods to request a Local Network Access permission
// asynchronously. The LocalNetworkAccessPermissionInterface class encapsulates
// a single permission request.
//
// Usage:
// // An implementation of the factory should be passed in by the embedder.
// std::unique_ptr<LocalNetworkAccessPermissionFactoryInterface> factory =
//   embedder_factory;
//
// Stores pending permission requests.
// std::vector<std::unique_ptr<LocalNetworkAccessPermissionInterface>>
//     permission_list;
//
// std::unique_ptr<LocalNetworkAccessPermissionInterface> permission =
//     factory->Create();
// permission->RequestPermission(
//     target_address,
//     [&, r = permission.get()](LocalNetworkAccessPermissionStatus status) {
//       std::erase_if(permission_list,
//           [&](const auto& refptr) { return refptr.get() == r; });
//
//       if (status == LocalNetworkAccessPermissionStatus::kGranted) {
//         // Permission was granted.
//       } else {
//         // Permission was denied.
//       }
//     });
// permission_list.push_back(std::move(permission));

enum class LocalNetworkAccessPermissionStatus {
  kGranted,
  kDenied,
};

// The API for a single permission query.
// The constructor, destructor and all functions must be called from
// the same sequence, and the callback will also be called on that sequence.
// The class guarantees that the callback will not be called if the
// permission's destructor has been called.
class LocalNetworkAccessPermissionInterface {
 public:
  virtual ~LocalNetworkAccessPermissionInterface() = default;

  // Returns whether or not the caller should request permission. Depending
  // on the originator's address space, sometimes it's not necessary to request
  // permission.
  // TODO(crbug.com/421223919): Make this method pure virtual once all
  // implementations implement it.
  virtual bool ShouldRequestPermission(const SocketAddress& addr) {
    return addr.IsPrivateIP() || addr.IsLoopbackIP();
  }

  // The callback will be called when the permission is granted or denied. The
  // callback will be called on the sequence that the caller runs on.
  virtual void RequestPermission(
      const SocketAddress& addr,
      absl::AnyInvocable<void(LocalNetworkAccessPermissionStatus)>
          callback) = 0;
};

// An abstract factory for creating LocalNetworkPermissionInterfaces. This
// allows client applications to provide WebRTC with their own mechanism for
// checking and requesting Local Network Access permission.
class LocalNetworkAccessPermissionFactoryInterface {
 public:
  virtual ~LocalNetworkAccessPermissionFactoryInterface() = default;

  // Creates a LocalNetworkAccessPermission.
  virtual std::unique_ptr<LocalNetworkAccessPermissionInterface> Create() = 0;
};

}  // namespace webrtc

#endif  // API_LOCAL_NETWORK_ACCESS_PERMISSION_H_
