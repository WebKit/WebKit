async function testProfile(codec)
{
    const width = 320;
    const height = 240;

    const config = {
        codec,
        width,
        height,
        latencyMode: "quality",
        avc: { format: "annexb" },
    };

    const support = await VideoEncoder.isConfigSupported(config);
    assert_true(support.supported, `${codec} reported as supported by isConfigSupported`);

    const img = new ImageData(width, height);
    for (let i = 0; i < img.data.length; i += 4) {
        img.data[i + 0] = 127;
        img.data[i + 1] = 127;
        img.data[i + 2] = 127;
        img.data[i + 3] = 255;
    }
    const bitmap = await createImageBitmap(img);

    let framesCount = 0;
    let errorCount = 0;
    const encoder = new VideoEncoder({
        output: () => { ++framesCount; },
        error: () => { ++errorCount; },
    });
    encoder.configure(config);

    for (let i = 0; i < 30; ++i) {
        const frame = new VideoFrame(bitmap, { timestamp: i * 33333 });
        encoder.encode(frame, { keyFrame: i === 0 });
        frame.close();
    }
    bitmap.close();

    const flushSettled = await Promise.race([
        encoder.flush().then(() => "resolved", e => `rejected: ${e && e.message}`),
        new Promise(resolve => setTimeout(() => resolve("timeout"), 5000)),
    ]);

    try { encoder.close(); } catch (e) { }

    assert_equals(framesCount, 30);
    assert_equals(errorCount, 0, "no error callback invoked");
    assert_equals(flushSettled, "resolved", "flush() should resolve");
    assert_greater_than(framesCount, 0, "encoder should emit at least one chunk");
}

promise_test(() => testProfile("avc1.424028"), "H.264 Constrained Baseline 4.0 with latencyMode=quality emits output");
promise_test(() => testProfile("avc1.420028"), "H.264 Baseline 4.0 with latencyMode=quality emits output");
promise_test(() => testProfile("avc1.4D0028"), "H.264 Main 4.0 with latencyMode=quality emits output");
promise_test(() => testProfile("avc1.640028"), "H.264 High 4.0 with latencyMode=quality emits output");
