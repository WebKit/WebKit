// Based on LayoutTests/fast/resources/repaint.js -- adapted to reftests instead of pixel tests (disable visual repaint rect tracking).
// Tests based on runRepaintRefTest() verify that a given change (e.g. transform change) take visual effect (either indirectly because
// of relayouts, or directly because of explicit repaints), against a reftest expectation that contains the same test with the change
// already applied. That's useful in companion with text-based-repaint.js scripts that dump the repaint rects, to fully cover repainting
// in tests, without relying on pixel tests.
function runRepaintRefTest() {
    if (window.testRunner)
        testRunner.waitUntilDone();

    // Force layout, if necessary.
    document.body.offsetHeight;

    // Force repainting
    if (window.testRunner)
        testRunner.display();

    // setTimeouts() are not necessary in DRT/WRT mode, but useful for manual in-browser testing.
    setTimeout(callRepaintRefTest, 0);
}

function callRepaintRefTest() {
    repaintRefTest();
    setTimeout(finishRefTest, 0);
}

function finishRefTest() {
    if (window.testRunner)
        testRunner.notifyDone();
}
