/*
 *  Copyright 2023 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;

import androidx.annotation.Nullable;
import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import org.junit.Before;
import org.junit.Test;
import org.webrtc.PeerConnectionEndToEndTest.SdpObserverLatch;
import org.webrtc.RtpParameters.Encoding;
import org.webrtc.RtpTransceiver.RtpTransceiverInit;

/**
 * Unit-tests for {@link RtpCapabilities}.
 */
public class RtpCapabilitiesTest {
  private PeerConnectionFactory factory;
  private PeerConnection pc;

  static class CustomHardwareVideoEncoderFactory implements VideoEncoderFactory {
    private final List<VideoCodecInfo> supportedCodecs;

    public CustomHardwareVideoEncoderFactory(List<VideoCodecInfo> supportedCodecs) {
      this.supportedCodecs = supportedCodecs;
    }

    @Override
    public @Nullable VideoEncoder createEncoder(VideoCodecInfo info) {
      return null;
    }

    @Override
    public VideoCodecInfo[] getSupportedCodecs() {
      return supportedCodecs.toArray(new VideoCodecInfo[0]);
    }
  }

  static class CustomHardwareVideoDecoderFactory implements VideoDecoderFactory {
    private final List<VideoCodecInfo> supportedCodecs;

    public CustomHardwareVideoDecoderFactory(List<VideoCodecInfo> supportedCodecs) {
      this.supportedCodecs = supportedCodecs;
    }

    @Nullable
    @Override
    public VideoDecoder createDecoder(VideoCodecInfo videoCodecInfo) {
      return null;
    }

    @Override
    public VideoCodecInfo[] getSupportedCodecs() {
      return supportedCodecs.toArray(new VideoCodecInfo[0]);
    }
  }

  @Before
  public void setUp() {
    PeerConnectionFactory.initialize(PeerConnectionFactory.InitializationOptions
                                         .builder(InstrumentationRegistry.getTargetContext())
                                         .setNativeLibraryName(TestConstants.NATIVE_LIBRARY)
                                         .createInitializationOptions());

    VideoCodecInfo vp8Codec = new VideoCodecInfo("VP8", new HashMap<>());
    VideoCodecInfo h264Codec = new VideoCodecInfo("H264", new HashMap<>());
    List<VideoCodecInfo> supportedCodecs = new ArrayList<>();
    supportedCodecs.add(vp8Codec);
    supportedCodecs.add(h264Codec);

    factory = PeerConnectionFactory.builder()
                  .setVideoEncoderFactory(new CustomHardwareVideoEncoderFactory(supportedCodecs))
                  .setVideoDecoderFactory(new CustomHardwareVideoDecoderFactory(supportedCodecs))
                  .createPeerConnectionFactory();

    PeerConnection.RTCConfiguration config =
        new PeerConnection.RTCConfiguration(Collections.emptyList());
    // RtpTranceiver is part of new unified plan semantics.
    config.sdpSemantics = PeerConnection.SdpSemantics.UNIFIED_PLAN;
    pc = factory.createPeerConnection(config, mock(PeerConnection.Observer.class));
  }

  /**
   * Test that getRtpSenderCapabilities returns the codecs supported by the video encoder factory.
   */
  @Test
  @SmallTest
  public void testGetSenderCapabilities() {
    RtpCapabilities capabilities =
        factory.getRtpSenderCapabilities(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO);
    List<String> codecNames = new ArrayList<>();
    for (RtpCapabilities.CodecCapability codec : capabilities.getCodecs()) {
      codecNames.add(codec.name);
    }

    assertTrue(codecNames.containsAll(Arrays.asList("VP8", "H264")));
  }

  /**
   * Test that getRtpReceiverCapabilities returns the codecs supported by the video decoder factory.
   */
  @Test
  @SmallTest
  public void testGetReceiverCapabilities() {
    RtpCapabilities capabilities =
        factory.getRtpReceiverCapabilities(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO);
    List<String> codecNames = new ArrayList<>();
    for (RtpCapabilities.CodecCapability codec : capabilities.getCodecs()) {
      codecNames.add(codec.name);
    }

    assertTrue(codecNames.containsAll(Arrays.asList("VP8", "H264")));
  }

  /**
   * Test that a created offer reflects the codec capabilities passed into setCodecPreferences.
   */
  @Test
  @SmallTest
  public void testSetCodecPreferences() {
    List<Encoding> encodings = new ArrayList<>();
    encodings.add(new Encoding("1", true, null));

    RtpTransceiverInit init = new RtpTransceiverInit(
        RtpTransceiver.RtpTransceiverDirection.SEND_ONLY, Collections.emptyList(), encodings);
    RtpTransceiver transceiver =
        pc.addTransceiver(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO, init);

    RtpCapabilities capabilities =
        factory.getRtpSenderCapabilities(MediaStreamTrack.MediaType.MEDIA_TYPE_VIDEO);
    RtpCapabilities.CodecCapability targetCodec = null;
    for (RtpCapabilities.CodecCapability codec : capabilities.getCodecs()) {
      if (codec.name.equals("VP8")) {
        targetCodec = codec;
        break;
      }
    }

    assertNotNull(targetCodec);

    transceiver.setCodecPreferences(Arrays.asList(targetCodec));

    SdpObserverLatch sdpLatch = new SdpObserverLatch();
    pc.createOffer(sdpLatch, new MediaConstraints());
    assertTrue(sdpLatch.await());

    SessionDescription offerSdp = sdpLatch.getSdp();
    assertNotNull(offerSdp);

    String[] sdpDescriptionLines = offerSdp.description.split("\r\n");
    List<String> mediaDescriptions = getMediaDescriptions(sdpDescriptionLines);
    assertEquals(1, mediaDescriptions.size());

    List<String> payloads = getMediaPayloads(mediaDescriptions.get(0));
    assertEquals(1, payloads.size());

    String targetPayload = payloads.get(0);
    List<String> rtpMaps = getRtpMaps(sdpDescriptionLines);
    assertEquals(1, rtpMaps.size());

    String rtpMap = rtpMaps.get(0);

    String expected =
        "a=rtpmap:" + targetPayload + " " + targetCodec.name + "/" + targetCodec.getClockRate();
    assertEquals(expected, rtpMap);
  }

  private List<String> getMediaDescriptions(String[] sdpDescriptionLines) {
    List<String> mediaDescriptions = new ArrayList<>();
    for (String line : sdpDescriptionLines) {
      if (line.charAt(0) == 'm') {
        mediaDescriptions.add(line);
      }
    }

    return mediaDescriptions;
  }

  private List<String> getMediaPayloads(String mediaDescription) {
    String[] fields = mediaDescription.split(" ");
    assertTrue(fields.length >= 4);

    // Media formats are described from the fourth field onwards.
    return Arrays.asList(Arrays.copyOfRange(fields, 3, fields.length));
  }

  private List<String> getRtpMaps(String[] sdpDescriptionLines) {
    List<String> rtpMaps = new ArrayList<>();
    for (String line : sdpDescriptionLines) {
      if (line.startsWith("a=rtpmap")) {
        rtpMaps.add(line);
      }
    }

    return rtpMaps;
  }
}
