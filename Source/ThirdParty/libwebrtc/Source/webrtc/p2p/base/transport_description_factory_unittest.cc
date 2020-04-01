/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "p2p/base/transport_description_factory.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "p2p/base/p2p_constants.h"
#include "p2p/base/transport_description.h"
#include "rtc_base/copy_on_write_buffer.h"
#include "rtc_base/fake_ssl_identity.h"
#include "rtc_base/ssl_certificate.h"
#include "rtc_base/ssl_fingerprint.h"
#include "rtc_base/ssl_identity.h"
#include "test/gmock.h"
#include "test/gtest.h"

using cricket::OpaqueTransportParameters;
using cricket::TransportDescription;
using cricket::TransportDescriptionFactory;
using cricket::TransportOptions;
using ::testing::Contains;
using ::testing::Not;

class TransportDescriptionFactoryTest : public ::testing::Test {
 public:
  TransportDescriptionFactoryTest()
      : ice_credentials_({}),
        cert1_(rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            new rtc::FakeSSLIdentity("User1")))),
        cert2_(rtc::RTCCertificate::Create(std::unique_ptr<rtc::SSLIdentity>(
            new rtc::FakeSSLIdentity("User2")))) {}

  void CheckDesc(const TransportDescription* desc,
                 const std::string& opt,
                 const std::string& ice_ufrag,
                 const std::string& ice_pwd,
                 const std::string& dtls_alg) {
    ASSERT_TRUE(desc != NULL);
    EXPECT_EQ(!opt.empty(), desc->HasOption(opt));
    if (ice_ufrag.empty() && ice_pwd.empty()) {
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
                desc->ice_ufrag.size());
      EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
                desc->ice_pwd.size());
    } else {
      EXPECT_EQ(ice_ufrag, desc->ice_ufrag);
      EXPECT_EQ(ice_pwd, desc->ice_pwd);
    }
    if (dtls_alg.empty()) {
      EXPECT_TRUE(desc->identity_fingerprint.get() == NULL);
    } else {
      ASSERT_TRUE(desc->identity_fingerprint.get() != NULL);
      EXPECT_EQ(desc->identity_fingerprint->algorithm, dtls_alg);
      EXPECT_GT(desc->identity_fingerprint->digest.size(), 0U);
    }
  }

  // This test ice restart by doing two offer answer exchanges. On the second
  // exchange ice is restarted. The test verifies that the ufrag and password
  // in the offer and answer is changed.
  // If |dtls| is true, the test verifies that the finger print is not changed.
  void TestIceRestart(bool dtls) {
    SetDtls(dtls);
    cricket::TransportOptions options;
    // The initial offer / answer exchange.
    std::unique_ptr<TransportDescription> offer =
        f1_.CreateOffer(options, NULL, &ice_credentials_);
    std::unique_ptr<TransportDescription> answer =
        f2_.CreateAnswer(offer.get(), options, true, NULL, &ice_credentials_);

    // Create an updated offer where we restart ice.
    options.ice_restart = true;
    std::unique_ptr<TransportDescription> restart_offer =
        f1_.CreateOffer(options, offer.get(), &ice_credentials_);

    VerifyUfragAndPasswordChanged(dtls, offer.get(), restart_offer.get());

    // Create a new answer. The transport ufrag and password is changed since
    // |options.ice_restart == true|
    std::unique_ptr<TransportDescription> restart_answer = f2_.CreateAnswer(
        restart_offer.get(), options, true, answer.get(), &ice_credentials_);
    ASSERT_TRUE(restart_answer.get() != NULL);

    VerifyUfragAndPasswordChanged(dtls, answer.get(), restart_answer.get());
  }

  void VerifyUfragAndPasswordChanged(bool dtls,
                                     const TransportDescription* org_desc,
                                     const TransportDescription* restart_desc) {
    EXPECT_NE(org_desc->ice_pwd, restart_desc->ice_pwd);
    EXPECT_NE(org_desc->ice_ufrag, restart_desc->ice_ufrag);
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_UFRAG_LENGTH),
              restart_desc->ice_ufrag.size());
    EXPECT_EQ(static_cast<size_t>(cricket::ICE_PWD_LENGTH),
              restart_desc->ice_pwd.size());
    // If DTLS is enabled, make sure the finger print is unchanged.
    if (dtls) {
      EXPECT_FALSE(
          org_desc->identity_fingerprint->GetRfc4572Fingerprint().empty());
      EXPECT_EQ(org_desc->identity_fingerprint->GetRfc4572Fingerprint(),
                restart_desc->identity_fingerprint->GetRfc4572Fingerprint());
    }
  }

  void TestIceRenomination(bool dtls) {
    SetDtls(dtls);

    cricket::TransportOptions options;
    // The initial offer / answer exchange.
    std::unique_ptr<TransportDescription> offer =
        f1_.CreateOffer(options, nullptr, &ice_credentials_);
    ASSERT_TRUE(offer);
    EXPECT_THAT(offer->transport_options, Not(Contains("renomination")));

    std::unique_ptr<TransportDescription> answer = f2_.CreateAnswer(
        offer.get(), options, true, nullptr, &ice_credentials_);
    ASSERT_TRUE(answer);
    EXPECT_THAT(answer->transport_options, Not(Contains("renomination")));

    options.enable_ice_renomination = true;
    std::unique_ptr<TransportDescription> renomination_offer =
        f1_.CreateOffer(options, offer.get(), &ice_credentials_);
    ASSERT_TRUE(renomination_offer);
    EXPECT_THAT(renomination_offer->transport_options,
                Contains("renomination"));

    std::unique_ptr<TransportDescription> renomination_answer =
        f2_.CreateAnswer(renomination_offer.get(), options, true, answer.get(),
                         &ice_credentials_);
    ASSERT_TRUE(renomination_answer);
    EXPECT_THAT(renomination_answer->transport_options,
                Contains("renomination"));
  }

 protected:
  void SetDtls(bool dtls) {
    if (dtls) {
      f1_.set_secure(cricket::SEC_ENABLED);
      f2_.set_secure(cricket::SEC_ENABLED);
      f1_.set_certificate(cert1_);
      f2_.set_certificate(cert2_);
    } else {
      f1_.set_secure(cricket::SEC_DISABLED);
      f2_.set_secure(cricket::SEC_DISABLED);
    }
  }

  cricket::IceCredentialsIterator ice_credentials_;
  TransportDescriptionFactory f1_;
  TransportDescriptionFactory f2_;

  rtc::scoped_refptr<rtc::RTCCertificate> cert1_;
  rtc::scoped_refptr<rtc::RTCCertificate> cert2_;
};

