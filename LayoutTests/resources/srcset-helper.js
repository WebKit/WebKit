window.addEventListener("load", function () {
    if (!window.testRunner || !window.sessionStorage)
        return;

    if (!window.targetScaleFactor)
        window.targetScaleFactor = 2;

    var needsBackingScaleFactorChange = window.targetScaleFactor != 1 && !sessionStorage.pageReloaded;

    if (needsBackingScaleFactorChange) {
        testRunner.waitUntilDone();
        testRunner.setBackingScaleFactor(targetScaleFactor, function() {
            // Right now there is a bug that srcset does not properly deal with dynamic changes to the scale factor,
            // so to work around that, we must reload the page to get the new image.
            sessionStorage.pageReloaded = true;
            setTimeout(function() { document.location.reload(true) }, 0);
        });
        return;
    }

    try {
        if (window.runTest)
            runTest();
    } catch (ex) {
        testFailed("Uncaught exception" + ex);
    }

    var didReload = sessionStorage.pageReloaded;
    delete sessionStorage.pageReloaded;

    if (didReload)
        testRunner.notifyDone();
});
