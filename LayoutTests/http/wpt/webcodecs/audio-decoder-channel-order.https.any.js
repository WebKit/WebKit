// META: global=window,dedicatedworker

// The WebCodecs spec does not define the channel order of decoded AudioData
// (https://w3c.github.io/webcodecs/#audio-channel-ordering). This test checks
// that decoding 5.1 audio produces the Web Audio channel order
// (https://webaudio.github.io/web-audio-api/#ChannelOrdering), regardless of
// the codec's own coded channel order.
//
// Each channel of sfx-5.1.adts / sfx-5.1-opus.ogg carries a distinct tone, so
// the speaker position of every output channel can be identified.

const SAMPLE_RATE = 48000;
const AAC_FRAME_SIZE = 1024;
// sfx-5.1-opus.ogg was encoded with 20ms frames.
const OPUS_FRAME_SIZE = 960;

// Web Audio order for 5.1, with the tone used for each speaker.
const EXPECTED_ORDER = ['FL', 'FR', 'FC', 'LFE', 'BL', 'BR'];
const TONES = {FL: 500, FR: 700, FC: 900, LFE: 100, BL: 1100, BR: 1300};

// Returns the speaker label whose tone dominates `samples`, or '?' if none
// clearly does.
function identifyChannel(samples) {
  // Skip encoder priming and analyse 100ms.
  const start = 4800;
  const length = 4800;
  assert_greater_than_equal(samples.length, start + length,
      'enough decoded samples');
  const powers = Object.entries(TONES).map(([label, frequency]) => {
    const w = 2 * Math.PI * frequency / SAMPLE_RATE;
    let re = 0, im = 0;
    for (let i = 0; i < length; i++) {
      re += samples[start + i] * Math.cos(w * i);
      im += samples[start + i] * Math.sin(w * i);
    }
    return {label, power: re * re + im * im};
  }).sort((a, b) => b.power - a.power);
  return powers[0].power > 100 * powers[1].power ? powers[0].label : '?';
}

function identifyChannels(planes) {
  return planes.map(identifyChannel);
}

// Splits an ADTS stream into frames, and derives the equivalent
// AudioSpecificConfig, as an MP4 demuxer would provide in the esds box.
function parseADTS(buffer) {
  const bytes = new Uint8Array(buffer);
  const frames = [];
  let config = null;
  let offset = 0;
  while (offset + 7 <= bytes.length) {
    assert_equals((bytes[offset] << 4) | (bytes[offset + 1] >> 4), 0xfff,
        'ADTS syncword');
    const protectionAbsent = bytes[offset + 1] & 0x1;
    const headerSize = protectionAbsent ? 7 : 9;
    const frameLength = ((bytes[offset + 3] & 0x3) << 11) |
        (bytes[offset + 4] << 3) | (bytes[offset + 5] >> 5);
    if (!config) {
      const objectType = (bytes[offset + 2] >> 6) + 1;
      const frequencyIndex = (bytes[offset + 2] >> 2) & 0xf;
      const channelConfiguration =
          ((bytes[offset + 2] & 0x1) << 2) | (bytes[offset + 3] >> 6);
      config = new Uint8Array([
        (objectType << 3) | (frequencyIndex >> 1),
        ((frequencyIndex & 0x1) << 7) | (channelConfiguration << 3),
      ]);
    }
    frames.push({
      adts: bytes.subarray(offset, offset + frameLength),
      raw: bytes.subarray(offset + headerSize, offset + frameLength),
    });
    offset += frameLength;
  }
  return {frames, config};
}

// Splits an Ogg stream into packets. For Opus, the first packet is the
// OpusHead identification header, the second is OpusTags.
function parseOgg(buffer) {
  const bytes = new Uint8Array(buffer);
  const packets = [];
  let pending = [];
  let offset = 0;
  while (offset + 27 <= bytes.length) {
    assert_equals(String.fromCharCode(...bytes.subarray(offset, offset + 4)),
        'OggS', 'Ogg capture pattern');
    const segmentCount = bytes[offset + 26];
    const segmentTable = bytes.subarray(offset + 27, offset + 27 + segmentCount);
    let dataOffset = offset + 27 + segmentCount;
    for (const segmentSize of segmentTable) {
      pending.push(bytes.subarray(dataOffset, dataOffset + segmentSize));
      dataOffset += segmentSize;
      // A segment shorter than 255 bytes terminates the packet.
      if (segmentSize < 255) {
        const size = pending.reduce((sum, segment) => sum + segment.length, 0);
        const packet = new Uint8Array(size);
        let packetOffset = 0;
        for (const segment of pending) {
          packet.set(segment, packetOffset);
          packetOffset += segment.length;
        }
        packets.push(packet);
        pending = [];
      }
    }
    offset = dataOffset;
  }
  return packets;
}

