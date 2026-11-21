// META: global=window,dedicatedworker
// META: variant=?av1
// META: variant=?vp8
// META: variant=?vp9_p0
// META: variant=?h264_avc
// META: variant=?h264_annexb

const CODEC = {
  '?av1': {codec: 'av01.0.04M.08'},
  '?vp8': {codec: 'vp8'},
  '?vp9_p0': {codec: 'vp09.00.10.08'},
  '?h264_avc': {codec: 'avc1.42001E', avc: {format: 'avc'}},
  '?h264_annexb': {codec: 'avc1.42001E', avc: {format: 'annexb'}},
}[location.search];

const SIZES = [[64, 48], [128, 128], [48, 64], [32, 32], [128, 128]];
const FRAMES_PER_SIZE = 3;

function createFrame(width, height, timestamp) {
  const lumaSize = width * height;
  const chromaSize = (width / 2) * (height / 2);
  const data = new Uint8Array(lumaSize + 2 * chromaSize);
  data.fill(96, 0, lumaSize);
  data.fill(155, lumaSize, lumaSize + chromaSize);
  data.fill(104, lumaSize + chromaSize);
  return new VideoFrame(
      data, {format: 'I420', timestamp, codedWidth: width, codedHeight: height});
}

async function encodeChunks(width, height, firstTimestamp) {
  const chunks = [];
  let encoderError = null;
  const encoder = new VideoEncoder({
    output: (chunk, metadata) =>
        chunks.push({chunk, decoderConfig: metadata?.decoderConfig}),
    error: e => encoderError = e,
  });
  encoder.configure(
      {...CODEC, width, height, bitrate: 1000000, framerate: 30});
  for (let index = 0; index < FRAMES_PER_SIZE; ++index) {
    const frame = createFrame(width, height, firstTimestamp + index);
    encoder.encode(frame, {keyFrame: !index});
    frame.close();
  }
  await encoder.flush();
  encoder.close();
  assert_equals(
      encoderError?.message ?? null, null,
      `encoder error for ${width}x${height}`);
  return chunks;
}

promise_test(async t => {
  const support =
      await VideoEncoder.isConfigSupported({...CODEC, width: 128, height: 128});
  assert_implements_optional(support.supported, `${CODEC.codec} encoder is unsupported`);

  const chunks = [];
  const expected = [];
  for (const [width, height] of SIZES) {
    const segment = await encodeChunks(width, height, expected.length);
    assert_equals(segment.length, FRAMES_PER_SIZE, `chunk count for ${width}x${height}`);
    assert_equals(segment[0].chunk.type, 'key', `first chunk type for ${width}x${height}`);
    chunks.push(...segment);
    for (const {chunk} of segment)
      expected.push({timestamp: chunk.timestamp, width, height});
  }

  const decoded = [];
  let decoderError = null;
  const decoder = new VideoDecoder({
    output: frame => {
      decoded.push({
        timestamp: frame.timestamp,
        width: frame.codedWidth,
        height: frame.codedHeight
      });
      frame.close();
    },
    error: e => decoderError = e,
  });
  // Only H.264 in avc format needs the new description at a resolution change.
  decoder.configure({codec: CODEC.codec});
  for (const {chunk, decoderConfig} of chunks) {
    if (decoderConfig?.description)
      decoder.configure(decoderConfig);
    decoder.decode(chunk);
  }
  await decoder.flush().catch(() => {});
  if (decoder.state !== 'closed')
    decoder.close();

  assert_equals(decoderError?.message ?? null, null, 'decoder error');
  assert_array_equals(
      decoded.map(frame => frame.timestamp),
      expected.map(frame => frame.timestamp), 'timestamps');
  for (let index = 0; index < expected.length; ++index) {
    assert_equals(decoded[index].width, expected[index].width, `codedWidth of frame ${index}`);
    assert_equals(decoded[index].height, expected[index].height, `codedHeight of frame ${index}`);
  }
}, 'Test decoding across key frames that change the resolution');
