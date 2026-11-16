// Shared code for the edge-sampling tests. The tests are served from 127.0.0.1 and load their frames
// from localhost, so the frames are cross-site.

jsTestIsAsync = true;

var edgeSamplingColors;
var edgeSamplingFramesLoaded = false;

// Sets src from script, so the load listeners are attached first.
async function loadEdgeSamplingFrames()
{
    await Promise.all(Array.from(document.querySelectorAll("iframe[data-src]")).map(frame => {
        return new Promise(resolve => {
            frame.addEventListener("load", resolve, { once: true });
            frame.src = frame.dataset.src;
        });
    }));
    edgeSamplingFramesLoaded = true;
}

function waitForEdgeSamplingFramesToLoad()
{
    loadEdgeSamplingFrames();
    return shouldBecomeEqual("edgeSamplingFramesLoaded", "true");
}

// Posts `message` to `frame` and resolves with the frame's reply.
function messageEdgeSamplingFrame(frame, message)
{
    return new Promise(resolve => {
        addEventListener("message", event => resolve(event.data), { once: true });
        frame.contentWindow.postMessage(message, "*");
    });
}

// Polls until the sampled edges match `expected`. Getting a cross-site frame's color takes an IPC round trip
// whose timing isn't tied to how many presentation updates have happened, so there is no limit here; the test
// times out if the edges never settle.
async function settledEdges(expected)
{
    const sides = Object.keys(expected);
    let sampled;
    do {
        await UIHelper.ensurePresentationUpdate();
        sampled = await UIHelper.fixedContainerEdgeColors();
    } while (!sampled || !sides.every(side => sampled[side] === expected[side]));
    return sampled;
}

function assertEdges(expected)
{
    for (const side of Object.keys(expected)) {
        if (expected[side] === null)
            shouldBeNull(`edgeSamplingColors.${side}`);
        else
            shouldBeEqualToString(`edgeSamplingColors.${side}`, expected[side]);
    }
}

// Top and bottom only, because on iOS left and right insets narrow the layout viewport. Setting insets on
// iOS also scrolls the page, so the tests set TopContentInsetBackgroundCanChangeAfterScrolling to keep the
// top edge sampled after that scroll.
function setTopAndBottomEdgeInsets()
{
    return UIHelper.setObscuredInsets(120, 0, 120, 0);
}

// Loads the frames before setting the insets, because a result sampled before the frames paint is kept
// while the same container stays at that edge.
async function prepareEdgeSampling()
{
    await waitForEdgeSamplingFramesToLoad();
    await UIHelper.ensurePresentationUpdate();
    await setTopAndBottomEdgeInsets();
}

// `expected` maps each side to check to a color, "multiple", or null for no color.
async function testFixedContainerEdges(expected)
{
    await prepareEdgeSampling();
    edgeSamplingColors = await settledEdges(expected);
    assertEdges(expected);
    finishJSTest();
}