async function decodeWithWebCodecs(t, config, payloads, frameSize) {
  const support = await AudioDecoder.isConfigSupported(config);
  assert_implements_optional(support.supported,
      `5.1 ${config.codec} decoding is not supported`);

  const outputs = [];
  const decoder = new AudioDecoder({
    output: data => outputs.push(data),
    error: t.unreached_func('decoder error'),
  });
  decoder.configure(config);
  payloads.forEach((payload, i) => {
    decoder.decode(new EncodedAudioChunk({
      type: 'key',
      timestamp: Math.round(i * frameSize * 1e6 / SAMPLE_RATE),
      data: payload,
    }));
  });
  await decoder.flush();
  decoder.close();

  assert_greater_than(outputs.length, 0, 'decoder produced output');
  const numberOfChannels = outputs[0].numberOfChannels;
  assert_equals(numberOfChannels, EXPECTED_ORDER.length, 'numberOfChannels');
  const totalFrames = outputs.reduce((sum, data) => sum + data.numberOfFrames, 0);
  const planes = [];
  for (let c = 0; c < numberOfChannels; c++) {
    const plane = new Float32Array(totalFrames);
    let offset = 0;
    for (const data of outputs) {
      data.copyTo(plane.subarray(offset, offset + data.numberOfFrames),
          {planeIndex: c, format: 'f32-planar'});
      offset += data.numberOfFrames;
    }
    planes.push(plane);
  }
  outputs.forEach(data => data.close());
  return planes;
}

async function fetchBuffer(url) {
  const response = await fetch(url);
  return response.arrayBuffer();
}

promise_test(async t => {
  const {frames, config} = parseADTS(await fetchBuffer('resources/sfx-5.1.adts'));
  const planes = await decodeWithWebCodecs(t, {
    codec: 'mp4a.40.2',
    sampleRate: SAMPLE_RATE,
    numberOfChannels: EXPECTED_ORDER.length,
    description: config,
  }, frames.map(frame => frame.raw), AAC_FRAME_SIZE);
  const order = identifyChannels(planes);
  assert_array_equals(order, EXPECTED_ORDER,
      `AudioDecoder channel order is [${order}]`);
}, 'AudioDecoder outputs 5.1 AAC with AudioSpecificConfig (MP4) in Web Audio channel order');

promise_test(async t => {
  const {frames} = parseADTS(await fetchBuffer('resources/sfx-5.1.adts'));
  const planes = await decodeWithWebCodecs(t, {
    codec: 'mp4a.40.2',
    sampleRate: SAMPLE_RATE,
    numberOfChannels: EXPECTED_ORDER.length,
  }, frames.map(frame => frame.adts), AAC_FRAME_SIZE);
  const order = identifyChannels(planes);
  assert_array_equals(order, EXPECTED_ORDER,
      `AudioDecoder channel order is [${order}]`);
}, 'AudioDecoder outputs 5.1 AAC in ADTS in Web Audio channel order');

promise_test(async t => {
  const [opusHead, opusTags, ...packets] =
      parseOgg(await fetchBuffer('resources/sfx-5.1-opus.ogg'));
  // Channel mapping family 1 uses the Vorbis channel order.
  assert_equals(opusHead[18], 1, 'OpusHead channel mapping family');
  const planes = await decodeWithWebCodecs(t, {
    codec: 'opus',
    sampleRate: SAMPLE_RATE,
    numberOfChannels: EXPECTED_ORDER.length,
    description: opusHead,
  }, packets, OPUS_FRAME_SIZE);
  const order = identifyChannels(planes);
  assert_array_equals(order, EXPECTED_ORDER,
      `AudioDecoder channel order is [${order}]`);
}, 'AudioDecoder outputs 5.1 Opus (channel mapping family 1) in Web Audio channel order');