TEST_F(TransportDescriptionFactoryTest, TestOfferDefault) {
  std::unique_ptr<TransportDescription> desc =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", "");
}

TEST_F(TransportDescriptionFactoryTest, TestOfferDtls) {
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_certificate(cert1_);
  std::string digest_alg;
  ASSERT_TRUE(
      cert1_->GetSSLCertificate().GetSignatureDigestAlgorithm(&digest_alg));
  std::unique_ptr<TransportDescription> desc =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", digest_alg);
  // Ensure it also works with SEC_REQUIRED.
  f1_.set_secure(cricket::SEC_REQUIRED);
  desc = f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", digest_alg);
}

// Test generating an offer with DTLS fails with no identity.
TEST_F(TransportDescriptionFactoryTest, TestOfferDtlsWithNoIdentity) {
  f1_.set_secure(cricket::SEC_ENABLED);
  std::unique_ptr<TransportDescription> desc =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(desc.get() == NULL);
}

// Test updating an offer with DTLS to pick ICE.
// The ICE credentials should stay the same in the new offer.
TEST_F(TransportDescriptionFactoryTest, TestOfferDtlsReofferDtls) {
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_certificate(cert1_);
  std::string digest_alg;
  ASSERT_TRUE(
      cert1_->GetSSLCertificate().GetSignatureDigestAlgorithm(&digest_alg));
  std::unique_ptr<TransportDescription> old_desc =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(old_desc.get() != NULL);
  std::unique_ptr<TransportDescription> desc =
      f1_.CreateOffer(TransportOptions(), old_desc.get(), &ice_credentials_);
  CheckDesc(desc.get(), "", old_desc->ice_ufrag, old_desc->ice_pwd, digest_alg);
}

