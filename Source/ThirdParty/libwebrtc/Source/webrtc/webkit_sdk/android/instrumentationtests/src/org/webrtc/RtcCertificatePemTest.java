/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.filters.SmallTest;
import org.junit.Before;
import org.junit.Test;
import org.webrtc.PeerConnection;
import org.webrtc.RtcCertificatePem;

/** Tests for RtcCertificatePem.java. */
public class RtcCertificatePemTest {
  @Before
  public void setUp() {
    System.loadLibrary(TestConstants.NATIVE_LIBRARY);
  }

  @Test
  @SmallTest
  public void testConstructor() {
    RtcCertificatePem original = RtcCertificatePem.generateCertificate();
    RtcCertificatePem recreated = new RtcCertificatePem(original.privateKey, original.certificate);
    assertThat(original.privateKey).isEqualTo(recreated.privateKey);
    assertThat(original.certificate).isEqualTo(recreated.certificate);
  }

  @Test
  @SmallTest
  public void testGenerateCertificateDefaults() {
    RtcCertificatePem rtcCertificate = RtcCertificatePem.generateCertificate();
    assertThat(rtcCertificate.privateKey).isNotEmpty();
    assertThat(rtcCertificate.certificate).isNotEmpty();
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomKeyTypeDefaultExpires() {
    RtcCertificatePem rtcCertificate =
        RtcCertificatePem.generateCertificate(PeerConnection.KeyType.RSA);
    assertThat(rtcCertificate.privateKey).isNotEmpty();
    assertThat(rtcCertificate.certificate).isNotEmpty();
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomExpiresDefaultKeyType() {
    RtcCertificatePem rtcCertificate = RtcCertificatePem.generateCertificate(60 * 60 * 24);
    assertThat(rtcCertificate.privateKey).isNotEmpty();
    assertThat(rtcCertificate.certificate).isNotEmpty();
  }

  @Test
  @SmallTest
  public void testGenerateCertificateCustomKeyTypeAndExpires() {
    RtcCertificatePem rtcCertificate =
        RtcCertificatePem.generateCertificate(PeerConnection.KeyType.RSA, 60 * 60 * 24);
    assertThat(rtcCertificate.privateKey).isNotEmpty();
    assertThat(rtcCertificate.certificate).isNotEmpty();
  }
}
