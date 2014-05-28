if (window.testRunner) {
    testRunner.waitUntilDone();
    testRunner.dumpAsText();
}

function resizeTest(sizes, testfn) {
    if (!sizes.length) {
        if (window.testRunner)
            testRunner.notifyDone();
        return;
    }

    var size = sizes.shift();
    var width = size[0];
    var height = size[1];
    window.resizeTo(width, height);
    window.setTimeout(function () {
        shouldBe("window.outerWidth", "" + width);
        shouldBe("window.outerHeight", "" + height);
        testfn(size);
        resizeTest(sizes, testfn);
    }, 0);
}

function standardResizeTest(testfn) {
    resizeTest([[800, 600], [900, 600], [900, 640], [500, 640], [800, 600]], testfn);
}
