// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BSSL_PKI_OCSP_REVOCATION_STATUS_H_
#define BSSL_PKI_OCSP_REVOCATION_STATUS_H_

BSSL_NAMESPACE_BEGIN

// This value is histogrammed, so do not re-order or change values, and add
// new values at the end.
enum class OCSPRevocationStatus {
  GOOD = 0,
  REVOKED = 1,
  UNKNOWN = 2,
  MAX_VALUE = UNKNOWN
};

BSSL_NAMESPACE_END

#endif  // BSSL_PKI_OCSP_REVOCATION_STATUS_H_
