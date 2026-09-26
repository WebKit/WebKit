// Shared code for the edge-sampling tests. The tests are served from 127.0.0.1 and load their frames
// from localhost, so the frames are cross-site.

jsTestIsAsync = true;

var color;

// Sets src from script, so the load listener is attached first.
function loadEdgeSamplingFrames()
{
    return Promise.all(Array.from(document.querySelectorAll("iframe[data-src]")).map(frame => {
        return new Promise(resolve => {
            frame.addEventListener("load", resolve, { once: true });
            frame.src = frame.dataset.src;
        });
    }));
}

// Polls until the sampled edges match `expected`, or gives up after 2 seconds of wall-clock time. Time-based
// rather than a fixed number of updates, because the 200ms sampling throttle is wall-clock, not update-count.
async function settledEdges(expected)
{
    const maximumMilliseconds = 2000;
    const sides = Object.keys(expected);
    let sampled;

    const deadline = performance.now() + maximumMilliseconds;
    while (performance.now() < deadline) {
        await UIHelper.ensurePresentationUpdate();
        sampled = await UIHelper.fixedContainerEdgeColors();
        if (sampled && sides.every(side => sampled[side] === expected[side]))
            break;
    }

    if (performance.now() >= deadline)
        debug(`Gave up after ${maximumMilliseconds}ms; last sampled ${JSON.stringify(sampled)}`);

    return sampled;
}

function assertEdges(expected)
{
    for (const side of Object.keys(expected)) {
        if (expected[side] === null)
            shouldBeNull(`color.${side}`);
        else
            shouldBeEqualToString(`color.${side}`, expected[side]);
    }
}

// Top and bottom only, because on iOS left and right insets narrow the layout viewport. Setting insets on
// iOS also scrolls the page, so the tests set TopContentInsetBackgroundCanChangeAfterScrolling to keep the
// top edge sampled after that scroll.
function setEdgeSamplingInsets()
{
    return UIHelper.setObscuredInsets(120, 0, 120, 0);
}

// Loads the frames before setting the insets, because a result sampled before the frames paint is kept
// while the same container stays at that edge.
async function prepareEdgeSampling()
{
    await loadEdgeSamplingFrames();
    testPassed("Loaded frames");
    await UIHelper.ensurePresentationUpdate();
    await setEdgeSamplingInsets();
}

// `expected` maps each side to check to a color, "multiple", or null for no color.
async function testFixedContainerEdges(expected)
{
    await prepareEdgeSampling();
    color = await settledEdges(expected);
    assertEdges(expected);
    finishJSTest();
}
