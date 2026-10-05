// META: timeout=long
// META: global=window,dedicatedworker
// META: script=/webcodecs/video-encoder-utils.js
// META: variant=?av1
// META: variant=?vp8
// META: variant=?vp9_p0
// META: variant=?h264_avc
// META: variant=?h265_hevc

// Verifies that partial VideoColorSpace information provided when constructing a
// VideoFrame from a BufferSource is preserved through a full encode/decode cycle.

var ENCODER_CONFIG = null;

promise_setup(async () => {
  const configs = {
    '?av1': {
      codec: 'av01.0.04M.08',
      hardwareAcceleration: 'prefer-software',
    },
    '?vp8': {
      codec: 'vp8',
      hardwareAcceleration: 'prefer-software',
    },
    '?vp9_p0': {
      codec: 'vp09.00.10.08',
      hardwareAcceleration: 'prefer-software',
    },
    '?h264_avc': {
      codec: 'avc1.42001E',
      avc: { format: 'avc' },
      hardwareAcceleration: 'prefer-software',
    },
    '?h265_hevc': {
      codec: 'hvc1.1.6.L123.00',
      hevc: { format: 'hevc' },
      hardwareAcceleration: 'prefer-hardware',
    },
  };
  const config = configs[location.search];
  config.width = 320;
  config.height = 200;
  config.bitrate = 1000000;
  config.bitrateMode = 'constant';
  config.framerate = 30;
  ENCODER_CONFIG = config;
});

function createI420Buffer(width, height, seed) {
  const yPlaneSize = width * height;
  const uvPlaneSize = (width / 2) * (height / 2);
  const buffer = new Uint8Array(yPlaneSize + 2 * uvPlaneSize);
  buffer.fill(128, 0, yPlaneSize);
  const barSize = 16;
  const barX = (seed * 20) % Math.max(1, width - barSize);
  const barY = 10;
  for (let y = barY; y < barY + barSize; y++) {
    const rowStart = y * width + barX;
    buffer.fill(0, rowStart, rowStart + barSize);
  }
  // Neutral chroma.
  buffer.fill(128, yPlaneSize);
  return buffer;
}

const partialColorSpaceCases = [
  { name: 'primaries only 1',
    colorSpace: { primaries: 'smpte170m' } },
  { name: 'primaries only 2',
    colorSpace: { primaries: 'bt709' } },
  { name: 'transfer only1',
    colorSpace: { transfer: 'iec61966-2-1' } },
  { name: 'transfer only 2',
    colorSpace: { transfer: 'bt709' } },
  { name: 'matrix only 1',
    colorSpace: { matrix: 'bt470bg' } },
  { name: 'matrix only 2',
    colorSpace: { matrix: 'bt709' } },
  { name: 'primaries and transfer',
    colorSpace: { primaries: 'smpte170m', transfer: 'iec61966-2-1' } },
  { name: 'primaries and matrix',
    colorSpace: { primaries: 'smpte170m', matrix: 'bt470bg' } },
  { name: 'transfer and matrix',
    colorSpace: { transfer: 'iec61966-2-1', matrix: 'bt470bg' } },
  { name: 'all fields set',
    colorSpace: { primaries: 'bt709', transfer: 'bt709', matrix: 'bt709', fullRange: false } },
];

const COLOR_SPACE_FIELDS = ['primaries', 'transfer', 'matrix'];

function snapshotColorSpace(colorSpace) {
  const out = {};
  for (const field of COLOR_SPACE_FIELDS)
    out[field] = colorSpace[field];
  return out;
}

async function runPartialColorSpaceTest(t, options, partialCase) {
  const encoder_config = { ...ENCODER_CONFIG };
  const w = encoder_config.width;
  const h = encoder_config.height;
  const frames_to_encode = 4;
  let frames_encoded = 0;
  let frames_decoded = 0;
  let source_color_space = null;
  let encoder_color_space = null;
  let decoder_configured = false;

  await checkEncoderSupport(t, encoder_config);

  const decoder = new VideoDecoder({
    output(frame) {
      t.add_cleanup(() => { frame.close(); });
      for (const field of COLOR_SPACE_FIELDS) {
        assert_equals(
          frame.colorSpace[field],
          source_color_space[field],
          `decoded frame colorSpace.${field} (${partialCase.name})`);
      }
      frames_decoded++;
    },
    error(e) { assert_unreached(e.message); }
  });

  const encoder = new VideoEncoder({
    output(chunk, metadata) {
      if (metadata.decoderConfig && !decoder_configured) {
        const config = { ...metadata.decoderConfig };
        config.colorSpace = source_color_space;

        decoder.configure(config);
        decoder_configured = true;
      }
      decoder.decode(chunk);
      frames_encoded++;
    },
    error(e) { assert_unreached(e.message); }
  });

  encoder.configure(encoder_config);

  for (let i = 0; i < frames_to_encode; i++) {
    const buffer = createI420Buffer(w, h, i);
    const frame = new VideoFrame(buffer, {
      format: 'I420',
      codedWidth: w,
      codedHeight: h,
      timestamp: i * 33333,
      duration: 33333,
      colorSpace: partialCase.colorSpace,
    });

    if (i === 0) {
      // Verify the source frame itself preserves the partial fields we requested.
      for (const field of COLOR_SPACE_FIELDS) {
        if (partialCase.colorSpace[field] !== undefined) {
          assert_equals(
            frame.colorSpace[field],
            partialCase.colorSpace[field],
            `source VideoFrame colorSpace.${field} (${partialCase.name})`);
        }
      }
      source_color_space = snapshotColorSpace(frame.colorSpace);
    }

    encoder.encode(frame, { keyFrame: i === 0 });
    frame.close();
  }

  await encoder.flush();
  await decoder.flush();
  encoder.close();
  decoder.close();

  assert_equals(frames_encoded, frames_to_encode, 'frames_encoded');
  assert_equals(frames_decoded, frames_encoded, 'frames_decoded');
}

for (const partialCase of partialColorSpaceCases) {
  promise_test(async t => {
    return runPartialColorSpaceTest(t, {}, partialCase);
  }, `Partial color space preserved through decoderConfig - ${partialCase.name}`);
}
