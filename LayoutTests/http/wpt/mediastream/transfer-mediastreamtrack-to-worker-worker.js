// META: title=MediaStreamTrack processor and generator.
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
    t.add_cleanup(() => frame1.close());

    const generator = new VideoTrackGenerator();
    generator.muted = true;
    await new Promise(resolve => generator.track.onmute = resolve);

    generator.muted = false;
    await new Promise(resolve => generator.track.onunmute = resolve);
}, "Track gets muted based on VideoTrackGenerator.muted");

promise_test(async (t) => {
    const frame1 = makeOffscreenCanvasVideoFrame(100, 100);
    t.add_cleanup(() => frame1.close());

    const frame2 = makeOffscreenCanvasVideoFrame(200, 200);
    t.add_cleanup(() => frame2.close());

    const generator = new VideoTrackGenerator();
    const processor = new MediaStreamTrackProcessor({ track : generator.track });

    const writer = generator.writable.getWriter();
    const reader = processor.readable.getReader();

    writer.write(frame1);

    let chunk = await reader.read();
    assert_equals(chunk.value.codedWidth, 100);

    writer.write(frame2);

    chunk = await reader.read();
    assert_equals(chunk.value.codedWidth, 200);

    writer.close();

    chunk = await reader.read();
    assert_true(chunk.done);
}, "Test frames going from VideoTrackGenerator to MediaStreamTrackProcessor");

done();
