/*
 *  Copyright 2018 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static com.google.common.truth.Truth.assertThat;

import androidx.test.runner.AndroidJUnit4;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.webrtc.CryptoOptions;

@RunWith(AndroidJUnit4.class)
@Config(manifest = Config.NONE)
public class CryptoOptionsTest {
  // Validates the builder builds by default all false options.
  @Test
  public void testBuilderDefaultsAreFalse() {
    CryptoOptions cryptoOptions = CryptoOptions.builder().createCryptoOptions();
    assertThat(cryptoOptions.getSrtp().getEnableGcmCryptoSuites()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableAes128Sha1_32CryptoCipher()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableEncryptedRtpHeaderExtensions()).isFalse();
    assertThat(cryptoOptions.getSFrame().getRequireFrameEncryption()).isFalse();
  }

  // Validates the builder sets the correct parameters.
  @Test
  public void testBuilderCorrectlyInitializingGcmCrypto() {
    CryptoOptions cryptoOptions =
        CryptoOptions.builder().setEnableGcmCryptoSuites(true).createCryptoOptions();
    assertThat(cryptoOptions.getSrtp().getEnableGcmCryptoSuites()).isTrue();
    assertThat(cryptoOptions.getSrtp().getEnableAes128Sha1_32CryptoCipher()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableEncryptedRtpHeaderExtensions()).isFalse();
    assertThat(cryptoOptions.getSFrame().getRequireFrameEncryption()).isFalse();
  }

  @Test
  public void testBuilderCorrectlyInitializingAes128Sha1_32CryptoCipher() {
    CryptoOptions cryptoOptions =
        CryptoOptions.builder().setEnableAes128Sha1_32CryptoCipher(true).createCryptoOptions();
    assertThat(cryptoOptions.getSrtp().getEnableGcmCryptoSuites()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableAes128Sha1_32CryptoCipher()).isTrue();
    assertThat(cryptoOptions.getSrtp().getEnableEncryptedRtpHeaderExtensions()).isFalse();
    assertThat(cryptoOptions.getSFrame().getRequireFrameEncryption()).isFalse();
  }

  @Test
  public void testBuilderCorrectlyInitializingEncryptedRtpHeaderExtensions() {
    CryptoOptions cryptoOptions =
        CryptoOptions.builder().setEnableEncryptedRtpHeaderExtensions(true).createCryptoOptions();
    assertThat(cryptoOptions.getSrtp().getEnableGcmCryptoSuites()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableAes128Sha1_32CryptoCipher()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableEncryptedRtpHeaderExtensions()).isTrue();
    assertThat(cryptoOptions.getSFrame().getRequireFrameEncryption()).isFalse();
  }

  @Test
  public void testBuilderCorrectlyInitializingRequireFrameEncryption() {
    CryptoOptions cryptoOptions =
        CryptoOptions.builder().setRequireFrameEncryption(true).createCryptoOptions();
    assertThat(cryptoOptions.getSrtp().getEnableGcmCryptoSuites()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableAes128Sha1_32CryptoCipher()).isFalse();
    assertThat(cryptoOptions.getSrtp().getEnableEncryptedRtpHeaderExtensions()).isFalse();
    assertThat(cryptoOptions.getSFrame().getRequireFrameEncryption()).isTrue();
  }
}
