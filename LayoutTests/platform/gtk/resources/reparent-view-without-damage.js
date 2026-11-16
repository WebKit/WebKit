if (window.testRunner) {
    testRunner.dumpAsText();
    testRunner.waitUntilDone();
    testRunner.dontForceRepaint();
}

function waitForVisibility(state)
{
    if (document.visibilityState === state)
        return Promise.resolve();

    return new Promise(resolve => {
        function changed()
        {
            if (document.visibilityState !== state)
                return;
            document.removeEventListener("visibilitychange", changed);
            resolve();
        }
        document.addEventListener("visibilitychange", changed);
    });
}

function changeViewParent(method)
{
    // The normal UI script API waits for drawing, which cannot finish while detached.
    return new Promise(resolve => testRunner.runUIScriptImmediately(`
        uiController.${method}(function() {
            uiController.uiScriptComplete();
        });
    `, resolve));
}

function reparentViewImmediately()
{
    return new Promise(resolve => testRunner.runUIScriptImmediately(`
        uiController.removeViewFromWindow(function() { });
        uiController.addViewToWindow(function() {
            uiController.uiScriptComplete();
        });
    `, resolve));
}

async function waitForAnimationFrames()
{
    await new Promise(requestAnimationFrame);
    await new Promise(requestAnimationFrame);
}

function takeSnapshot()
{
    return new Promise(resolve => testRunner.takeViewPortSnapshot(resolve));
}

async function displayedPixel()
{
    const dataURL = await takeSnapshot();
    if (!dataURL)
        return null;

    const image = new Image;
    image.src = dataURL;
    await image.decode();

    // Keep measurements detached so observing the view cannot repaint the page.
    const canvas = document.createElement("canvas");
    canvas.width = canvas.height = 1;
    const context = canvas.getContext("2d");
    context.drawImage(image, Math.floor(image.width / 2), Math.floor(image.height / 2), 1, 1, 0, 0, 1, 1);
    return Array.from(context.getImageData(0, 0, 1, 1).data);
}

async function waitFor(condition)
{
    const deadline = performance.now() + 2000;
    do {
        if (await condition())
            return true;
        await new Promise(resolve => setTimeout(resolve, 20));
    } while (performance.now() < deadline);
    return false;
}

function waitForPixel(expected)
{
    return waitFor(async () => (await displayedPixel())?.join() === expected);
}

window.addEventListener("load", async () => {
    if (!window.testRunner)
        return;

    const results = [];
    const redPixel = "255,0,0,255";
    const bluePixel = "0,0,255,255";
    const initialWindowWidth = outerWidth;
    const initialWindowHeight = outerHeight;
    function check(condition, message)
    {
        results.push(`${condition ? "PASS" : "FAIL"}: ${message}`);
    }

    try {
        check(await waitForPixel(redPixel), "The initial view paints red.");

        const hidden = waitForVisibility("hidden");
        await changeViewParent("removeViewFromWindow");
        await hidden;
        check(!await takeSnapshot(), "A detached view has no snapshot.");

        const visible = waitForVisibility("visible");
        await changeViewParent("addViewToWindow");
        await visible;
        await waitForAnimationFrames();
        check(await waitForPixel(redPixel), "Red content returns after reparenting an unchanged view.");

        await reparentViewImmediately();
        await waitForAnimationFrames();
        check(await waitForPixel(redPixel), "Red content returns after reparenting in one UI callback.");

        let repeatedlyRestored = true;
        for (let iteration = 0; iteration < 8; ++iteration) {
            await reparentViewImmediately();
            await waitForAnimationFrames();
            if (!await waitForPixel(redPixel)) {
                repeatedlyRestored = false;
                break;
            }
        }
        check(repeatedlyRestored, "Red content returns after repeated reparenting without damage.");

        let changedContentRestored = true;
        let expectedPixel = redPixel;
        for (let iteration = 0; iteration < 8; ++iteration) {
            document.documentElement.style.backgroundColor = iteration % 2 ? "red" : "blue";
            expectedPixel = iteration % 2 ? redPixel : bluePixel;
            await reparentViewImmediately();
            await waitForAnimationFrames();
            if (!await waitForPixel(expectedPixel)) {
                changedContentRestored = false;
                break;
            }
        }
        check(changedContentRestored, "Changed content paints while reparenting.");

        testRunner.setPageVisibility("hidden");
        await waitForVisibility("hidden");
        testRunner.resetPageVisibility();
        await waitForVisibility("visible");
        await waitForAnimationFrames();
        check(await waitForPixel(expectedPixel), "Content remains visible after hiding and showing the view.");

        const initialWidth = innerWidth;
        const initialHeight = innerHeight;
        await changeViewParent("removeViewFromWindow");
        await waitForVisibility("hidden");
        document.documentElement.style.backgroundColor = "blue";
        testRunner.setViewSize(initialWindowWidth + 200, initialWindowHeight + 200);
        await changeViewParent("addViewToWindow");
        await waitForVisibility("visible");
        await waitForAnimationFrames();
        check(await waitFor(() => innerWidth > initialWidth && innerHeight > initialHeight), "Resizing a detached view reaches the page.");
        check(await waitForPixel(bluePixel), "Changed content returns after reparenting a hidden and resized view.");
    } catch (error) {
        results.push(`FAIL: ${error}`);
    } finally {
        await changeViewParent("addViewToWindow");
        testRunner.resetPageVisibility();
        testRunner.setViewSize(initialWindowWidth, initialWindowHeight);
        // Writing results is itself a repaint, so do it only after all measurements.
        document.body.textContent = results.join("\n");
        testRunner.notifyDone();
    }
});
