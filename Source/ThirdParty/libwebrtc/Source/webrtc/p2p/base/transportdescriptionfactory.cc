/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transportdescriptionfactory.h"

#include <memory>

#include "p2p/base/transportdescription.h"
#include "rtc_base/helpers.h"
#include "rtc_base/logging.h"
#include "rtc_base/messagedigest.h"
#include "rtc_base/sslfingerprint.h"

namespace cricket {

TransportDescriptionFactory::TransportDescriptionFactory()
    : secure_(SEC_DISABLED) {}

TransportDescriptionFactory::~TransportDescriptionFactory() = default;

TransportDescription* TransportDescriptionFactory::CreateOffer(
    const TransportOptions& options,
    const TransportDescription* current_description) const {
  std::unique_ptr<TransportDescription> desc(new TransportDescription());

  // Generate the ICE credentials if we don't already have them.
  if (!current_description || options.ice_restart) {
    desc->ice_ufrag = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    desc->ice_pwd = rtc::CreateRandomString(ICE_PWD_LENGTH);
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }
  desc->AddOption(ICE_OPTION_TRICKLE);
  if (options.enable_ice_renomination) {
    desc->AddOption(ICE_OPTION_RENOMINATION);
  }

  // If we are trying to establish a secure transport, add a fingerprint.
  if (secure_ == SEC_ENABLED || secure_ == SEC_REQUIRED) {
    // Fail if we can't create the fingerprint.
    // If we are the initiator set role to "actpass".
    if (!SetSecurityInfo(desc.get(), CONNECTIONROLE_ACTPASS)) {
      return NULL;
    }
  }

  return desc.release();
}

TransportDescription* TransportDescriptionFactory::CreateAnswer(
    const TransportDescription* offer,
    const TransportOptions& options,
    bool require_transport_attributes,
    const TransportDescription* current_description) const {
  // TODO(juberti): Figure out why we get NULL offers, and fix this upstream.
  if (!offer) {
    RTC_LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                           "because offer is NULL";
    return NULL;
  }

  std::unique_ptr<TransportDescription> desc(new TransportDescription());
  // Generate the ICE credentials if we don't already have them or ice is
  // being restarted.
  if (!current_description || options.ice_restart) {
    desc->ice_ufrag = rtc::CreateRandomString(ICE_UFRAG_LENGTH);
    desc->ice_pwd = rtc::CreateRandomString(ICE_PWD_LENGTH);
  } else {
    desc->ice_ufrag = current_description->ice_ufrag;
    desc->ice_pwd = current_description->ice_pwd;
  }
  desc->AddOption(ICE_OPTION_TRICKLE);
  if (options.enable_ice_renomination) {
    desc->AddOption(ICE_OPTION_RENOMINATION);
  }

  // Negotiate security params.
  if (offer && offer->identity_fingerprint.get()) {
    // The offer supports DTLS, so answer with DTLS, as long as we support it.
    if (secure_ == SEC_ENABLED || secure_ == SEC_REQUIRED) {
      // Fail if we can't create the fingerprint.
      // Setting DTLS role to active.
      ConnectionRole role = (options.prefer_passive_role)
                                ? CONNECTIONROLE_PASSIVE
                                : CONNECTIONROLE_ACTIVE;

      if (!SetSecurityInfo(desc.get(), role)) {
        return NULL;
      }
    }
  } else if (require_transport_attributes && secure_ == SEC_REQUIRED) {
    // We require DTLS, but the other side didn't offer it. Fail.
    RTC_LOG(LS_WARNING) << "Failed to create TransportDescription answer "
                           "because of incompatible security settings";
    return NULL;
  }

  return desc.release();
}

bool TransportDescriptionFactory::SetSecurityInfo(TransportDescription* desc,
                                                  ConnectionRole role) const {
  if (!certificate_) {
    RTC_LOG(LS_ERROR) << "Cannot create identity digest with no certificate";
    return false;
  }

  // This digest algorithm is used to produce the a=fingerprint lines in SDP.
  // RFC 4572 Section 5 requires that those lines use the same hash function as
  // the certificate's signature, which is what CreateFromCertificate does.
  desc->identity_fingerprint.reset(
      rtc::SSLFingerprint::CreateFromCertificate(certificate_));
  if (!desc->identity_fingerprint) {
    return false;
  }

  // Assign security role.
  desc->connection_role = role;
  return true;
}

}  // namespace cricket
