let source;
let sourceBuffer;
let initSegment;

const TRACK_ID = 1;
const QUEUE_DEPTH_UNLIMITED = -1;

if (window.internals)
    internals.initializeMockMediaSource();

function parseMockSampleText(text) {
    // {PTS({958/1000 = 0.958000}), DTS({958/1000 = 0.958000}), duration({42/1000 = 0.042000}), flags(0), generation(1)}
    // Remove braces.
    text = /^\{(.*)\}$/.exec(text)[1];
    const components = Object.fromEntries(text.split(",")
        .map(x => x.trim())
        .map(x => /^(.*?)\((.*?)\)/.exec(x).slice(1)));
    return {
        pts: parseMockTimestamp(components["PTS"]),
        dts: parseMockTimestamp(components["DTS"]),
        duration: parseMockTimestamp(components["duration"]),
        flags: parseInt(components["flags"]),
        generation: parseInt(components["generation"]),
    }
}
function parseMockTimestamp(text) {
    const [_, numerator, denominator, value] = /^{(\d+)\/(\d+) = (\d+\.?\d*)}$/.exec(text);
    return {
        numerator: parseInt(numerator),
        denominator: parseInt(denominator),
        value: parseFloat(value),
    }
}
function compactMockSampleLines(lines) {
    function samplesAreSimilar(refSample, newSample) {
        if (refSample.flags != newSample.flags)
            return false;
        if (refSample.generation != newSample.generation)
            return false;
        return true;
    }
    function formatTimestamp(time) {
        const digitsBeforePeriod = 2;
        const digitsAfterPeriod = 6;
        return time.toFixed(digitsAfterPeriod).padStart(digitsBeforePeriod + 1 + digitsAfterPeriod);
    }

    const samples = lines.map(parseMockSampleText);
    if (samples.length == 0)
        return [];

    const compactedLines = [];
    let refIx = 0;
    let foundDtsDiscontinuity = false;
    let nextExpectedDts = samples[refIx].dts.value;
    while (refIx < samples.length) {
        const hasDtsDiscont = Math.abs(samples[refIx].dts.value - nextExpectedDts) >= 0.001 ? "*" : " ";

        nextExpectedDts = samples[refIx].dts.value + samples[refIx].duration.value;
        let cursorIx = refIx + 1;
        while (cursorIx < samples.length && samplesAreSimilar(samples[refIx], samples[cursorIx]) && Math.abs(samples[cursorIx].dts.value - nextExpectedDts) < 0.001) {
            nextExpectedDts = samples[cursorIx].dts.value + samples[cursorIx].duration.value;
            cursorIx++;
        }

        const sync = (samples[refIx].flags & 1 ? "SYNC" : "").padStart(4);
        const nonDisp = (samples[refIx].flags & 2 ? "NON-DISP" : "").padStart(8);
        const sampleCount = (cursorIx - refIx).toString().padStart(3);
        const compactedLine = `<span style="white-space: pre">DTS: ${formatTimestamp(samples[refIx].dts.value)} .. ${formatTimestamp(nextExpectedDts)} ${hasDtsDiscont} GEN ${samples[refIx].generation} ${sync} ${nonDisp} ${sampleCount} samples</span>`;
        compactedLines.push(compactedLine);

        refIx = cursorIx;
    }
    return compactedLines;
}

async function showEnqueuedSamples() {
    consoleWrite("Enqueued samples:");
    (await internals.enqueuedSamplesForTrackID(sourceBuffer, TRACK_ID)).forEach(consoleWrite);
}
async function showBufferedSamples() {
    consoleWrite("Buffered samples:");
    (await internals.bufferedSamplesForTrackId(sourceBuffer, TRACK_ID)).forEach(consoleWrite);
}
async function sourceBufferAppend(generation, timeRanges) {
    run(`sourceBuffer.appendBuffer(gops(${generation}, ${JSON.stringify(timeRanges)}))`);
    await waitFor(sourceBuffer, 'updateend');
}
function gops(generation, timeRanges) {
    const samples = [];
    for (let [start, end] of timeRanges) {
        for (let t = start; t < end; t++)
            samples.push(makeASample(t, t, 1, 1, TRACK_ID, t == start ? SAMPLE_FLAG.SYNC : SAMPLE_FLAG.NONE, generation));
    }
    return concatenateSamples(samples);
}
async function sourceBufferAppendGopContinuation(generation, timeRange) {
    run(`sourceBuffer.appendBuffer(gopContinuation(${generation}, ${JSON.stringify(timeRange)}))`);
    await waitFor(sourceBuffer, 'updateend');
}
function gopContinuation(generation, timeRange) {
    const samples = [];
    const [start, end] = timeRange;
    for (let t = start; t < end; t++)
        samples.push(makeASample(t, t, 1, 1, TRACK_ID, SAMPLE_FLAG.NONE, generation));
    return concatenateSamples(samples);
}
function smoothSwitchTest(callback) {
    window.addEventListener('load', async () => {
        findMediaElement();
        source = new MediaSource();
        testExpected('source.readyState', 'closed');
        const sourceOpened = waitFor(source, 'sourceopen');

        const videoSource = document.createElement('source');
        videoSource.type = 'video/mock; codecs=mock';
        videoSource.src = URL.createObjectURL(source);
        video.appendChild(videoSource);

        await sourceOpened;
        run('sourceBuffer = source.addSourceBuffer("video/mock; codecs=mock")');
        initSegment = makeAInit(10, [makeATrack(1, 'mock', TRACK_KIND.VIDEO)]);
        run('sourceBuffer.appendBuffer(initSegment)');
        await waitFor(sourceBuffer, 'updateend');

        await callback();
        endTest();
    });
}