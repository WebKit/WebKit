// Shared driver: runs `frames` rendering updates, calling step(i) before each one.
function nextFrame() { return new Promise(resolve => requestAnimationFrame(() => resolve())); }

async function runStress(frames, step, options = {})
{
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
        if (testRunner.dontForceRepaint)
            testRunner.dontForceRepaint();
    }
    if (window.internals && options.screenFormats)
        internals.setScreenContentsFormatsForTesting(options.screenFormats);
    await nextFrame();
    await nextFrame();
    for (let i = 0; i < frames; ++i) {
        await step(i);
        await nextFrame();
        if (options.renderingUpdate && window.UIHelper)
            await UIHelper.renderingUpdate();
    }
    document.getElementById('result').textContent = 'DONE';
    if (window.testRunner)
        testRunner.notifyDone();
}

// Partial repaint of a small child inside a composited layer.
function poke(element, i)
{
    element.style.backgroundColor = (i % 2) ? 'rgb(0, 128, 0)' : 'rgb(0, 0, 255)';
}
