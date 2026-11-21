// META: global=window,dedicatedworker
// META: script=videoDecoder-codec-specific-setup.js
// META: variant=?av1
// META: variant=?vp8
// META: variant=?vp9
// META: variant=?h264_avc
// META: variant=?h264_annexb
// META: variant=?h265_hevc
// META: variant=?h265_annexb

// CHUNKS again, with timestamps following those of CHUNKS.
function secondSegment() {
  return CHUNK_DATA.map((data, i) => new EncodedVideoChunk({
    type: CHUNKS[i].type,
    timestamp: CHUNKS.length + i,
    duration: 1,
    data,
  }));
}

function timestampsRange(start, end) {
  return Array.from({length: end - start}, (_, i) => start + i);
}

function sorted(timestamps) {
  return [...timestamps].sort((a, b) => a - b);
}

promise_test(async t => {
  await checkImplements();
  const timestamps = [];
  const decoder = createVideoDecoder(t, {
    output: frame => {
      timestamps.push(frame.timestamp);
      frame.close();
    },
  });

  decoder.configure(CONFIG);
  for (const chunk of CHUNKS)
    decoder.decode(chunk);
  decoder.configure(CONFIG);
  for (const chunk of secondSegment())
    decoder.decode(chunk);
  await decoder.flush();

  assert_array_equals(sorted(timestamps), timestampsRange(0, 2 * CHUNKS.length), 'output timestamps');
}, 'Test configure() between decode() calls outputs the frames decoded with the previous configuration');

promise_test(async t => {
  await checkImplements();
  let resetCalled = false;
  const timestampsAfterReset = [];
  let resolveFlushed;
  const flushed = new Promise(resolve => resolveFlushed = resolve);
  const decoder = createVideoDecoder(t, {
    output: frame => {
      const timestamp = frame.timestamp;
      frame.close();
      if (resetCalled) {
        timestampsAfterReset.push(timestamp);
        return;
      }
      // The remaining frames of the first chunks are still being output.
      decoder.configure(CONFIG);
      decoder.reset();
      resetCalled = true;
      decoder.configure(CONFIG);
      for (const chunk of secondSegment())
        decoder.decode(chunk);
      resolveFlushed(decoder.flush());
    },
  });

  decoder.configure(CONFIG);
  for (const chunk of CHUNKS)
    decoder.decode(chunk);
  await flushed;

  assert_array_equals(sorted(timestampsAfterReset), timestampsRange(CHUNKS.length, 2 * CHUNKS.length), 'output timestamps after reset()');
}, 'Test reset() while configure() waits for the previous decoder discards its frames');

promise_test(async t => {
  await checkImplements();
  const timestamps = [];
  let resolveError;
  const errored = new Promise(resolve => resolveError = resolve);
  const decoder = createVideoDecoder(t, {
    output: frame => {
      timestamps.push(frame.timestamp);
      frame.close();
    },
    error: e => resolveError(e),
  });

  decoder.configure(CONFIG);
  for (const chunk of CHUNKS)
    decoder.decode(chunk);
  decoder.configure({...CONFIG, codec: 'unsupported'});
  const error = await errored;

  assert_equals(error.name, 'NotSupportedError', 'error name');
  assert_equals(decoder.state, 'closed', 'state');
  assert_array_equals(sorted(timestamps), timestampsRange(0, CHUNKS.length), 'output timestamps');
}, 'Test configure() with an unsupported codec outputs the frames decoded with the previous configuration');
