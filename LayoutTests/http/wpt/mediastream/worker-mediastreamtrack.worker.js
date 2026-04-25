// META: title=MediaStreamTrack processor and generator tests.
importScripts("/resources/testharness.js");

function makeOffscreenCanvasVideoFrame(width, height, timestamp) {
    if (!timestamp)
        timestamp = 1;
    let canvas = new OffscreenCanvas(width, height);
    let ctx = canvas.getContext('2d');
    ctx.fillStyle = 'rgba(50, 100, 150, 255)';
    ctx.fillRect(0, 0, width, height);
    return new VideoFrame(canvas, { timestamp });
}

promise_test(async t => {
    const generator = new VideoTrackGenerator();
    t.add_cleanup(() => generator.track.stop());

    // Use a MediaStreamTrackProcessor as a sink for |generator| to verify
    // that |processor| actually forwards the frames written to its writable
    // field.
    self.processor = new MediaStreamTrackProcessor(generator);
    t.add_cleanup(() => self.processor = undefined);
    const reader = processor.readable.getReader();

    const writer = generator.writable.getWriter();
    const videoFrame1 = makeOffscreenCanvasVideoFrame(100, 100, 1);
    writer.write(videoFrame1);
    const result1 = await reader.read();
    t.add_cleanup(() => result1.value.close());
    assert_equals(result1.value.codedWidth, 100);
    generator.muted = true;

    // This frame is expected to be discarded.
    const videoFrame2 = makeOffscreenCanvasVideoFrame(200, 200, 2);
    writer.write(videoFrame2);
    generator.muted = false;

    const videoFrame3 = makeOffscreenCanvasVideoFrame(300, 300, 3);
    writer.write(videoFrame3);
    const result3 = await reader.read();
    t.add_cleanup(() => result3.value.close());
    assert_equals(result3.value.codedWidth, 300);

    // Set up a read ahead of time, then mute, enqueue and unmute.
    const promise5 = reader.read();
    generator.muted = true;
    writer.write(makeOffscreenCanvasVideoFrame(400, 400, 4)); // Expected to be discarded.
    generator.muted = false;
    writer.write(makeOffscreenCanvasVideoFrame(500, 500, 5));
    const result5 = await promise5;
    t.add_cleanup(() => result5.value.close());
    assert_equals(result5.value.codedWidth, 500);
}, 'Tests that VideoTrackGenerator forwards frames only when unmuted');

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100);
    t.add_cleanup(() => frame1.close());

    const generator = new VideoTrackGenerator();
    const writer = generator.writable.getWriter();
    assert_equals(frame1.codedWidth, 100);
    await writer.write(frame1);
    assert_equals(frame1.codedWidth, 0);
}, "Frames get closed when written in a VideoTrackGenerator writable");

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100);
    frame1.close();

    const generator = new VideoTrackGenerator();
    const writer = generator.writable.getWriter();
    await promise_rejects_js(t, TypeError, writer.write(frame1));

    await promise_rejects_js(t, TypeError, writer.closed);
}, "Writable gets closed when writing a closed VideoFrame");

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100);
    frame1.close();

    const generator = new VideoTrackGenerator();
    const writer = generator.writable.getWriter();
    await promise_rejects_js(t, TypeError, writer.write(generator));

    await promise_rejects_js(t, TypeError, writer.closed);
}, "Writable gets closed when writing something else than a VideoFrame");

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100);
    t.add_cleanup(() => frame1.close());

    const generator = new VideoTrackGenerator();
    const writer = generator.writable.getWriter();
    let settings = generator.track.getSettings();
    let capabilities = generator.track.getCapabilities();
    assert_equals(settings.width, undefined, "width 1");
    assert_equals(settings.height, undefined, "height 1");
    assert_equals(capabilities.width.min, 0, "min width 1");
    assert_equals(capabilities.width.max, 0, "max width 1");
    assert_equals(capabilities.height.min, 0, "min height 1");
    assert_equals(capabilities.height.max, 0, "max height 1");

    await writer.write(frame1);

    let counter = 0;
    while (++counter < 100 && generator.track.getSettings().width !== 100)
        await new Promise(resolve => t.step_timeout(resolve, 50));

    settings = generator.track.getSettings();
    capabilities = generator.track.getCapabilities();
    assert_equals(settings.width, 100, "width 2");
    assert_equals(settings.height, 100, "height 2");
    assert_equals(capabilities.width.min, 0, "min width 2");
    assert_equals(capabilities.width.max, 100, "max width 2");
    assert_equals(capabilities.height.min, 0, "min height 2");
    assert_equals(capabilities.height.max, 100, "max height 2");

    const frame2 = makeOffscreenCanvasVideoFrame(200, 200);
    t.add_cleanup(() => frame2.close());

    await writer.write(frame2);

    counter = 0;
    while (++counter < 100 && generator.track.getSettings().width !== 200)
        await new Promise(resolve => t.step_timeout(resolve, 50));

    settings = generator.track.getSettings();
    capabilities = generator.track.getCapabilities();
    assert_equals(settings.width, 200, "width 3");
    assert_equals(settings.height, 200, "height 3");
    assert_equals(capabilities.width.min, 0, "min width 3");
    assert_equals(capabilities.width.max, 200, "max width 3");
    assert_equals(capabilities.height.min, 0, "min height 3");
    assert_equals(capabilities.height.max, 200, "max height 3");

    const frame3 = makeOffscreenCanvasVideoFrame(100, 200);
    t.add_cleanup(() => frame3.close());

    await writer.write(frame3);

    counter = 0;
    while (++counter < 100 && generator.track.getSettings().width !== 100)
        await new Promise(resolve => t.step_timeout(resolve, 50));

    settings = generator.track.getSettings();
    capabilities = generator.track.getCapabilities();
    assert_equals(settings.width, 100, "width 4");
    assert_equals(settings.height, 200, "height 4");
    assert_equals(capabilities.width.min, 0, "min width 4");
    assert_equals(capabilities.width.max, 200, "max width 4");
    assert_equals(capabilities.height.min, 0, "min height 4");
    assert_equals(capabilities.height.max, 200, "max height 4");
}, "Track settings are updated according enqueued video frames");

