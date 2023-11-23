async function encoderTest(testConfig)
{
    const width = 200;
    const height = 100;
    const img = new ImageData(width, height);

    for (let r = 0; r < height; r++) {
        for (let c = 0; c < width; c++) {
            const index = (r * width + c) * 4;
            img.data[index + 0] = 127;
            img.data[index + 1] = 127;
            img.data[index + 2] = 127;
            img.data[index + 3] = 255;
        }
    }
    const bitmap = await createImageBitmap(img);

    let framesCount = 0;
    let errorCount = 0;
    const encoder = new VideoEncoder({
      output: (chunk, metadata) => {
          ++framesCount;
      }, error: (err) => {
          ++errorCount;
      }
    })

    const encoderConfig = {
        codec: "avc1.42001f", // Baseline profile (42 00) with level 3.1 (1f)
        width,
        height,
        latencyMode: "realtime",
        avc: { format: "annexb" },
    }
    if (testConfig.framerate)
        encoderConfig.framerate = testConfig.framerate;
    if (testConfig.bitrate)
        encoderConfig.bitrate = testConfig.bitrate;

    encoder.configure(encoderConfig);

    for (let i = 0; i < 20; i++) {
        const frame = new VideoFrame(bitmap, { timestamp: i });
        encoder.encode(frame, {keyFrame: i === 0});
        frame.close();
    }

    bitmap.close();

    await encoder.flush();
    encoder.close();

    assert_greater_than(framesCount, 0, "frames count");
    assert_equals(errorCount, 0, "error count");
}

promise_test(async () => {
    return encoderTest({ });
}, "Realtime encoding without framerate and bitrate");

promise_test(async () => {
    return encoderTest({ frameRate: 10 });
}, "Realtime encoding without bitrate");

promise_test(async () => {
    return encoderTest({ bitrate: 1000 });
}, "Realtime encoding without framerate");

