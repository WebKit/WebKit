/*
 *  Copyright 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static java.util.Collections.singletonList;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import java.util.Arrays;
import java.util.List;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.webrtc.PeerConnection.TlsCertPolicy;

/** Unit tests for {@link PeerConnection}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class PeerConnectionTest {
  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());
  }

  @Test
  @SmallTest
  public void testIceServerChanged() throws Exception {
    PeerConnection.IceServer iceServer1 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Same as iceServer1.
    PeerConnection.IceServer iceServer2 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the url.
    PeerConnection.IceServer iceServer3 =
        PeerConnection.IceServer.builder("turn:fake.example2.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the username.
    PeerConnection.IceServer iceServer4 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername2")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the password.
    PeerConnection.IceServer iceServer5 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword2")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_SECURE)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS certificate policy.
    PeerConnection.IceServer iceServer6 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the hostname.
    PeerConnection.IceServer iceServer7 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname2")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS ALPN.
    PeerConnection.IceServer iceServer8 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol2"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve"))
            .createIceServer();
    // Differs from iceServer1 by the TLS elliptic curve.
    PeerConnection.IceServer iceServer9 =
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .setTlsCertPolicy(TlsCertPolicy.TLS_CERT_POLICY_INSECURE_NO_CHECK)
            .setHostname("fakeHostname")
            .setTlsAlpnProtocols(singletonList("fakeTlsAlpnProtocol"))
            .setTlsEllipticCurves(singletonList("fakeTlsEllipticCurve2"))
            .createIceServer();

    assertTrue(iceServer1.equals(iceServer2));
    assertFalse(iceServer1.equals(iceServer3));
    assertFalse(iceServer1.equals(iceServer4));
    assertFalse(iceServer1.equals(iceServer5));
    assertFalse(iceServer1.equals(iceServer6));
    assertFalse(iceServer1.equals(iceServer7));
    assertFalse(iceServer1.equals(iceServer8));
    assertFalse(iceServer1.equals(iceServer9));
  }

  // TODO(fischman) MOAR test ideas:
  // - Test that PC.removeStream() works; requires a second
  //   createOffer/createAnswer dance.
  // - audit each place that uses |constraints| for specifying non-trivial
  //   constraints (and ensure they're honored).
  // - test error cases
  // - ensure reasonable coverage of jni code is achieved.  Coverage is
  //   extra-important because of all the free-text (class/method names, etc)
  //   in JNI-style programming; make sure no typos!
  // - Test that shutdown mid-interaction is crash-free.

  // Tests that the JNI glue between Java and C++ does not crash when creating a PeerConnection.
  @Test
  @SmallTest
  public void testCreationWithConfig() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    List<PeerConnection.IceServer> iceServers = Arrays.asList(
        PeerConnection.IceServer.builder("stun:stun.l.google.com:19302").createIceServer(),
        PeerConnection.IceServer.builder("turn:fake.example.com")
            .setUsername("fakeUsername")
            .setPassword("fakePassword")
            .createIceServer());
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(iceServers);

    // Test configuration options.
    config.continualGatheringPolicy = PeerConnection.ContinualGatheringPolicy.GATHER_CONTINUALLY;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
    assertNotNull(offeringPC);
  }

  @Test
  @SmallTest
  public void testCreationWithCertificate() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(Arrays.asList());

    // Test certificate.
    RtcCertificatePem originalCert = RtcCertificatePem.generateCertificate();
    config.certificate = originalCert;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));

    RtcCertificatePem restoredCert = offeringPC.getCertificate();
    assertEquals(originalCert.privateKey, restoredCert.privateKey);
    assertEquals(originalCert.certificate, restoredCert.certificate);
  }

  @Test
  @SmallTest
  public void testCreationWithCryptoOptions() throws Exception {
    PeerConnectionFactory factory = PeerConnectionFactory.builder().createPeerConnectionFactory();
    PeerConnection.RTCConfiguration config = new PeerConnection.RTCConfiguration(Arrays.asList());

    assertNull(config.cryptoOptions);

    CryptoOptions cryptoOptions = CryptoOptions.builder()
                                      .setEnableGcmCryptoSuites(true)
                                      .setEnableAes128Sha1_32CryptoCipher(true)
                                      .setEnableEncryptedRtpHeaderExtensions(true)
                                      .setRequireFrameEncryption(true)
                                      .createCryptoOptions();
    config.cryptoOptions = cryptoOptions;

    PeerConnection offeringPC =
        factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
    assertNotNull(offeringPC);
  }
}