TEST_F(TransportDescriptionFactoryTest, TestOfferOpaqueTransportParameters) {
  OpaqueTransportParameters params;
  params.protocol = "fake";
  params.parameters = "foobar";

  TransportOptions options;
  options.opaque_parameters = params;

  std::unique_ptr<TransportDescription> desc =
      f1_.CreateOffer(options, NULL, &ice_credentials_);

  CheckDesc(desc.get(), "", "", "", "");
  EXPECT_EQ(desc->opaque_parameters, params);
}

TEST_F(TransportDescriptionFactoryTest, TestAnswerOpaqueTransportParameters) {
  OpaqueTransportParameters params;
  params.protocol = "fake";
  params.parameters = "foobar";

  TransportOptions options;
  options.opaque_parameters = params;

  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(options, NULL, &ice_credentials_);
  std::unique_ptr<TransportDescription> answer =
      f2_.CreateAnswer(offer.get(), options, true, NULL, &ice_credentials_);

  CheckDesc(answer.get(), "", "", "", "");
  EXPECT_EQ(answer->opaque_parameters, params);
}

TEST_F(TransportDescriptionFactoryTest, TestAnswerNoOpaqueTransportParameters) {
  OpaqueTransportParameters params;
  params.protocol = "fake";
  params.parameters = "foobar";

  TransportOptions options;
  options.opaque_parameters = params;

  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(options, NULL, &ice_credentials_);
  std::unique_ptr<TransportDescription> answer = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);

  CheckDesc(answer.get(), "", "", "", "");
  EXPECT_EQ(answer->opaque_parameters, absl::nullopt);
}

TEST_F(TransportDescriptionFactoryTest,
       TestAnswerNoOpaqueTransportParametersInOffer) {
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);

  OpaqueTransportParameters params;
  params.protocol = "fake";
  params.parameters = "foobar";

  TransportOptions options;
  options.opaque_parameters = params;
  std::unique_ptr<TransportDescription> answer =
      f2_.CreateAnswer(offer.get(), options, true, NULL, &ice_credentials_);

  CheckDesc(answer.get(), "", "", "", "");
  EXPECT_EQ(answer->opaque_parameters, absl::nullopt);
}

TEST_F(TransportDescriptionFactoryTest, TestAnswerDefault) {
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<TransportDescription> desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", "");
  desc = f2_.CreateAnswer(offer.get(), TransportOptions(), true, NULL,
                          &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", "");
}

// Test that we can update an answer properly; ICE credentials shouldn't change.
TEST_F(TransportDescriptionFactoryTest, TestReanswer) {
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<TransportDescription> old_desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);
  ASSERT_TRUE(old_desc.get() != NULL);
  std::unique_ptr<TransportDescription> desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, old_desc.get(), &ice_credentials_);
  ASSERT_TRUE(desc.get() != NULL);
  CheckDesc(desc.get(), "", old_desc->ice_ufrag, old_desc->ice_pwd, "");
}

// Test that we handle answering an offer with DTLS with no DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerDtlsToNoDtls) {
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_certificate(cert1_);
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<TransportDescription> desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", "");
}

// Test that we handle answering an offer without DTLS if we have DTLS enabled,
// but fail if we require DTLS.
TEST_F(TransportDescriptionFactoryTest, TestAnswerNoDtlsToDtls) {
  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_certificate(cert2_);
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<TransportDescription> desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", "");
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc = f2_.CreateAnswer(offer.get(), TransportOptions(), true, NULL,
                          &ice_credentials_);
  ASSERT_TRUE(desc.get() == NULL);
}

