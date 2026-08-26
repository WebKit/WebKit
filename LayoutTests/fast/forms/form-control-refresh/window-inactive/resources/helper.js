async function runWindowStateRepaintTest() {
    if (!window.internals || !window.testRunner)
        return;

    testRunner.dumpAsText();
    testRunner.waitUntilDone();

    await UIHelper.ensurePresentationUpdate();
    window.internals.startTrackingRepaints();

    window.testRunner.setWindowIsKey(false);
    await UIHelper.ensurePresentationUpdate();

    const windowInactiveRepaintRects = internals.repaintRectsAsText();
    internals.stopTrackingRepaints();

    window.internals.startTrackingRepaints();
    window.testRunner.setWindowIsKey(true);
    await UIHelper.ensurePresentationUpdate();

    const windowActiveRepaintRects = internals.repaintRectsAsText();
    internals.stopTrackingRepaints();

    const wasSameRepaintRect = windowActiveRepaintRects == windowInactiveRepaintRects;
    const passText = wasSameRepaintRect ? "PASS: " : "FAIL: ";
    const wasSameText = wasSameRepaintRect ? "" : "NOT";
    document.getElementById("result").innerText += passText + "The repaint rects when the window went inactive were " + wasSameText + " the same as the repaint rects when the window went active.\n\nInactive (should be 1):\n" +  windowInactiveRepaintRects + "\nActive (should be 1):\n" + windowActiveRepaintRects;
    testRunner.notifyDone();
}

async function runWindowInactiveFocusTest() {
    if (!window.internals || !window.testRunner)
        return;

    testRunner.waitUntilDone();

    document.getElementById("focusable").focus();
    window.testRunner.setWindowIsKey(false);
    await UIHelper.ensurePresentationUpdate();

    testRunner.notifyDone();
}

async function runWindowActiveFocusTest() {
    if (!window.internals || !window.testRunner)
        return;

    testRunner.waitUntilDone();

    document.getElementById("focusable").focus();
    window.testRunner.setWindowIsKey(false);
    await UIHelper.ensurePresentationUpdate();
    window.testRunner.setWindowIsKey(true);
    await UIHelper.ensurePresentationUpdate();

    testRunner.notifyDone();
}

async function setWindowActiveFocusExpected() {
    if (!window.internals || !window.testRunner)
        return;

    testRunner.waitUntilDone();

    document.getElementById("focusable").focus();
    await UIHelper.ensurePresentationUpdate();

    testRunner.notifyDone();
}