promise_test(async (t) => {
    const generator = new VideoTrackGenerator();
    generator.muted = true;
    await new Promise(resolve => generator.track.onmute = resolve);

    generator.muted = false;
    await new Promise(resolve => generator.track.onunmute = resolve);
}, "Track gets muted based on VideoTrackGenerator.muted");

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100, 1);
    t.add_cleanup(() => frame1.close());

    const frame2 = makeOffscreenCanvasVideoFrame(200, 200, 2);
    t.add_cleanup(() => frame2.close());

    const generator = new VideoTrackGenerator();
    const processor = new MediaStreamTrackProcessor({ track : generator.track });

    const writer = generator.writable.getWriter();
    const reader = processor.readable.getReader();

    writer.write(frame1);

    const chunk1 = await reader.read();
    assert_equals(chunk1.value.codedWidth, 100);
    assert_equals(chunk1.value.timestamp, 1);
    t.add_cleanup(() => chunk1.value.close());

    writer.write(frame2);

    const chunk2 = await reader.read();
    assert_equals(chunk2.value.codedWidth, 200);
    assert_equals(chunk2.value.timestamp, 2);
    t.add_cleanup(() => chunk2.value.close());

    const endedPromise = new Promise(resolve => generator.track.onended = resolve);

    writer.close();

    chunk = await reader.read();
    assert_true(chunk.done);

    await endedPromise;
}, "VideoTrackGenerator to MediaStreamTrackProcessor");

promise_test(async (t) => {
    const generator = new VideoTrackGenerator();
    const track = generator.track;

    assert_equals(track.kind, "video");

    for (let hint of ["motion", "detail", "text", ""]) {
        track.contentHint = hint;
        assert_equals(track.contentHint, hint);
    }

    for (let hint of ["speech", "music", "funky"]) {
        track.contentHint = hint;
        assert_equals(track.contentHint, "");
    }
}, "kind and content hint tests");

done();