// Test that we handle answering an DTLS offer with DTLS, both if we have
// DTLS enabled and required.
TEST_F(TransportDescriptionFactoryTest, TestAnswerDtlsToDtls) {
  f1_.set_secure(cricket::SEC_ENABLED);
  f1_.set_certificate(cert1_);

  f2_.set_secure(cricket::SEC_ENABLED);
  f2_.set_certificate(cert2_);
  // f2_ produces the answer that is being checked in this test, so the
  // answer must contain fingerprint lines with cert2_'s digest algorithm.
  std::string digest_alg2;
  ASSERT_TRUE(
      cert2_->GetSSLCertificate().GetSignatureDigestAlgorithm(&digest_alg2));

  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(TransportOptions(), NULL, &ice_credentials_);
  ASSERT_TRUE(offer.get() != NULL);
  std::unique_ptr<TransportDescription> desc = f2_.CreateAnswer(
      offer.get(), TransportOptions(), true, NULL, &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", digest_alg2);
  f2_.set_secure(cricket::SEC_REQUIRED);
  desc = f2_.CreateAnswer(offer.get(), TransportOptions(), true, NULL,
                          &ice_credentials_);
  CheckDesc(desc.get(), "", "", "", digest_alg2);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true.
TEST_F(TransportDescriptionFactoryTest, TestIceRestart) {
  TestIceRestart(false);
}

// Test that ice ufrag and password is changed in an updated offer and answer
// if |TransportDescriptionOptions::ice_restart| is true and DTLS is enabled.
TEST_F(TransportDescriptionFactoryTest, TestIceRestartWithDtls) {
  TestIceRestart(true);
}

// Test that ice renomination is set in an updated offer and answer
// if |TransportDescriptionOptions::enable_ice_renomination| is true.
TEST_F(TransportDescriptionFactoryTest, TestIceRenomination) {
  TestIceRenomination(false);
}

// Test that ice renomination is set in an updated offer and answer
// if |TransportDescriptionOptions::enable_ice_renomination| is true and DTLS
// is enabled.
TEST_F(TransportDescriptionFactoryTest, TestIceRenominationWithDtls) {
  TestIceRenomination(true);
}

// Test that offers and answers have ice-option:trickle.
TEST_F(TransportDescriptionFactoryTest, AddsTrickleIceOption) {
  cricket::TransportOptions options;
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(options, nullptr, &ice_credentials_);
  EXPECT_TRUE(offer->HasOption("trickle"));
  std::unique_ptr<TransportDescription> answer =
      f2_.CreateAnswer(offer.get(), options, true, nullptr, &ice_credentials_);
  EXPECT_TRUE(answer->HasOption("trickle"));
}

// Test CreateOffer with IceCredentialsIterator.
TEST_F(TransportDescriptionFactoryTest, CreateOfferIceCredentialsIterator) {
  std::vector<cricket::IceParameters> credentials = {
      cricket::IceParameters("kalle", "anka", false)};
  cricket::IceCredentialsIterator credentialsIterator(credentials);
  cricket::TransportOptions options;
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(options, nullptr, &credentialsIterator);
  EXPECT_EQ(offer->GetIceParameters().ufrag, credentials[0].ufrag);
  EXPECT_EQ(offer->GetIceParameters().pwd, credentials[0].pwd);
}

// Test CreateAnswer with IceCredentialsIterator.
TEST_F(TransportDescriptionFactoryTest, CreateAnswerIceCredentialsIterator) {
  cricket::TransportOptions options;
  std::unique_ptr<TransportDescription> offer =
      f1_.CreateOffer(options, nullptr, &ice_credentials_);

  std::vector<cricket::IceParameters> credentials = {
      cricket::IceParameters("kalle", "anka", false)};
  cricket::IceCredentialsIterator credentialsIterator(credentials);
  std::unique_ptr<TransportDescription> answer = f1_.CreateAnswer(
      offer.get(), options, false, nullptr, &credentialsIterator);
  EXPECT_EQ(answer->GetIceParameters().ufrag, credentials[0].ufrag);
  EXPECT_EQ(answer->GetIceParameters().pwd, credentials[0].pwd);
}
