// META: title=MediaStreamTrack processor and generator tests.
importScripts("/resources/testharness.js");

function makeOffscreenCanvasVideoFrame(width, height) {
    let canvas = new OffscreenCanvas(width, height);
    let ctx = canvas.getContext('2d');
    ctx.fillStyle = 'rgba(50, 100, 150, 255)';
    ctx.fillRect(0, 0, width, height);
    return new VideoFrame(canvas, { timestamp: 1 });
}

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
    assert_equals(settings.width, 0, "width 1");
    assert_equals(settings.height, 0, "height 1");
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

done();
