// META: global=window,dedicatedworker
// META: script=/webcodecs/video-encoder-utils.js

promise_test(async (t) => {
  const config = {
    codec: 'avc1.42001E',
    avc: {format: 'avc'},
    width: 14,
    height: 14,
    bitrate: 1000000,
    bitrateMode: "constant",
    framerate: 30
  };

  let decoder = new VideoDecoder({
    output(frame) {
      frame.close();
    },
    error(e) {
    }
  });

  const encoder = new VideoEncoder({
    output(chunk, metadata) {
      if (metadata.decoderConfig)
        decoder.configure(metadata.decoderConfig);
      decoder.decode(chunk);
    },
    error(e) {
    }
  });
  encoder.configure(config);

  for (let i = 0; i < 5; i++) {
    let frame = createDottedFrame(config.width, config.height, i);
    let keyframe = (i % 5 == 0);
    encoder.encode(frame, { keyFrame: keyframe });
    frame.close();
  }
  await encoder.flush();
  // FIXME: We should check that flush is fine
  await decoder.flush().then(() => { }, () => { });
}, 'Encoding and decoding H264 small size');

